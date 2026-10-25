#!/usr/bin/env python3
"""Classify how a reviewer session ended: finished, cut off by a limit, or lost to an infra failure.

  python3 _budget_exhausted.py <transcript> [--budget 25] [--wall 3600] [--rc N] [--apply GRADE_JSON]

status (owner rule, 2026-10-14) — a limit label needs POSITIVE evidence of the limit:
  finished          Claude: a `result` event that is not a spend-cap stop. Codex: the `tokens used` line.
  budget_exhausted  spend cap: Claude result subtype naming the budget, or total_cost_usd >= 0.98 x budget;
                    Codex `[codex budget] stopped` marker.
                    wall: agent exit status 124/137 (timeout), or — when no status was recorded — a Claude
                    transcript spanning >= wall - 60 s.
  infra_failure     no verdict and no limit evidence (container crash, provider 5xx/429, OOM, daemon down), and any
                    Claude result event with is_error=true that is not a spend-cap stop (provider error, session
                    or usage limit): such a session never reviewed, so its untouched tree must not be scored.
The agent exit status is read from --rc, else from <transcript minus .jsonl>.rc (written by agent_run.sh).

--apply relabels a verdict-derived outcome (CATCH / MISS / REJECT_BUT_WRONG_SITE / BUILD_BROKEN / UNGRADED):
budget_exhausted -> REVIEWER_BUDGET_EXHAUSTED (graded on the tree it left, never scored as MISS/CATCH);
over_cap (output-token cap, from <transcript>.meter.json) -> the verdict-derived label is KEPT: the tree the reviewer
left when the cap stopped it is graded and scored (owner rule 2026-10-15; ended_at_output_cap=true marks it);
infra_failure -> REVIEWER_INFRA_FAILURE (unscored, eligible for re-run). The would-be label is kept in
outcome_if_finished; the classification is stored under "budget". Prints the resulting outcome.
"""
import json, os, re, sys, argparse
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _provider_quota
WALL_RC = {124, 137}
RELABEL = {"REVIEWER_CATCH", "REVIEWER_MISS", "REJECT_BUT_WRONG_SITE", "REVIEWER_BUILD_BROKEN", "UNGRADED"}


def ts(s):
    try:
        return datetime.fromisoformat(s.replace("Z", "+00:00")).timestamp()
    except Exception:
        return None


CODEX_PROVIDER_ERR = re.compile(r"^ERROR: .*(at capacity|retry limit|stream disconnected|rate.?limit|429|5\d\d\b|overloaded|"
                                r"[Ii]nternal server error|[Ss]ervice unavailable|unexpected status)", re.M)


CODEX_CLI_LOG = re.compile(r"^\d{4}-\d\d-\d\dT[\d:.]+Z +(ERROR|WARN|INFO|DEBUG) codex_")   # the CLI's own log lines


def codex_provider_error(raw):
    """the provider error a Codex session ended on, or None. Only a TERMINAL error counts: after the last matching
    ERROR line nothing may follow but more ERROR lines and the CLI's closing `tokens used` / count."""
    ms = list(CODEX_PROVIDER_ERR.finditer(raw))
    if not ms:
        return None
    tail = raw[ms[-1].end():].split("\n")[1:]
    rest = [l.strip() for l in tail if l.strip() and not l.startswith("ERROR: ") and not CODEX_CLI_LOG.match(l)]
    if all(l == "tokens used" or l.replace(",", "").isdigit() for l in rest):
        return ms[-1].group(0)[len("ERROR: "):]
    return None


def classify(path, budget, wall, rc):
    out = {"status": "infra_failure", "reason": "", "result_event": "n", "rc": rc, "span_s": None,
           "duration_s": None, "cost_usd": None, "bytes": 0, "format": "unknown"}
    try:
        raw = open(path, errors="replace").read()
    except OSError:
        out["reason"] = "no_transcript"; return out
    out["bytes"] = len(raw.encode())
    events, t0, t1 = [], None, None
    # a Codex transcript is plain text with a fixed header; JSON the agent printed (e.g. `cat review.json`)
    # must not make it look like a Claude stream-json transcript
    codex = 'OpenAI Codex v' in raw[:4000]
    for line in ([] if codex else raw.splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            e = json.loads(line)
        except Exception:
            continue
        events.append(e)
        t = ts(e["timestamp"]) if isinstance(e.get("timestamp"), str) else None
        if t:
            t0 = t if t0 is None else t0; t1 = t
    if t0 and t1:
        out["span_s"] = round(t1 - t0, 1)
    wall_killed = rc in WALL_RC
    if events:
        out["format"] = "claude-stream-json"
        res = [e for e in events if e.get("type") == "result"]
        if res:
            r = res[-1]; out["result_event"] = "y"
            out["cost_usd"] = r.get("total_cost_usd")
            # a session can emit several result events (it resumes after background tasks); each duration_ms
            # covers only its own segment, so the session duration is the transcript span (fallback: their sum)
            out["result_events"] = len(res)
            dms = [x.get("duration_ms") for x in res if x.get("duration_ms") is not None]
            if dms:
                out["duration_s"] = round(sum(dms) / 1000, 1)
            sub = str(r.get("subtype", ""))
            if "budget" in sub or (budget and (out["cost_usd"] or 0) >= 0.98 * budget):
                out.update(status="budget_exhausted", reason=f"spend_cap({sub or 'cost>=0.98*budget'})")
            elif (d := _provider_quota.decisive_result(res)) and d.get("is_error"):
                # the session ended on a provider error (e.g. 429 "You've hit your session limit"), not a verdict;
                # trailing empty result events from background-subagent resumptions do not undo it
                out.update(status="infra_failure",
                           reason=f"provider_error({d.get('api_error_status')}: {str(d.get('result') or '')[:80]})")
            else:
                out.update(status="finished", reason=sub or "result")
        elif wall_killed:
            out.update(status="budget_exhausted", reason=f"wall(rc={rc})")
        elif rc is None and out["span_s"] is not None and out["span_s"] >= wall - 60:
            out.update(status="budget_exhausted", reason=f"wall(span {out['span_s']}s >= wall-60)")
        else:
            out["reason"] = f"no_result_event(rc={rc}, span={out['span_s']}s, wall={wall:g}s)"
        if out["span_s"] is not None:
            out["duration_s"] = out["span_s"]
    else:
        out["format"] = "codex-text"
        perr = codex_provider_error(raw)
        if perr:
            # the session ENDED on a provider error ("ERROR: Selected model is at capacity"): the CLI still prints
            # `tokens used`, but the reviewer never reviewed, so the untouched tree must not be scored (2026-10-19:
            # 7 of 683 GPT reviews were scored MISS/CATCH this way)
            out.update(status="infra_failure", reason=f"provider_error(codex: {perr[:80]})")
        elif "[codex budget] stopped" in raw:
            out.update(status="budget_exhausted", reason="spend_cap(codex)")
        elif "tokens used" in raw:
            out.update(status="finished", reason="tokens_used", result_event="y")
        elif wall_killed:
            out.update(status="budget_exhausted", reason=f"wall(rc={rc})")
        else:
            out["reason"] = f"no_completion_marker(rc={rc})"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("transcript"); ap.add_argument("--budget", default="25")   # a number, or none (no spend cap)
    ap.add_argument("--wall", type=float, default=3600.0); ap.add_argument("--rc", type=int)
    ap.add_argument("--apply", metavar="GRADE_JSON")
    a = ap.parse_args()
    rc = a.rc
    if rc is None:
        rcf = (a.transcript[:-6] if a.transcript.endswith(".jsonl") else a.transcript) + ".rc"
        try:
            rc = int(open(rcf).read().strip())
        except (OSError, ValueError):
            rc = None
    budget = None if str(a.budget).lower() in ("none", "0", "") else float(a.budget)
    out = classify(a.transcript, budget, a.wall, rc)
    # a session whose FINAL answer is a refusal / provider safeguard block ("API Error: ... safeguards flagged this
    # message", stop_reason=refusal) never produced a verdict: it must not be scored as CATCH/MISS (2 labels were,
    # freetype k3_23010 / k4_29252). Same rule as the label import: reviewer_refused -> REVIEWER_REFUSED.
    try:
        import _refusal
        if _refusal.is_refusal(a.transcript):
            out.update(status="refused", reason="final answer is a refusal / safeguard block")
    except Exception:  # noqa: BLE001
        pass
    # Output-token cap protocol: the call's meter record (written by agent_run.sh beside the transcript) is the
    # evidence. Over the cap -> over_cap (scored on the tree left at the cutoff). Under that protocol WALL is only a hang
    # guard, so a wall kill is an infrastructure failure, not a limit.
    meter = (a.transcript[:-6] if a.transcript.endswith(".jsonl") else a.transcript) + ".meter.json"
    if os.path.exists(meter):
        try:
            m = json.load(open(meter))
        except (OSError, ValueError):
            m = {}
        out["output_tokens"] = m.get("output_tokens"); out["output_cap"] = m.get("cap")
        if m.get("capped"):
            out.update(status="over_cap", reason=f"output_cap({m.get('output_tokens')} >= {m.get('cap')})")
        elif out["status"] == "budget_exhausted" and str(out.get("reason", "")).startswith("wall"):
            out.update(status="infra_failure", reason="hang_guard(" + out["reason"] + ")")
    out["exhausted"] = int(out["status"] in ("budget_exhausted", "over_cap"))
    if a.apply:
        g = json.load(open(a.apply)); prev = g.get("outcome")
        g["budget"] = out
        if out["status"] == "over_cap":
            g["ended_at_output_cap"] = True
        elif prev in RELABEL and out["status"] != "finished":
            g["outcome"] = {"budget_exhausted": "REVIEWER_BUDGET_EXHAUSTED", "refused": "REVIEWER_REFUSED",
                            "over_cap": "REVIEWER_OVER_CAP"}.get(
                out["status"], "REVIEWER_INFRA_FAILURE")
            if out["status"] == "refused": g["reviewer_refused"] = True
            g["outcome_if_finished"] = prev
        json.dump(g, open(a.apply, "w"), indent=2); print(g["outcome"]); return
    print(json.dumps(out))


if __name__ == "__main__":
    main()

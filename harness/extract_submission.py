#!/usr/bin/env python3
"""Extract the final fenced JSON manifest (attack_submission.v1 / patch_submission.v1) from a Role A
stream-json transcript. Usage: extract_submission.py <transcript.jsonl> <out.json>

The manifest is the last ```json ... ``` block in the run's final `result` text; falls back to the last
assistant text if no result line carries it. Writes nothing (exit 1) if no parseable block is found.
"""
import sys, json, re


def main() -> int:
    if len(sys.argv) != 3:
        return 2
    tx, out = sys.argv[1], sys.argv[2]
    result_text, last_assistant = "", ""
    try:
        for line in open(tx):
            line = line.strip()
            if not line:
                continue
            try:
                o = json.loads(line)
            except Exception:
                continue
            if o.get("type") == "result" and isinstance(o.get("result"), str):
                result_text = o["result"]
            elif o.get("type") == "assistant":
                for c in o.get("message", {}).get("content", []):
                    if c.get("type") == "text" and isinstance(c.get("text"), str):
                        last_assistant = c["text"]
    except FileNotFoundError:
        return 1

    for text in (result_text, last_assistant):
        if not text:
            continue
        blocks = re.findall(r"```json\s*(.*?)```", text, re.S)
        for b in reversed(blocks):
            try:
                obj = json.loads(b)
            except Exception:
                continue
            json.dump(obj, open(out, "w"), indent=2)
            print(out)
            return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())

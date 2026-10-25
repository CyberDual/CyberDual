#!/usr/bin/env python3
"""Commit-message filter for reviewer handoffs (git filter-branch --msg-filter): stdin -> stdout.

Drops lines that mark which commits an agent wrote or which commits were replayed, uniformly for every commit
so no commit stands out: AI attribution trailers (Co-Authored-By naming an AI model or vendor, "Generated
with/by ..." footers, robot emoji) and "(cherry picked from commit ...)" trailers. Trailing blank lines left
behind are removed. `--check` exits 1 if the message on stdin contains any such line.
"""
import re
import sys

AI = r'(claude|anthropic|openai|chatgpt|gpt-?\d|codex|copilot|gemini|kimi|moonshot|glm|zhipu|z\.ai|grok|llm|\bai\b)'
PATTERNS = [
    re.compile(r'^\s*[a-z][a-z-]*-by:.*' + AI, re.I),          # Co-Authored-By / Assisted-By / Generated-By / Helped-by ...
    re.compile(r'^\s*[a-z][a-z-]*-by:.*noreply@(anthropic|openai)', re.I),
    re.compile(r'generated (with|by)\b.*' + AI, re.I),
    re.compile('\U0001F916'),
    re.compile(r'^\s*\(cherry picked from commit [0-9a-f]{7,40}\)\s*$', re.I),
]


def flagged(line):
    return any(p.search(line) for p in PATTERNS)


def main():
    text = sys.stdin.read()
    lines = text.split('\n')
    if '--check' in sys.argv:
        return 1 if any(flagged(l) for l in lines) else 0
    kept = [l for l in lines if not flagged(l)]
    while kept and not kept[-1].strip():
        kept.pop()
    sys.stdout.write('\n'.join(kept) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env bash
# tools/check-commit-msg.sh - validate a commit message file.
# One Conventional Commits line: type[(scope)][!]: description. No body.
# Allowed types: feat fix docs style refactor perf test build ci chore revert.
# Merge, fixup!/squash!/amend! and Revert lines are exempt. Pure ASCII.
# Borrowed from Bowcki's _tools/check-commit-msg.sh; rules unchanged.
# Usage: check-commit-msg.sh <file>; exit 0 when valid, 1 with reason if not.
set -uo pipefail

file=${1:?usage: check-commit-msg.sh <file>}
lines=()
while IFS= read -r line || [[ -n $line ]]; do
    case $line in '#'*) continue ;; esac
    lines+=("${line%$'\r'}")
done <"$file"
while ((${#lines[@]} > 0)) && [[ -z ${lines[-1]//[[:space:]]/} ]]; do
    unset 'lines[-1]'
done
while ((${#lines[@]} > 0)) && [[ -z ${lines[0]//[[:space:]]/} ]]; do
    lines=("${lines[@]:1}")
done
if ((${#lines[@]} == 0)); then
    echo "commit message is empty"
    exit 1
fi
subject=${lines[0]}
case $subject in
"Merge "* | "fixup! "* | "squash! "* | "amend! "* | 'Revert "'* | "Revert "*) exit 0 ;;
esac
if ((${#lines[@]} > 1)); then
    echo "commit message must be one line, no body: $subject"
    exit 1
fi
if printf '%s' "$subject" | LC_ALL=C grep -q '[^ -~]'; then
    echo "commit message must be pure ASCII: $subject"
    exit 1
fi
if ((${#subject} > 72)); then
    echo "commit message over 72 columns: $subject"
    exit 1
fi
if ! [[ $subject =~ ^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([A-Za-z0-9_.-]+\))?(!)?:\ [^[:space:]] ]]; then
    echo "commit message must match 'type[(scope)][!]: description' with a valid type: $subject"
    exit 1
fi
exit 0

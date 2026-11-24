#!/usr/bin/env bash
# tools/check-text-hygiene.sh - text file hygiene: every text file ends
# with a newline and carries no trailing whitespace, docs keep their
# line widths (80 soft, reported; 120 hard, fails).
#
# Borrowed from Bowcki's _tools/check-text-hygiene.sh, adjusted: the C#
# rules stay with the cs skill (the client predates them), table rows
# are exempt from both width limits (a row cannot wrap), _ref/ is the
# read-only reference (the pre-commit guard owns it, hygiene skips it),
# and the text globs match this tree (C, asm, Python, shell, docs).
# Run directly it checks the given files, or every tracked file when
# given none; that is how .githooks/pre-commit checks the staged files.
# Functions only, so no side effects when sourced. Pure ASCII.

# Globs that make a file a text file here: the extensions this tree
# edits as text, plus the extensionless files that are text (Makefiles,
# hooks, config). Windows batch is CRLF by design and data files have
# meaningful bytes (tsv game text, .bin/.sha goldens, _ref/orig-bin),
# so those stay out.
text_file_globs() {
    printf '%s\n' \
        '*.c' '*.h' '*.asm' '*.inc' '*.py' '*.md' '*.sh' '*.txt' \
        '*.json' '*.yml' '*.yaml' '*.cfg' '*.conf' \
        'Makefile' 'Makefile.*' 'Dockerfile' '.gitignore' \
        'LICENSE*' 'CONTRIBUTING*' 'AGENTS*' '.githooks/*'
}

is_text_file() {
    local file="$1" glob
    while IFS= read -r glob; do
        case "$file" in
            $glob) return 0 ;;
        esac
    done < <(text_file_globs)
    return 1
}

# Markdown line widths: 80 is advice and only reported, 120 fails. Fenced
# code is exempt from the soft limit, since a path or a command does not wrap,
# but not from the hard one - past 120 columns it is a wall of text. Table
# rows are exempt from both: a row is one line by construction.
check_doc_width() {
    local file="$1" report
    case "$file" in
        *.md) ;;
        *) return 0 ;;
    esac
    report=$(awk '
        /^[[:space:]]*```/ { fence = !fence; next }
        /^[[:space:]]*\|/ { next }
        !fence && length > 80 { soft = soft NR "," }
        length > 120 { hard = hard NR "," }
        END { if (soft != "") print "soft " soft; if (hard != "") print "hard " hard }
    ' "$file")
    local fail=0 kind lines
    while read -r kind lines; do
        [ -n "$kind" ] || continue
        lines="${lines%,}"
        if [ "$kind" = soft ]; then
            echo "  $file: over 80 columns (soft) on line(s) $lines"
        else
            echo "  $file: over 120 columns on line(s) $lines"
            fail=1
        fi
    done <<< "$report"
    return $fail
}

# Echoes one line per problem, returns 1 when the file has a hard one.
check_text_file() {
    local file="$1" lines fail=0
    [ -f "$file" ] || return 0
    if [ -n "$(tail -c 1 -- "$file")" ]; then
        echo "  $file: no newline at end of file"
        fail=1
    fi
    lines=$(grep -nE '[[:space:]]+$' -- "$file" | cut -d: -f1 | paste -sd, -)
    if [ -n "$lines" ]; then
        echo "  $file: trailing whitespace on line(s) $lines"
        fail=1
    fi
    check_doc_width "$file" || fail=1
    return $fail
}

# Checks the given files, ignoring build outputs, the read-only
# reference, whitespace-semantic fixtures and non-text ones.
check_text_files() {
    local file fail=0
    for file in "$@"; do
        case "$file" in
            build/* | _ref/*) continue ;;
        esac
        # tests/*/script.txt: a lone space is a no-op step for the
        # matrix runners (tool_combat counts ' ' lines), so the
        # whitespace rule must not touch it.
        case "$file" in
            tests/*/script.txt) continue ;;
        esac
        is_text_file "$file" || continue
        check_text_file "$file" || fail=1
    done
    [ "$fail" -eq 0 ]
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    set -uo pipefail
    cd "$(git rev-parse --show-toplevel)" || exit 1
    if [ "$#" -gt 0 ]; then
        check_text_files "$@"
    else
        check_text_files $(git ls-files)
    fi
fi

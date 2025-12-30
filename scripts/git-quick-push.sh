#!/bin/zsh

# Fail fast on errors or unset vars
set -euo pipefail

usage() {
    echo "Usage: ${0##*/} \"commit message\"" >&2
    exit 1
}

[[ $# -ge 1 ]] || usage

# Resolve repo root relative to this script
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

echo "→ Staging all changes"
git add -A

echo "→ Committing: $1"
git commit -m "$1"

echo "→ Pushing to origin/main"
git push

echo "✅ Done"


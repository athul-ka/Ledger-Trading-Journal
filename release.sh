#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <version-tag> <commit-message>"
  echo "Example: $0 v1.0.1 \"Fix: improve trade entry validation\""
  exit 1
fi

TAG="$1"
shift
COMMIT_MSG="$*"

if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Error: tag must follow semantic versioning like v1.0.1"
  exit 1
fi

# Ensure we're in a git repository
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "Error: not inside a git repository"
  exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "$CURRENT_BRANCH" == "HEAD" ]]; then
  echo "Error: detached HEAD state. Checkout a branch before releasing."
  exit 1
fi

# Ensure there is something to commit
if [[ -z "$(git status --porcelain)" ]]; then
  echo "Error: no changes to commit"
  exit 1
fi

# Prevent accidental overwrite of existing tags
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  echo "Error: local tag '$TAG' already exists"
  exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
  echo "Error: remote tag '$TAG' already exists on origin"
  exit 1
fi

echo "Releasing on branch: $CURRENT_BRANCH"
echo "Commit message: $COMMIT_MSG"
echo "Tag: $TAG"

git add .
git commit -m "$COMMIT_MSG"
git tag "$TAG"
git push origin "$CURRENT_BRANCH"
git push origin "$TAG"

echo "Release complete: pushed commit and tag $TAG"

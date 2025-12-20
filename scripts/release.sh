#!/bin/bash
set -e

# Usage: ./scripts/release.sh [patch|minor|major]
#
# This script automates the git-flow release process:
# 1. Computes next version from latest tag
# 2. Prompts for confirmation
# 3. Creates git-flow release branch
# 4. Bumps VERSION file
# 5. Finishes release (merges to main and develop)
# 6. Pushes everything including tags
#
# After push, GitHub Actions will build and publish the release.

TYPE="${1:-patch}"

# Check prerequisites
if ! git flow version &>/dev/null; then
    echo "Error: git-flow is not installed"
    echo "Install with: brew install git-flow-avh"
    exit 1
fi

if ! git config --get gitflow.branch.master &>/dev/null; then
    echo "Error: git-flow not initialized in this repository"
    echo "Run: git flow init"
    exit 1
fi

if [[ ! "$TYPE" =~ ^(patch|minor|major)$ ]]; then
    echo "Usage: $0 [patch|minor|major]"
    echo ""
    echo "  patch  - Bug fixes, minor changes (v1.0.0 → v1.0.1)"
    echo "  minor  - New features, backwards compatible (v1.0.1 → v1.1.0)"
    echo "  major  - Breaking changes (v1.1.0 → v2.0.0)"
    exit 1
fi

# Get latest tag
LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
echo "Current version: $LATEST_TAG"

# Parse version components
VERSION="${LATEST_TAG#v}"
IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"

# Compute next version
case "$TYPE" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
esac

NEXT_VERSION="v${MAJOR}.${MINOR}.${PATCH}"
echo "Next version: $NEXT_VERSION"
echo ""

# Confirm
read -p "Proceed with release $NEXT_VERSION? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

# Ensure we're on develop
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "develop" ]; then
    echo "Error: Must be on develop branch (currently on $CURRENT_BRANCH)"
    exit 1
fi

# Ensure develop is up to date with remote
echo "Checking remote sync..."
git fetch origin develop --quiet
LOCAL=$(git rev-parse develop)
REMOTE=$(git rev-parse origin/develop)
if [ "$LOCAL" != "$REMOTE" ]; then
    echo "Error: Local develop is not in sync with origin/develop"
    echo "  Local:  $LOCAL"
    echo "  Remote: $REMOTE"
    echo "Please pull or push as needed."
    exit 1
fi

# Ensure working directory is clean
if ! git diff-index --quiet HEAD --; then
    echo "Error: Working directory has uncommitted changes"
    exit 1
fi

# Start release
echo ""
echo "Starting release $NEXT_VERSION..."
git flow release start "$NEXT_VERSION"

# Update VERSION file
echo "$NEXT_VERSION" > VERSION
git add VERSION
git commit -m "Bump version to $NEXT_VERSION"

# Finish release (merges to main and develop, creates tag)
# Use GIT_MERGE_AUTOEDIT=no to avoid editor prompts
export GIT_MERGE_AUTOEDIT=no
git flow release finish -m "$NEXT_VERSION" "$NEXT_VERSION"

# Push everything
echo ""
echo "Pushing to origin..."
git push origin main develop --tags

echo ""
echo "Release $NEXT_VERSION complete!"
echo "GitHub Actions will now build and publish the release."
echo ""
echo "Monitor the build at:"
echo "  https://github.com/lessui-hq/LessUI/actions"

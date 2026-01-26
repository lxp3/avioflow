# Release Tag Guide

This document explains how to use different tag formats to control what gets built and published.

## Tag Formats

### 1. **Full Release** - `v0.1.5`
Builds and publishes **everything**:
- ✅ C++ binaries (Linux + Windows)
- ✅ Python wheels (Linux + Windows)
- ✅ npm package (Node.js addon)

**Usage:**
```bash
git tag v0.1.5
git push origin v0.1.5
```

### 2. **C++ Only** - `v0.1.5-app`
Builds and publishes **C++ binaries only**:
- ✅ C++ binaries (Linux + Windows)
- ❌ Python wheels
- ❌ npm package

**Usage:**
```bash
git tag v0.1.5-app
git push origin v0.1.5-app
```

### 3. **Python Only** - `v0.1.5-wheels`
Builds and publishes **Python wheels only**:
- ❌ C++ binaries
- ✅ Python wheels (Linux + Windows)
- ❌ npm package

**Usage:**
```bash
git tag v0.1.5-wheels
git push origin v0.1.5-wheels
```

### 4. **npm Only** - `v0.1.5-npm`
Builds and publishes **npm package only**:
- ❌ C++ binaries
- ❌ Python wheels
- ✅ npm package (Node.js addon)

**Usage:**
```bash
git tag v0.1.5-npm
git push origin v0.1.5-npm
```

## Workflow

### Example: Release npm package only

```bash
# 1. Update version in package.json
npm version patch  # or minor, major

# 2. Commit changes
git add package.json
git commit -m "chore: bump version to 0.1.5"

# 3. Create and push tag for npm only
git tag v0.1.5-npm
git push origin main
git push origin v0.1.5-npm

# 4. GitHub Actions will:
#    - Build Linux and Windows Node.js addons
#    - Create prebuilds/linux-x64/ and prebuilds/win32-x64/
#    - Publish to npm registry
```

### Example: Full release

```bash
# 1. Update versions
npm version patch
# Also update version in pyproject.toml if needed

# 2. Commit changes
git add package.json pyproject.toml
git commit -m "chore: release v0.1.5"

# 3. Create and push tag for full release
git tag v0.1.5
git push origin main
git push origin v0.1.5

# 4. GitHub Actions will:
#    - Build C++ binaries (Linux + Windows)
#    - Build Python wheels (Linux + Windows)
#    - Build Node.js addons (Linux + Windows)
#    - Publish to GitHub Releases
#    - Publish to PyPI
#    - Publish to npm
```

## Build Matrix

| Tag Format | C++ Binaries | Python Wheels | npm Package |
|------------|--------------|---------------|-------------|
| `v0.1.5` | ✅ | ✅ | ✅ |
| `v0.1.5-app` | ✅ | ❌ | ❌ |
| `v0.1.5-wheels` | ❌ | ✅ | ❌ |
| `v0.1.5-npm` | ❌ | ❌ | ✅ |

## Notes

- **Version format**: Must follow semantic versioning: `v{major}.{minor}.{patch}`
- **Tag validation**: Invalid tag formats will cause the workflow to fail early
- **Parallel builds**: Linux and Windows builds run in parallel for faster CI
- **Conditional publishing**: Only the selected artifacts will be published
- **Artifact retention**: All build artifacts are uploaded to GitHub Actions for 90 days

## Troubleshooting

### Check workflow status
```bash
# View recent workflow runs
gh run list --workflow=release.yml

# View specific run details
gh run view <run-id>
```

### Delete a tag (if needed)
```bash
# Delete local tag
git tag -d v0.1.5-npm

# Delete remote tag
git push origin :refs/tags/v0.1.5-npm
```

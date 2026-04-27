# Repository Rules

## Version Updates

Use `version.txt` as the single source of truth for the project version.

When updating the version:

1. Edit `version.txt` to the target semantic version, for example `0.2.6`.
2. Run `python3 scripts/sync_version.py` from the repository root.
3. Review and commit every metadata file changed by the script.

Do not manually update version fields in package metadata when they can be synchronized by `scripts/sync_version.py`. If a new versioned metadata file is added, update `scripts/sync_version.py` so it is derived from `version.txt`.

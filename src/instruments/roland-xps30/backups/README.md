# Backups

Back up before changing system software, expansions, favorites, performances, or
sample assignments.

## Naming

Use dated, descriptive names:

```text
YYYY-MM-DD_xps30_before-<change>
YYYY-MM-DD_xps30_after-<change>
```

Examples:

```text
2026-07-16_xps30_before-first-expansion-test
2026-07-16_xps30_after-worship-favorites-bank-0
```

## Backup Log

| Date | Filename | Reason | System Version | Expansion | Verified Restore? |
| --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |

## Policy

- Keep at least one known-good backup before experiments.
- Do not overwrite raw backups.
- If binary backups become large, store the binary outside normal Git history and
  commit the manifest here.
- Test restore procedures before relying on the setup live.

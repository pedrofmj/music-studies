# Airstar Stable Rig Baseline

Status: **complete**

Capture: fixture at 2026-08-19T04:50:19-03:00

The observer is read-only: it creates no remote files, changes no graph
connections, and controls no services.

## Compatibility

| Check | Expected | Observed | Result |
| --- | ---: | ---: | --- |
| Project SHA-256 | protected | 48ea4f777c68ebd39105902739258795edfa96fefd8820b62fb31d4efe329974 | PASS |
| Plugins | 49 | 49 | PASS |
| Unique plugin names | 49 | 49 | PASS |
| Project connections | 111 | 111 | PASS |
| Raw graph links | 118 | 118 | PASS |
| MIDI graph links | 68 | 68 | PASS |
| Quantum | 1024 | 1024 | PASS |
| Sample rate | available | 48000 | PASS |
| Stable services | active | 4 | PASS |

## Performance Snapshot

| Metric | Observed |
| --- | ---: |
| PipeWire ERR total | 0 |
| Relevant process CPU | 0.8999999999999999% |
| Relevant process RSS | 199266304 bytes |
| Stable service memory | 443371520 bytes |

## Services

| Service | Enabled | Active | PID | Restarts |
| --- | --- | --- | ---: | ---: |
| arturia-main-volume-encoder.service | enabled | active/running | 807766 | 0 |
| smk25-pad-layers.service | enabled | active/running | 3405 | 0 |
| pipewire-carla-quantum.service | enabled | active/exited | 0 | 0 |
| pipewire-patchbay-watch.service | enabled | active/running | 1289652 | 0 |

# Linux Shadow Service Contract

`music-rigd.service` is an uninstalled systemd user-unit contract for the
output-suppressed host. The repository has no command that copies, enables, or
starts it. Its condition also keeps the unit inactive unless the operator has
separately created `$XDG_CONFIG_HOME/music-rig/shadow-enabled` (or the standard
per-user configuration equivalent represented by systemd `%E`).

Both lifecycle commands are explicit and read-only. `resolve-paths
--check-only` validates and reports XDG locations without creating them.
`run-shadow --output-suppressed` resolves the same paths, emits rate-limited
structured diagnostics on stderr for journald, waits for `SIGINT` or `SIGTERM`,
and exits cleanly. It does not read a definition, open control/MIDI/audio/graph
endpoints, load plugins, publish output, or write state.

The unit expects a separately staged `%h/.local/bin/music-rigd`. Installing or
activating this contract is deliberately deferred to an explicit promotion
step after the later shadow-mode adapter work and live-session approval.

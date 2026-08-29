# Device MIDI Output Host

`runtime/linux/music_rig_jack_midi_output.c` is the explicit Linux JACK host
for output-enabled Device/MIDI behavior. It is separate from the input-only
shadow host and is not installed or selected by the default daemon.

## Initialization Order

The caller owns all storage and initializes the host before initializing the
portable Device/MIDI engine:

1. Build a validated generation slot and compiled table image.
2. Initialize the output host from that slot and table image.
3. Obtain its Device/MIDI observer with
   `music_rig_jack_midi_output_observer_init`.
4. Initialize `music_rig_device_midi_shadow` with that observer and
   `MUSIC_RIG_OUTPUT_ENABLED`.
5. Attach the initialized engine to the host.
6. Start the host and register paired stable input/output ports.
7. Build the runtime output-adoption adapter and initialize the runtime.

The runtime must be initialized only after the host is active because the
adapter's initial confirmation requires an active backend.

## Real-Time Boundary

The JACK process callback adopts the newest generation at the beginning of each
cycle, obtains one output buffer per slot, clears those buffers, processes the
fixed input ports, and reserves emitted MIDI directly in the corresponding
output buffer. The callback performs no allocation, locking, filesystem I/O,
JSON traversal, discovery, or graph connection.

Output reserve failures are recorded and make the process-cycle result an
adapter failure. Port registration, lifecycle, generation validation, and
output-adoption callbacks remain control-thread operations.

## Safety Boundary

The host registers stable `device.<slot>.midi-input` and
`device.<slot>.midi-output` ports but never connects or disconnects them. The
default daemon, input-only shadow command, and protected production deployment
remain unchanged. Any future live use requires an explicit command, an
independently supplied definition fingerprint, and an authorized rehearsal.

The explicit daemon command is available only in a JSON-enabled Linux JACK
build:

```text
music-rigd run-midi-output --definition PATH \
  --expected-fingerprint SHA256 --output-enabled --acknowledge-output
```

The control-server variant uses the same explicit boundary while routing
status, switch, and reset requests through the runtime transaction API:

```text
music-rigd run-output --definition PATH \
  --expected-fingerprint SHA256 --acknowledge-output
```

It also accepts the optional `--prepared-definition PATH
--prepared-fingerprint SHA256` pair before `--acknowledge-output`.

The acknowledgement is required because this command can emit MIDI. It still
does not create or remove graph links; external routing remains a separate,
authorized operation.

When JACK signals backend shutdown, the control thread may call
`music_rig_jack_midi_output_reconnect`. The host closes the dead client, clears
owned port state, and reopens the same paired stable ports. Reconnect refuses
to run without a shutdown signal and closes the replacement client if
activation or registration fails.

The explicit daemon uses the timed Linux lifecycle poll hook to detect that
shutdown and perform one reconnect transaction outside the JACK process
callback. A reconnect failure terminates the command after cleanup rather than
continuing with an uncertain backend.

Each JACK processing cycle records the generation adopted by the Device/MIDI
engine and a JACK-derived nanosecond timestamp in lock-free atomic fields. The
runtime adoption query reads those fields without waiting or entering the JACK
callback, allowing IPC status to expose `adopted_at_ns` after the first cycle.

The `run-output` control-server mode performs the same check from its periodic
control poll. Reconnect completes before the next authenticated request is
handled, so status, switch, and reset operations cannot use a dead backend.

The fake-JACK offline test proves paired registration, generated Arturia MIDI
emission, output metrics, adoption prepare/confirm/rollback callbacks, backend
shutdown, and registration-failure cleanup. The fake-JACK output-runtime
process test additionally proves authenticated status, global switch,
switch-back, persistence, and clean daemon shutdown through the control socket.

# Preset Lifecycle And Device Binding

Sources: [EasyEffects user manual](https://wwmm.github.io/easyeffects/) and
[Creating and Importing User Presets](https://wwmm.github.io/easyeffects/user_interface/userpresets.html).

EasyEffects applies effects to PipeWire-managed application output or input.
A preset represents an effects-chain configuration, not a portable guarantee
that an effect, plugin dependency, device, or latency setting is available on
another computer.

## Safe Preset Workflow

1. Start with a bypassed or empty chain and confirm the selected PipeWire
   device and audio direction.
2. Add one effect at a time, checking gain and bypass after each change.
3. Save the working chain under an identifiable preset name.
4. Associate an autoload profile only after the preset has been auditioned on
   that exact input or output device.
5. Record the preset name, device, PipeWire version, chain order, and bypass
   result in the applicable checklist.

The official manual allows presets to be imported and reused. It also supports
autoload profiles that associate a preset with a selected input or output
device. These bindings are host- and device-specific; do not assume that a
preset made for headphones is safe for speakers, a microphone, or a live
instrument path.

## Recovery Rule

Keep a neutral or bypassed route available while creating a chain. Delete or
replace an autoload binding only after confirming that normal unprocessed audio
can be restored. Do not place EasyEffects in an untested live keyboard signal
path immediately before a performance.

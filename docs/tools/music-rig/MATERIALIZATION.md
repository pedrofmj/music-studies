# Temporary Materialization Contract

`materialize-compiled-rig.py` renders the compiled `full-live-rack` Carla
project and Patchbay deployment snapshot into an isolated authoring bundle. It
does not install the files, activate a project, connect a graph, contact a
service, or write persistent state.

## Inputs

The command requires:

- a compiled `music-studies/compiled-performance-rig/v1` definition;
- the authored Rig root and repository root used to resolve its fingerprinted
  `airstar-current` Platform Binding;
- a JSON PipeWire graph observation containing exactly one input named
  `playback_FL` and one named `playback_FR` for the selected default node;
- an absolute POSIX target home; and
- an absolute POSIX SoundFont root.

The POSIX relocation values describe the Linux output artifact even when the
authoring test runs on Windows. They are data, not paths opened on the test
machine.

Before rendering, the command verifies the complete compiled-definition
fingerprint, the independent Platform Binding fingerprint, the two protected
source-file checksums, the authoring-only safety flags, selected Carla and
Patchbay targets, and an empty, unapplied graph delta. The checked-in protected
Carla and Patchbay materializers remain unchanged. Text-source checksums use
canonical LF bytes so an existing Windows CRLF checkout cannot change their
identity; repository attributes pin both sources to LF in new checkouts.

## Output Boundary

`--output-root` must be either a new path or an empty directory that is a
strict descendant of the platform's system temporary directory. The command
rejects the temporary directory itself, non-temporary locations, symbolic-link
roots, and nonempty roots. It never reads the installed Carla or Patchbay
target paths from the Platform Binding as output destinations.

The resulting bundle contains exactly:

~~~text
carla/pedro.uproject
patchbay/pedro-live-rack-patchbay.json
materialization.json
~~~

`materialization.json` records the compiled-definition identity, relocation
inputs, output byte counts and SHA-256 checksums, and the no-activation safety
state. It excludes timestamps and the temporary root, so identical inputs
produce identical bytes in different working directories and on Linux or
Windows.

## Offline Example

This example uses the checked-in graph snapshot as read-only default-sink
evidence and writes only below `/tmp`:

~~~bash
work_root="$(mktemp -d /tmp/music-rig-materialize.XXXXXX)"

python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --output "$work_root/compiled.json"

python3 docs/tools/music-rig/materialize-compiled-rig.py \
  --compiled-definition "$work_root/compiled.json" \
  --current-graph src/midi-setup/pedro-live-rack-patchbay.json \
  --default-node-name \
    alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__sink \
  --target-home /home/ldap/pedro.ferreira \
  --target-soundfont-root \
    "/home/ldap/pedro.ferreira/Flash/PED/MIDI/Pack de Timbres/Library" \
  --output-root "$work_root/bundle"
~~~

Use `--check-only` instead of `--output-root` to perform every validation and
render both artifacts in memory without creating any file.

The output is test evidence only. Promotion to an installed or live path is
not implemented by this command and remains prohibited before the later
deployment milestones.

Run the read-only [semantic parity gate](PARITY.md) against the completed
bundle to compare all 49 plugins, 111 Carla project connections, and 115
Patchbay links with the protected sources.

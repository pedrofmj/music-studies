# Windows Portable Build

Status: hosted Windows proof passed; `beanstar` is the selected physical
Windows reference machine, with audio and MIDI certification still pending.

## Scope

The portable C17 core, CLI, protocol golden frames, portability guard,
benchmark contracts, and synthetic Windows generation-adoption adapter run in
the `Music Rig Portable Core` GitHub Actions workflow.

The workflow pins the operating-system label to `windows-2025` instead of using
the moving `windows-latest` alias. GitHub documents this as an x64 hosted
Windows runner, and its maintained image includes Visual Studio, CMake, Python,
and the other tools required by the default build:

- [GitHub-hosted runners reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)
- [Windows Server 2025 runner image](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-Readme.md)

This CI runner proves Windows compilation and platform-contract behavior. It is
ephemeral and has no music hardware, so it is not the Windows audio reference
machine and cannot certify MIDI devices, audio backends, plugin hosts, xruns,
dropouts, resource budgets, or audible behavior.

## Native Reproduction

From a Visual Studio 2022 version 17.5 or newer developer PowerShell with CMake
and Python available:

```powershell
cmake -S docs/tools/music-rig -B build/music-rig `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/music-rig --config Release
ctest --test-dir build/music-rig -C Release --output-on-failure
```

No service is installed and no audio or MIDI API is opened. The default build
does not discover or link `json-c`. CMake enables MSVC's
`/experimental:c11atomics` option explicitly; the tests then require pointer
atomics to report lock-free behavior at runtime.

## Hosted Evidence

The first pinned Windows proof passed on 2026-08-10:

- commit: `280147a20e8d4edd8663fbfd0a27420699f8f768`;
- workflow: [run 31391585787](https://github.com/pedrofmj/music-studies/actions/runs/31391585787);
- runner label: `windows-2025` on x64;
- compiler job: [Windows MSVC](https://github.com/pedrofmj/music-studies/actions/runs/31391585787/job/93464356027);
- result: configure, build, and all nine configured CTest entries passed; and
- companion Linux GCC job: passed from the same commit and workflow.

The Windows test set covers the core, protocol golden frames, portability guard
and its self-test, CLI version, three benchmark-contract checks, native
generation adoption, and the named-pipe transport. The hosted runner is
repeatable build evidence, not an audio benchmark or hardware certification
result.

The named-pipe implementation first passed on commit
`4e9a4546057872ed291ae79c394f83913bdb9bd5` in
[run 31399835507](https://github.com/pedrofmj/music-studies/actions/runs/31399835507),
where all ten Windows tests passed and the job published a hash-manifested
reference bundle.

The opt-in Windows `json-c` proof passed on commit
`6c07e7a3e8ae208a067f0f1e8eb0527f525a154f` in
[run 31414189191](https://github.com/pedrofmj/music-studies/actions/runs/31414189191).
The default build passed 10/10 tests; the pinned vcpkg build passed 11/11 and
published the static library, license, exact fixture, probe, and unlinked
baseline CLI in the reference bundle.

## Evidence Boundary

A passing hosted workflow proves:

- the same portable core and CLI sources compile with MSVC;
- the version command executes on Windows;
- protocol request and response bytes match the portable golden frames;
- platform and backend identifiers remain outside the core;
- the benchmark contract validator behaves identically; and
- a native Windows thread observes lock-free immutable-generation publication.

It does not select or certify the Windows audio, MIDI, plugin-host, and service
backends.

The workflow run URL and pinned runner label are recorded with each accepted
hosted proof. The selected physical machine and its initial read-only inventory
are recorded in [WINDOWS-REFERENCE.md](WINDOWS-REFERENCE.md). Each physical run
additionally records the exact Windows build, artifact commit and hash,
hardware, drivers, and applicable backend versions.

The named-pipe and generation-adoption bundle passed on `beanstar`; the full
artifact, timing, and cleanup evidence is in
[benchmarks/windows-beanstar-m0.json](benchmarks/windows-beanstar-m0.json).
The optional JSON build and physical footprint result is in
[benchmarks/json-c-windows.json](benchmarks/json-c-windows.json).

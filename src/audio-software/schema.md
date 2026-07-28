# Audio Software Record Schema

catalog.json is a compatibility and licensing inventory, not a recommendation
or proof that software was tested on a particular computer.

## Required Fields

| Field | Purpose |
| --- | --- |
| id, name, publisher | Stable application identity. |
| categories, functions | What the application does. |
| platforms | Linux and Windows status with direct official evidence. |
| licensing | Source status, license, cost model, and dated price note. |
| official_resources | Official project, publisher, and documentation URLs. |
| manual | Official documentation URL and local-retention state. |

Platform status is one of native_supported, not_offered, planned, or unknown.
Native supported requires a current official native build or support statement.
Do not infer support from Wine, a community build, or an old release.

source_status is open_source or proprietary. cost_model is free, paid,
paid_with_trial, free_with_paid_options, or
free_source_or_distribution_with_paid_vendor_build. Open source and free are
separate facts. Every price note is a dated snapshot and needs an official URL.

manual.local_status is not_retained, retained, or not_retainable. Do not invent
a local file, checksum, page count, or text extraction before a PDF is retained.
The individual manuals.json records its URL, revision, filename, size, page
count, SHA-256, and extracted-text path.

Keep publisher documentation separate from local observations. A real test
record must include app version, OS, audio/MIDI backend, interface or device,
driver, plugin format, steps, and result.

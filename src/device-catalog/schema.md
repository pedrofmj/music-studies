# Capability Record Schema

Every device has `reference/capabilities.json`. It is a compact, structured
representation of the connection-relevant information in its stored manuals.
It is not a replacement for the manual or a claim that undocumented behavior
works.

## Required Top-Level Fields

| Field | Purpose |
| --- | --- |
| `schema_version` | Allows safe future evolution. |
| `device` | Stable ID, manufacturer, model, category, and repository root. |
| `manual_sources` | Local PDF and extracted-text sources used by the record. |
| `capabilities` | Audio, MIDI, USB, controller, storage, and power facts. |
| `connection_rules` | Constraints that determine whether a topology is valid. |
| `roles` | Intended roles that the documented capabilities support. |
| `unknowns` | Details not established by the stored official documentation. |

## Port Rules

Each MIDI or audio port records its `direction` from the device's perspective:

- `in`: the device receives the signal.
- `out`: the device sends the signal.
- `in_out`: the device both receives and sends it.
- `host_only`: a computer, mobile device, or adapter is required to make the
  route useful; it is not a direct device-to-device connection.

`connector` describes the physical interface. `protocol` describes what travels
over it. USB power, USB MIDI, USB audio, and USB storage are separate facts.

## Evidence Rules

- Each capability group and every connection rule includes `source_refs`.
- PDF page numbers refer to the stored original PDF page number, not the line
  number in extracted text.
- A web-only source may use `source_section` rather than PDF pages; its
  `authority` and unresolved limitations must be explicit.
- Manual statements are recorded as `documented`; successful Ubuntu or live
  testing should be added separately as observed evidence in mappings or setup
  logs.
- An `unknowns` entry must remain until a cited manual statement or an observed
  test resolves it.

## Update Procedure

1. Keep the original PDF and its checksum manifest.
2. Refresh the text extraction after adding or replacing a PDF.
3. Add or revise only facts with source references.
4. Update a connection recipe when the new fact changes a valid topology.
5. Record real endpoint names, cable adapters, and host settings in the
   device's mapping log; do not overwrite manual facts with host-specific data.

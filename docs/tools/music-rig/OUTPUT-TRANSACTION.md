# Output Transaction Contract

This document defines the Milestone 4 contract for a non-dry-run switch when
the runtime is output-enabled. It applies to global switches, device-profile
overrides, and device-reset transactions. The control thread owns every phase;
the real-time callback only observes the generation slot and never executes a
transaction.

## Phase Order

1. Validate the request, expected generation, target tables, stable ports, and
   readiness. No backend or persistent state is changed by validation.
2. Prepare the target generation through the output adapter. Preparation happens
   before publication. A preparation failure publishes nothing and does not
   require rollback.
3. Publish a target generation with a strictly greater generation ID. The
   target is now visible to the real-time adopter, but is not durable yet.
4. Confirm the target through the output adapter. Confirmation is the backend's
   acknowledgement that the target generation is usable.
5. Update the in-memory profile/base/override state and atomically persist the
   resulting state. A successful transaction reports `ROLLBACK_NOT_REQUIRED`.

Output-suppressed transactions skip backend preparation and confirmation but
retain the same publication and persistence ordering.

## Rollback

Rollback always creates a new generation. Generation IDs are never reused or
decremented, including after a failed transaction. The previous generation is
the generation that was active before phase 3.

If confirmation or persistence fails:

1. Invoke the adapter rollback callback with the previous generation. The
   callback must make the backend match that generation and must not mutate the
   graph.
2. Publish a rollback generation containing the previous mapping.
3. Restore the previous in-memory profile, base tables, and device overrides.
4. Persist the restored state.

Rollback is successful only when all four steps succeed. The response carries
the original transaction failure in `result_code`, `ROLLBACK_SUCCEEDED` in
`rollback_status`, and the rollback generation ID in `resulting_generation`.

If any rollback step fails, the response carries the original transaction
failure and `ROLLBACK_FAILED`. The runtime enters `FAILED` because its backend,
published generation, or durable state cannot be trusted. No later commit is
accepted from that runtime instance.

Initialization is different: if the initial output backend cannot be prepared
or confirmed, initialization fails before the runtime becomes usable and no
rollback is required.

## Adapter Rules

The output adapter is caller-owned and fixed-storage. Its callbacks must be
safe on the control thread and must not rely on JSON traversal, unbounded
allocation, or graph discovery. The rollback callback receives the previous
generation, not the failed target generation.

The output adapter is required only for output-enabled initialization. The
default daemon and all shadow commands remain output-suppressed.

## Evidence

Tests must cover target preparation failure, confirmation failure with
successful rollback, confirmation failure with failed rollback, persistence
failure with successful rollback, persistence failure with failed rollback,
strictly increasing generation IDs, restored state, and failed-runtime refusal.

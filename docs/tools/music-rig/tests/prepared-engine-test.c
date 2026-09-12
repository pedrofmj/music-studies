#include "music_rig/prepared_engine.h"

#include <stdio.h>
#include <string.h>

typedef enum failure_phase {
    FAILURE_NONE,
    FAILURE_STAGE,
    FAILURE_VALIDATE,
    FAILURE_ARM,
    FAILURE_COMMIT,
    FAILURE_ROLLBACK,
    FAILURE_DISCARD
} failure_phase;

typedef struct fake_engine {
    failure_phase failure;
    unsigned int stage_calls;
    unsigned int validate_calls;
    unsigned int arm_calls;
    unsigned int commit_calls;
    unsigned int rollback_calls;
    unsigned int discard_calls;
} fake_engine;

static music_rig_result result_for(fake_engine *engine, failure_phase phase)
{
    return engine->failure == phase
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_stage(
    void *opaque, const music_rig_generation *candidate
)
{
    fake_engine *engine = opaque;
    (void)candidate;
    engine->stage_calls += 1U;
    return result_for(engine, FAILURE_STAGE);
}

static music_rig_result fake_validate(
    void *opaque, const music_rig_generation *candidate
)
{
    fake_engine *engine = opaque;
    (void)candidate;
    engine->validate_calls += 1U;
    return result_for(engine, FAILURE_VALIDATE);
}

static music_rig_result fake_arm(
    void *opaque, const music_rig_generation *candidate
)
{
    fake_engine *engine = opaque;
    (void)candidate;
    engine->arm_calls += 1U;
    return result_for(engine, FAILURE_ARM);
}

static music_rig_result fake_commit(
    void *opaque, const music_rig_generation *candidate
)
{
    fake_engine *engine = opaque;
    (void)candidate;
    engine->commit_calls += 1U;
    return result_for(engine, FAILURE_COMMIT);
}

static music_rig_result fake_rollback(
    void *opaque, const music_rig_generation *previous
)
{
    fake_engine *engine = opaque;
    (void)previous;
    engine->rollback_calls += 1U;
    return result_for(engine, FAILURE_ROLLBACK);
}

static music_rig_result fake_discard(
    void *opaque, const music_rig_generation *candidate
)
{
    fake_engine *engine = opaque;
    (void)candidate;
    engine->discard_calls += 1U;
    return result_for(engine, FAILURE_DISCARD);
}

static music_rig_prepared_engine_adapter adapter_for(fake_engine *engine)
{
    music_rig_prepared_engine_adapter adapter = {0};

    adapter.abi_version = MUSIC_RIG_PREPARED_ENGINE_ADAPTER_ABI_VERSION;
    adapter.context = engine;
    adapter.stage = fake_stage;
    adapter.validate = fake_validate;
    adapter.arm = fake_arm;
    adapter.commit = fake_commit;
    adapter.rollback = fake_rollback;
    adapter.discard = fake_discard;
    return adapter;
}

static int expect_prepare_failure(failure_phase phase)
{
    fake_engine engine = {0};
    music_rig_prepared_engine_transaction transaction;
    music_rig_generation previous = {UINT64_C(1), NULL};
    music_rig_generation candidate = {UINT64_C(2), NULL};
    music_rig_prepared_engine_adapter adapter;

    engine.failure = phase;
    adapter = adapter_for(&engine);
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &adapter, &previous, &candidate
        ) != MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        engine.discard_calls != 1U || transaction.staged) {
        return 1;
    }
    return 0;
}

int main(void)
{
    fake_engine engine = {0};
    music_rig_prepared_engine_transaction transaction;
    music_rig_generation previous = {UINT64_C(1), NULL};
    music_rig_generation candidate = {UINT64_C(2), NULL};
    music_rig_prepared_engine_adapter adapter = adapter_for(&engine);

    if (expect_prepare_failure(FAILURE_STAGE) ||
        expect_prepare_failure(FAILURE_VALIDATE) ||
        expect_prepare_failure(FAILURE_ARM)) {
        fputs("prepared engine preparation failure was not fail-closed\n", stderr);
        return 1;
    }
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &adapter, &previous, &candidate
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_prepared_engine_transaction_commit(&transaction) !=
            MUSIC_RIG_RESULT_OK ||
        !transaction.committed ||
        music_rig_prepared_engine_transaction_rollback(&transaction) !=
            MUSIC_RIG_RESULT_OK ||
        engine.rollback_calls != 1U || engine.discard_calls != 1U) {
        fputs("prepared engine success transaction failed\n", stderr);
        return 1;
    }

    memset(&engine, 0, sizeof(engine));
    engine.failure = FAILURE_COMMIT;
    adapter = adapter_for(&engine);
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &adapter, &previous, &candidate
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_prepared_engine_transaction_commit(&transaction) !=
            MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        music_rig_prepared_engine_transaction_rollback(&transaction) !=
            MUSIC_RIG_RESULT_OK || engine.rollback_calls != 1U ||
        engine.discard_calls != 1U) {
        fputs("prepared engine commit failure did not roll back\n", stderr);
        return 1;
    }

    memset(&engine, 0, sizeof(engine));
    engine.failure = FAILURE_ROLLBACK;
    adapter = adapter_for(&engine);
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &adapter, &previous, &candidate
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_prepared_engine_transaction_rollback(&transaction) !=
            MUSIC_RIG_RESULT_ADAPTER_FAILURE || engine.rollback_calls != 1U ||
        engine.discard_calls != 1U) {
        fputs("prepared engine rollback failure was hidden\n", stderr);
        return 1;
    }

    memset(&engine, 0, sizeof(engine));
    engine.failure = FAILURE_DISCARD;
    adapter = adapter_for(&engine);
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &adapter, &previous, &candidate
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_prepared_engine_transaction_rollback(&transaction) !=
            MUSIC_RIG_RESULT_ADAPTER_FAILURE || engine.rollback_calls != 1U ||
        engine.discard_calls != 1U) {
        fputs("prepared engine discard failure was hidden\n", stderr);
        return 1;
    }
    puts("Prepared engine transaction tests: PASS");
    return 0;
}

#include "music_rig/prepared_engine.h"

#include <string.h>

bool music_rig_prepared_engine_adapter_is_valid(
    const music_rig_prepared_engine_adapter *adapter
)
{
    return adapter != NULL &&
        adapter->abi_version == MUSIC_RIG_PREPARED_ENGINE_ADAPTER_ABI_VERSION &&
        adapter->stage != NULL && adapter->validate != NULL &&
        adapter->arm != NULL && adapter->commit != NULL &&
        adapter->rollback != NULL && adapter->discard != NULL;
}

static music_rig_result discard_candidate(
    music_rig_prepared_engine_transaction *transaction
)
{
    music_rig_result result;

    if (!transaction->staged) {
        return MUSIC_RIG_RESULT_OK;
    }
    result = transaction->adapter.discard(
        transaction->adapter.context,
        transaction->candidate
    );
    transaction->staged = false;
    transaction->armed = false;
    transaction->committed = false;
    return result;
}

music_rig_result music_rig_prepared_engine_transaction_begin(
    music_rig_prepared_engine_transaction *transaction,
    const music_rig_prepared_engine_adapter *adapter,
    const music_rig_generation *previous,
    const music_rig_generation *candidate
)
{
    music_rig_result result;

    if (transaction == NULL ||
        !music_rig_prepared_engine_adapter_is_valid(adapter) ||
        previous == NULL || candidate == NULL ||
        previous->id == UINT64_C(0) || candidate->id == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(transaction, 0, sizeof(*transaction));
    transaction->adapter = *adapter;
    transaction->previous = previous;
    transaction->candidate = candidate;
    transaction->staged = true;

    result = transaction->adapter.stage(
        transaction->adapter.context, candidate
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)discard_candidate(transaction);
        return result;
    }
    result = transaction->adapter.validate(
        transaction->adapter.context, candidate
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)discard_candidate(transaction);
        return result;
    }
    result = transaction->adapter.arm(
        transaction->adapter.context, candidate
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)discard_candidate(transaction);
        return result;
    }
    transaction->armed = true;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_prepared_engine_transaction_commit(
    music_rig_prepared_engine_transaction *transaction
)
{
    music_rig_result result;

    if (transaction == NULL || !transaction->staged ||
        !transaction->armed || transaction->committed) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = transaction->adapter.commit(
        transaction->adapter.context, transaction->candidate
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        transaction->committed = true;
    }
    return result;
}

music_rig_result music_rig_prepared_engine_transaction_rollback(
    music_rig_prepared_engine_transaction *transaction
)
{
    music_rig_result rollback_result;
    music_rig_result discard_result;

    if (transaction == NULL || !transaction->staged) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    rollback_result = transaction->adapter.rollback(
        transaction->adapter.context, transaction->previous
    );
    discard_result = discard_candidate(transaction);
    if (rollback_result != MUSIC_RIG_RESULT_OK) {
        return rollback_result;
    }
    return discard_result;
}

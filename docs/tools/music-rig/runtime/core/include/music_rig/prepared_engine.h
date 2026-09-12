#ifndef MUSIC_RIG_PREPARED_ENGINE_H
#define MUSIC_RIG_PREPARED_ENGINE_H

#include "music_rig/definition.h"

#include <stdbool.h>
#include <stdint.h>

#define MUSIC_RIG_PREPARED_ENGINE_ADAPTER_ABI_VERSION UINT32_C(1)

typedef struct music_rig_prepared_engine_adapter {
    uint32_t abi_version;
    void *context;
    music_rig_result (*stage)(
        void *context,
        const music_rig_generation *candidate
    );
    music_rig_result (*validate)(
        void *context,
        const music_rig_generation *candidate
    );
    music_rig_result (*arm)(
        void *context,
        const music_rig_generation *candidate
    );
    music_rig_result (*commit)(
        void *context,
        const music_rig_generation *candidate
    );
    music_rig_result (*rollback)(
        void *context,
        const music_rig_generation *previous
    );
    music_rig_result (*discard)(
        void *context,
        const music_rig_generation *candidate
    );
} music_rig_prepared_engine_adapter;

typedef struct music_rig_prepared_engine_transaction {
    music_rig_prepared_engine_adapter adapter;
    const music_rig_generation *previous;
    const music_rig_generation *candidate;
    bool staged;
    bool armed;
    bool committed;
} music_rig_prepared_engine_transaction;

bool music_rig_prepared_engine_adapter_is_valid(
    const music_rig_prepared_engine_adapter *adapter
);

music_rig_result music_rig_prepared_engine_transaction_begin(
    music_rig_prepared_engine_transaction *transaction,
    const music_rig_prepared_engine_adapter *adapter,
    const music_rig_generation *previous,
    const music_rig_generation *candidate
);

music_rig_result music_rig_prepared_engine_transaction_commit(
    music_rig_prepared_engine_transaction *transaction
);

music_rig_result music_rig_prepared_engine_transaction_rollback(
    music_rig_prepared_engine_transaction *transaction
);

#endif

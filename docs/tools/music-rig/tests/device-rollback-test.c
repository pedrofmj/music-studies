#include "music_rig/runtime.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

static music_rig_compiled_tables tables;
static music_rig_generation generation = {UINT64_C(1), &tables};

static uint64_t now_ns(void *context)
{
    (void)context;
    return UINT64_C(1);
}

static music_rig_result control_start(void *context) { (void)context; return MUSIC_RIG_RESULT_OK; }
static music_rig_control_poll control_poll(void *context, music_rig_protocol_request *request)
{ (void)context; (void)request; return MUSIC_RIG_CONTROL_STOP; }
static music_rig_result control_wait(void *context) { (void)context; return MUSIC_RIG_RESULT_OK; }
static music_rig_result control_respond(void *context, const music_rig_protocol_response *response)
{ (void)context; (void)response; return MUSIC_RIG_RESULT_OK; }
static music_rig_result control_stop(void *context) { (void)context; return MUSIC_RIG_RESULT_OK; }
static music_rig_result storage_read(void *context, music_rig_storage_object object,
    uint8_t *output, size_t capacity, size_t *size)
{ (void)context; (void)object; (void)output; (void)capacity; (void)size; return MUSIC_RIG_RESULT_NOT_FOUND; }
static music_rig_result storage_replace(void *context, music_rig_storage_object object,
    const uint8_t *input, size_t size)
{ (void)context; (void)object; (void)input; (void)size; return MUSIC_RIG_RESULT_OK; }

int main(void)
{
    music_rig_generation_slot slot;
    music_rig_runtime runtime;
    music_rig_runtime_config config = {0};
    music_rig_platform_interfaces interfaces = {0};
    uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {0};

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK ||
        music_rig_generation_slot_init(&slot, &generation) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    interfaces.abi_version = MUSIC_RIG_RUNTIME_ABI_VERSION;
    interfaces.clock.now_ns = now_ns;
    interfaces.control.start = control_start;
    interfaces.control.poll = control_poll;
    interfaces.control.wait = control_wait;
    interfaces.control.respond = control_respond;
    interfaces.control.stop = control_stop;
    interfaces.storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    interfaces.storage.read = storage_read;
    interfaces.storage.atomic_replace = storage_replace;
    config.initial_generation = &generation;
    config.definition_fingerprint = fingerprint;
    config.definition_fingerprint_size = sizeof(fingerprint);
    config.active_rig_profile = "full-live-rack";
    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    puts("Device rollback contract fixture: OK");
    return 0;
}

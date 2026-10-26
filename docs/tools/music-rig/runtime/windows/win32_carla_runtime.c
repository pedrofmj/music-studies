#include "music_rig/win32_carla_runtime.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdlib.h>
#include <string.h>

typedef enum music_rig_win32_carla_runtime_state {
    MUSIC_RIG_WIN32_CARLA_RUNTIME_OPENING = 0,
    MUSIC_RIG_WIN32_CARLA_RUNTIME_RUNNING = 1,
    MUSIC_RIG_WIN32_CARLA_RUNTIME_CLOSING = 2
} music_rig_win32_carla_runtime_state;

struct music_rig_win32_carla_runtime {
    music_rig_winmm_midi winmm;
    music_rig_win32_carla_host *carla;
    music_rig_win32_carla_profile_map pad_profiles[
        MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY
    ];
    size_t pad_profile_count;
    uint16_t catalog_patch_count;
    DWORD owner_thread;
    size_t loopback_output_index;
    music_rig_win32_carla_runtime_state state;
};

static const uint8_t arturia_management_notes[16] = {
    40U, 41U, 42U, 43U, 36U, 37U, 38U, 39U,
    48U, 49U, 50U, 51U, 44U, 45U, 46U, 47U
};

static void copy_error(char *destination, size_t capacity, const char *message)
{
    size_t index;

    if (destination == NULL || capacity == 0U) {
        return;
    }
    destination[0] = '\0';
    if (message == NULL) {
        return;
    }
    for (index = 0U; index + 1U < capacity && message[index] != '\0'; ++index) {
        destination[index] = message[index];
    }
    destination[index] = '\0';
}

static music_rig_result validate_pad_profiles(
    const music_rig_win32_carla_profile_map *pad_profiles,
    size_t pad_profile_count,
    const music_rig_win32_carla_host_options *carla_options
)
{
    size_t index;

    if (pad_profile_count > MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY ||
        (pad_profile_count != 0U && pad_profiles == NULL) ||
        carla_options == NULL || carla_options->catalog_patch_count == 0U ||
        carla_options->patch_count == 0U ||
        carla_options->catalog_patch_count != carla_options->patch_count) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < pad_profile_count; ++index) {
        const music_rig_win32_carla_profile_map *profile =
            &pad_profiles[index];
        music_rig_arturia_shared_sf2_plan plan;
        size_t previous;
        music_rig_result result;

        if (profile->management_pad_code == 0U ||
            profile->management_pad_code > 16U ||
            profile->management_pad_code == 2U) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        for (previous = 0U; previous < index; ++previous) {
            if (pad_profiles[previous].management_pad_code ==
                    profile->management_pad_code) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
        result = music_rig_arturia_shared_sf2_plan_build(
            profile->patch_indexes, carla_options->catalog_patch_count,
            profile->active_layers, profile->active_layer_count, &plan
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            return result;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static uint8_t management_code_for_note(uint8_t note)
{
    size_t index;

    for (index = 0U; index < sizeof(arturia_management_notes); ++index) {
        if (arturia_management_notes[index] == note) {
            return (uint8_t)(index + 1U);
        }
    }
    return 0U;
}

static music_rig_result enqueue_input_midi(
    void *context,
    size_t slot_index,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_win32_carla_runtime *runtime = context;
    uint8_t management_code;
    size_t index;

    (void)slot_index;
    if (message != NULL && message_size == 3U &&
        (message[0] == UINT8_C(0x99) || message[0] == UINT8_C(0x89))) {
        management_code = management_code_for_note(message[1]);
        if (management_code != 0U) {
            if (message[0] == UINT8_C(0x99) && message[2] != 0U) {
                for (index = 0U; index < runtime->pad_profile_count; ++index) {
                    const music_rig_win32_carla_profile_map *profile =
                        &runtime->pad_profiles[index];

                    if (profile->management_pad_code == management_code) {
                        return music_rig_win32_carla_host_enqueue_profile(
                            runtime->carla, profile->patch_indexes,
                            runtime->catalog_patch_count,
                            profile->active_layers,
                            profile->active_layer_count
                        );
                    }
                }
            }
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return music_rig_win32_carla_host_enqueue_midi(
        runtime->carla, message, message_size
    );
}

static music_rig_result send_loopback_midi(
    void *context,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_win32_carla_runtime *runtime = context;

    return music_rig_winmm_midi_send(
        &runtime->winmm, runtime->loopback_output_index,
        message, message_size
    );
}

static bool valid_loopback_slot(
    const char *slot,
    const music_rig_winmm_midi_endpoint *endpoints,
    size_t endpoint_count,
    size_t *slot_index
)
{
    size_t length = 0U;
    size_t index;
    bool found = false;

    if (slot == NULL || endpoints == NULL || slot_index == NULL) {
        return false;
    }
    while (length < MUSIC_RIG_IDENTIFIER_CAPACITY && slot[length] != '\0') {
        ++length;
    }
    if (length == 0U || length == MUSIC_RIG_IDENTIFIER_CAPACITY) {
        return false;
    }
    for (index = 0U; index < endpoint_count; ++index) {
        if (strcmp(slot, endpoints[index].slot) == 0) {
            if (found) {
                return false;
            }
            found = true;
            *slot_index = index;
        }
    }
    return found;
}

static bool winmm_has_resources(const music_rig_winmm_midi *winmm)
{
    size_t index;

    if (winmm->input_worker_started || winmm->output_worker_started ||
        atomic_load_explicit(&winmm->active, memory_order_acquire)) {
        return true;
    }
    for (index = 0U; index < winmm->endpoint_count; ++index) {
        if (winmm->input_handles[index] != NULL ||
            winmm->output_handles[index] != NULL) {
            return true;
        }
    }
    return false;
}

static music_rig_result retain_first_failure(
    music_rig_result first,
    music_rig_result next
)
{
    return first == MUSIC_RIG_RESULT_OK ? next : first;
}

music_rig_result music_rig_win32_carla_runtime_open(
    music_rig_win32_carla_runtime **runtime_output,
    const music_rig_winmm_midi_ops *winmm_ops,
    void *winmm_ops_context,
    const music_rig_winmm_midi_endpoint *endpoints,
    size_t endpoint_count,
    const music_rig_win32_carla_profile_map *pad_profiles,
    size_t pad_profile_count,
    const music_rig_win32_carla_host_options *carla_options,
    const char *loopback_output_slot,
    char *error_message,
    size_t error_capacity
)
{
    music_rig_win32_carla_runtime *runtime;
    music_rig_win32_carla_host_options opening_options;
    music_rig_result result;
    char carla_error[512];

    copy_error(error_message, error_capacity, "");
    if (runtime_output == NULL) {
        copy_error(error_message, error_capacity,
            "Runtime output pointer is required");
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *runtime_output = NULL;
    if (winmm_ops == NULL || endpoints == NULL || endpoint_count == 0U ||
        carla_options == NULL || loopback_output_slot == NULL) {
        copy_error(error_message, error_capacity,
            "WinMM endpoints, Carla options, and a loopback output slot are required");
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = validate_pad_profiles(
        pad_profiles, pad_profile_count, carla_options
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        copy_error(error_message, error_capacity,
            "Invalid Arturia management-pad profile map");
        return result;
    }
    runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL) {
        copy_error(error_message, error_capacity,
            "Could not allocate the Windows Carla runtime");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    runtime->state = MUSIC_RIG_WIN32_CARLA_RUNTIME_OPENING;
    runtime->owner_thread = GetCurrentThreadId();
    runtime->pad_profile_count = pad_profile_count;
    runtime->catalog_patch_count = carla_options->catalog_patch_count;
    if (pad_profile_count != 0U) {
        memcpy(runtime->pad_profiles, pad_profiles,
            pad_profile_count * sizeof(runtime->pad_profiles[0]));
    }
    result = music_rig_winmm_midi_init(
        &runtime->winmm, winmm_ops, winmm_ops_context, endpoints,
        endpoint_count
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        copy_error(error_message, error_capacity,
            "Could not initialize the WinMM MIDI adapter");
        free(runtime);
        return result;
    }
    if (!valid_loopback_slot(loopback_output_slot, runtime->winmm.endpoints,
            runtime->winmm.endpoint_count, &runtime->loopback_output_index)) {
        copy_error(error_message, error_capacity,
            "Loopback output slot is not uniquely present in the endpoint table");
        free(runtime);
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_winmm_midi_set_input_observer(
        &runtime->winmm, enqueue_input_midi, runtime
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        copy_error(error_message, error_capacity,
            "Could not install the WinMM-to-Carla MIDI observer");
        free(runtime);
        return result;
    }

    opening_options = *carla_options;
    opening_options.raw_midi_output = send_loopback_midi;
    opening_options.raw_midi_output_context = runtime;
    result = music_rig_win32_carla_host_open(
        &runtime->carla, &opening_options, carla_error, sizeof(carla_error)
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        copy_error(error_message, error_capacity, carla_error);
        free(runtime);
        return result;
    }
    result = music_rig_winmm_midi_start(&runtime->winmm);
    if (result != MUSIC_RIG_RESULT_OK) {
        music_rig_result first_failure = result;

        if (winmm_has_resources(&runtime->winmm)) {
            music_rig_result stop_result = music_rig_winmm_midi_stop(
                &runtime->winmm
            );

            first_failure = retain_first_failure(first_failure, stop_result);
        }
        if (winmm_has_resources(&runtime->winmm)) {
            runtime->state = MUSIC_RIG_WIN32_CARLA_RUNTIME_CLOSING;
            *runtime_output = runtime;
            copy_error(error_message, error_capacity,
                "WinMM startup failed and cleanup remains pending; retry runtime close");
            return first_failure;
        }
        (void)music_rig_win32_carla_host_close(&runtime->carla);
        copy_error(error_message, error_capacity,
            "Could not start WinMM after opening Carla");
        free(runtime);
        return first_failure;
    }
    runtime->state = MUSIC_RIG_WIN32_CARLA_RUNTIME_RUNNING;
    *runtime_output = runtime;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_win32_carla_runtime_pump(
    music_rig_win32_carla_runtime *runtime,
    size_t *drained_message_count
)
{
    size_t drained = 0U;
    music_rig_result first_failure;
    music_rig_result drain_result;

    if (drained_message_count != NULL) {
        *drained_message_count = 0U;
    }
    if (runtime == NULL || runtime->state !=
            MUSIC_RIG_WIN32_CARLA_RUNTIME_RUNNING ||
        runtime->owner_thread != GetCurrentThreadId()) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    first_failure = music_rig_win32_carla_host_idle(runtime->carla);
    drain_result = music_rig_win32_carla_host_drain_midi(
        runtime->carla, &drained
    );
    if (drained_message_count != NULL) {
        *drained_message_count = drained;
    }
    return retain_first_failure(first_failure, drain_result);
}

music_rig_result music_rig_win32_carla_runtime_idle(
    music_rig_win32_carla_runtime *runtime
)
{
    return music_rig_win32_carla_runtime_pump(runtime, NULL);
}

music_rig_result music_rig_win32_carla_runtime_select_profile(
    music_rig_win32_carla_runtime *runtime,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
)
{
    if (runtime == NULL || runtime->state !=
            MUSIC_RIG_WIN32_CARLA_RUNTIME_RUNNING ||
        runtime->owner_thread != GetCurrentThreadId()) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    return music_rig_win32_carla_host_select_profile(
        runtime->carla, patch_indexes, catalog_patch_count, active_layers,
        active_layer_count
    );
}

music_rig_result music_rig_win32_carla_runtime_close(
    music_rig_win32_carla_runtime **runtime_pointer
)
{
    music_rig_win32_carla_runtime *runtime;
    music_rig_result first_failure = MUSIC_RIG_RESULT_OK;
    music_rig_result stop_result;

    if (runtime_pointer == NULL || *runtime_pointer == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    runtime = *runtime_pointer;
    if (runtime->owner_thread != GetCurrentThreadId() ||
        (runtime->state != MUSIC_RIG_WIN32_CARLA_RUNTIME_RUNNING &&
            runtime->state != MUSIC_RIG_WIN32_CARLA_RUNTIME_CLOSING)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    runtime->state = MUSIC_RIG_WIN32_CARLA_RUNTIME_CLOSING;
    stop_result = winmm_has_resources(&runtime->winmm)
        ? music_rig_winmm_midi_stop(&runtime->winmm)
        : MUSIC_RIG_RESULT_OK;
    first_failure = retain_first_failure(first_failure, stop_result);
    if (winmm_has_resources(&runtime->winmm)) {
        return first_failure == MUSIC_RIG_RESULT_OK
            ? MUSIC_RIG_RESULT_ADAPTER_FAILURE : first_failure;
    }
    first_failure = retain_first_failure(
        first_failure, music_rig_win32_carla_host_close(&runtime->carla)
    );
    free(runtime);
    *runtime_pointer = NULL;
    return first_failure;
}

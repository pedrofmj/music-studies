#define _POSIX_C_SOURCE 200809L

#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>
#include <lv2/state/state.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_BLOCK 1024U
#define ATOM_BUFFER_SIZE 65536U
#define MAX_URIS 128U
#define MAX_CONTROLS 256U

typedef struct uri_map {
    char *uris[MAX_URIS];
    uint32_t count;
} uri_map;

typedef struct control_probe {
    uri_map map;
    LilvInstance *instance;
    const LV2_Worker_Interface *worker;
    const LV2_State_Interface *state;
    float controls[MAX_CONTROLS];
    float *target;
    unsigned char state_data[64][1024];
    uint32_t state_keys[64];
    uint32_t state_types[64];
    uint32_t state_flags[64];
    size_t state_sizes[64];
    uint32_t state_count;
} control_probe;

static LV2_URID map_uri(LV2_URID_Map_Handle handle, const char *uri)
{
    uri_map *map = handle;
    uint32_t index;

    for (index = 0U; index < map->count; ++index) {
        if (strcmp(map->uris[index], uri) == 0) {
            return index + 1U;
        }
    }
    if (map->count >= MAX_URIS) {
        return 0U;
    }
    map->uris[map->count] = strdup(uri);
    if (map->uris[map->count] == NULL) {
        return 0U;
    }
    map->count += 1U;
    return map->count;
}

static const char *unmap_uri(LV2_URID_Unmap_Handle handle, LV2_URID urid)
{
    uri_map *map = handle;

    return urid == 0U || urid > map->count ? NULL : map->uris[urid - 1U];
}

static LV2_Worker_Status worker_respond(
    LV2_Worker_Respond_Handle handle, uint32_t size, const void *data
)
{
    control_probe *probe = handle;

    return probe->worker == NULL || probe->worker->work_response == NULL
        ? LV2_WORKER_SUCCESS
        : probe->worker->work_response(probe->instance->lv2_handle, size, data);
}

static LV2_Worker_Status schedule_work(
    LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data
)
{
    control_probe *probe = handle;

    if (probe->worker == NULL || probe->worker->work == NULL ||
        probe->worker->work(
            probe->instance->lv2_handle,
            worker_respond,
            probe,
            size,
            data
        ) != LV2_WORKER_SUCCESS) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    if (probe->worker->end_run != NULL &&
        probe->worker->end_run(probe->instance->lv2_handle) !=
            LV2_WORKER_SUCCESS) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    return LV2_WORKER_SUCCESS;
}

static LV2_State_Status state_store(
    LV2_State_Handle handle, uint32_t key, const void *data, size_t size,
    uint32_t type, uint32_t flags
)
{
    control_probe *probe = handle;

    if (probe->state_count >= 64U || size > sizeof(probe->state_data[0])) {
        return LV2_STATE_ERR_NO_SPACE;
    }
    probe->state_keys[probe->state_count] = key;
    probe->state_types[probe->state_count] = type;
    probe->state_flags[probe->state_count] = flags;
    probe->state_sizes[probe->state_count] = size;
    memcpy(probe->state_data[probe->state_count], data, size);
    probe->state_count += 1U;
    return LV2_STATE_SUCCESS;
}

static const void *state_retrieve(
    LV2_State_Handle handle, uint32_t key, size_t *size, uint32_t *type,
    uint32_t *flags
)
{
    control_probe *probe = handle;
    uint32_t index;

    for (index = 0U; index < probe->state_count; ++index) {
        if (probe->state_keys[index] == key) {
            *size = probe->state_sizes[index];
            *type = probe->state_types[index];
            *flags = probe->state_flags[index];
            return probe->state_data[index];
        }
    }
    return NULL;
}

static void free_map(uri_map *map)
{
    uint32_t index;

    for (index = 0U; index < map->count; ++index) {
        free(map->uris[index]);
    }
}

int main(void)
{
    LV2_Feature feature_map = {LV2_URID__map, NULL};
    LV2_Feature feature_unmap = {LV2_URID__unmap, NULL};
    LV2_Worker_Schedule worker_schedule = {NULL, schedule_work};
    LV2_Feature feature_worker = {LV2_WORKER__schedule, &worker_schedule};
    const LV2_Feature *features[] = {
        &feature_map, &feature_unmap, &feature_worker, NULL
    };
    const char *plugin_uri = "http://synthv1.sourceforge.net/lv2";
    LilvWorld *world = lilv_world_new();
    LilvNode *uri;
    LilvNode *input_class;
    LilvNode *output_class;
    LilvNode *audio_class;
    LilvNode *control_class;
    const LilvPlugin *plugin;
    LilvInstance *instance;
    control_probe probe = {0};
    LV2_URID_Map map_feature;
    LV2_URID_Unmap unmap_feature;
    uint8_t atom_input[ATOM_BUFFER_SIZE] = {0};
    uint8_t atom_output[ATOM_BUFFER_SIZE] = {0};
    float audio_in[2][AUDIO_BLOCK] = {{0}};
    float audio_out[2][AUDIO_BLOCK] = {{0}};
    uint32_t control_count = 0U;
    uint32_t audio_in_count = 0U;
    uint32_t audio_out_count = 0U;
    uint32_t index;
    float before;
    float after;
    bool finite = true;

    map_feature.handle = &probe.map;
    map_feature.map = map_uri;
    unmap_feature.handle = &probe.map;
    unmap_feature.unmap = unmap_uri;
    feature_map.data = &map_feature;
    feature_unmap.data = &unmap_feature;
    worker_schedule.handle = &probe;
    if (world == NULL) {
        return 1;
    }
    lilv_world_load_all(world);
    uri = lilv_new_uri(world, plugin_uri);
    plugin = lilv_plugins_get_by_uri(
        lilv_world_get_all_plugins(world), uri
    );
    input_class = lilv_new_uri(world, LILV_URI_INPUT_PORT);
    output_class = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
    audio_class = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
    control_class = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
    if (plugin == NULL) {
        fputs("synthv1 plugin not found\n", stderr);
        return 1;
    }
    instance = lilv_plugin_instantiate(plugin, 48000.0, features);
    if (instance == NULL) {
        fputs("synthv1 instantiation failed\n", stderr);
        return 1;
    }
    probe.instance = instance;
    probe.worker = (const LV2_Worker_Interface *)
        lilv_instance_get_extension_data(instance, LV2_WORKER__interface);
    probe.state = (const LV2_State_Interface *)
        lilv_instance_get_extension_data(instance, LV2_STATE__interface);
    for (index = 0U; index < lilv_plugin_get_num_ports(plugin); ++index) {
        const LilvPort *port = lilv_plugin_get_port_by_index(plugin, index);
        const LilvNode *symbol = lilv_port_get_symbol(plugin, port);
        bool input = lilv_port_is_a(plugin, port, input_class);
        bool output = lilv_port_is_a(plugin, port, output_class);
        bool audio = lilv_port_is_a(plugin, port, audio_class);
        bool control = lilv_port_is_a(plugin, port, control_class);
        float value = 0.0F;
        LilvNode *default_value = NULL;
        LilvNode *minimum = NULL;
        LilvNode *maximum = NULL;

        if (audio && input && audio_in_count < 2U) {
            lilv_instance_connect_port(instance, index, audio_in[audio_in_count++]);
        } else if (audio && output && audio_out_count < 2U) {
            lilv_instance_connect_port(instance, index, audio_out[audio_out_count++]);
        } else if (control && input && control_count < MAX_CONTROLS) {
            lilv_port_get_range(plugin, port, &default_value, &minimum, &maximum);
            if (default_value != NULL) {
                value = lilv_node_as_float(default_value);
            }
            probe.controls[control_count] = value;
            lilv_instance_connect_port(instance, index, &probe.controls[control_count]);
            if (strcmp(lilv_node_as_string(symbol), "DCO1_BALANCE") == 0) {
                probe.target = &probe.controls[control_count];
            }
            control_count += 1U;
        }
        lilv_node_free(default_value);
        lilv_node_free(minimum);
        lilv_node_free(maximum);
    }
    if (probe.target == NULL || audio_out_count != 2U) {
        fputs("synthv1 target/audio ports were not resolved\n", stderr);
        return 1;
    }
    before = *probe.target;
    *probe.target = 0.75F;
    after = *probe.target;
    lilv_instance_connect_port(instance, 0U, atom_input);
    lilv_instance_connect_port(instance, 1U, atom_output);
    lilv_instance_activate(instance);
    for (index = 0U; index < 8U; ++index) {
        lilv_instance_run(instance, AUDIO_BLOCK);
        for (uint32_t channel = 0U; channel < audio_out_count; ++channel) {
            for (uint32_t sample = 0U; sample < AUDIO_BLOCK; ++sample) {
                if (!isfinite(audio_out[channel][sample])) {
                    finite = false;
                }
            }
        }
    }
    lilv_instance_deactivate(instance);
    {
        LV2_State_Status save_status = probe.state == NULL
            ? LV2_STATE_ERR_NO_FEATURE
            : probe.state->save(instance->lv2_handle, state_store, &probe, 0U, NULL);
        LV2_State_Status restore_status = probe.state == NULL
            ? LV2_STATE_ERR_NO_FEATURE
            : probe.state->restore(instance->lv2_handle, state_retrieve, &probe, 0U, NULL);
        printf(
            "control_count=%u target_before=%.3f target_after=%.3f "
            "finite_audio=%s state_save_status=%u state_restore_status=%u "
            "state_properties=%u\n",
            control_count, before, after, finite ? "true" : "false",
            (unsigned)save_status, (unsigned)restore_status, probe.state_count
        );
    }
    lilv_instance_free(instance);
    lilv_node_free(uri);
    lilv_node_free(input_class);
    lilv_node_free(output_class);
    lilv_node_free(audio_class);
    lilv_node_free(control_class);
    lilv_world_free(world);
    free_map(&probe.map);
    return finite && after == 0.75F ? 0 : 1;
}

#define _POSIX_C_SOURCE 200809L

#include <lilv/lilv.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/state/state.h>
#include <lv2/patch/patch.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_BLOCK 1024U
#define ATOM_BUFFER_SIZE 262144U
#define MAX_URIS 128U

typedef struct uri_map {
    char *uris[MAX_URIS];
    uint32_t count;
} uri_map;

typedef struct probe {
    uri_map map;
    LV2_URID atom_object;
    LV2_URID atom_sequence;
    LV2_URID atom_float;
    LV2_URID patch_set;
    LV2_URID patch_get;
    LV2_URID patch_put;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID parameter;
    LV2_URID input_index;
    LV2_URID output_index;
    LV2_URID audio_in_index[8];
    LV2_URID audio_out_index[8];
    uint32_t audio_in_count;
    uint32_t audio_out_count;
    LilvInstance *instance;
    const LV2_Worker_Interface *worker;
    const LV2_State_Interface *state;
    struct {
        uint32_t key;
        uint32_t type;
        uint32_t flags;
        size_t size;
        unsigned char data[4096];
    } properties[64];
    uint32_t property_count;
} probe;

static LV2_State_Status state_store(
    LV2_State_Handle handle,
    uint32_t key,
    const void *data,
    size_t size,
    uint32_t type,
    uint32_t flags
)
{
    probe *value = handle;

    if (value->property_count >= 64U || size > sizeof(value->properties[0].data)) {
        return LV2_STATE_ERR_NO_SPACE;
    }
    value->properties[value->property_count].key = key;
    value->properties[value->property_count].type = type;
    value->properties[value->property_count].flags = flags;
    value->properties[value->property_count].size = size;
    memcpy(value->properties[value->property_count].data, data, size);
    value->property_count += 1U;
    return LV2_STATE_SUCCESS;
}

static const void *state_retrieve(
    LV2_State_Handle handle,
    uint32_t key,
    size_t *size,
    uint32_t *type,
    uint32_t *flags
)
{
    probe *value = handle;
    uint32_t index;

    for (index = 0U; index < value->property_count; ++index) {
        if (value->properties[index].key == key) {
            *size = value->properties[index].size;
            *type = value->properties[index].type;
            *flags = value->properties[index].flags;
            return value->properties[index].data;
        }
    }
    return NULL;
}

static LV2_Worker_Status worker_respond(
    LV2_Worker_Respond_Handle handle, uint32_t size, const void *data
)
{
    probe *value = handle;

    return value->worker == NULL || value->worker->work_response == NULL
        ? LV2_WORKER_SUCCESS
        : value->worker->work_response(
            value->instance->lv2_handle, size, data
        );
}

static LV2_Worker_Status schedule_work(
    LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data
)
{
    probe *value = handle;

    if (value->worker == NULL || value->worker->work == NULL) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    if (value->worker->work(
            value->instance->lv2_handle,
            worker_respond,
            value,
            size,
            data
        ) != LV2_WORKER_SUCCESS) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    if (value->worker->end_run != NULL &&
        value->worker->end_run(value->instance->lv2_handle) !=
            LV2_WORKER_SUCCESS) {
        return LV2_WORKER_ERR_UNKNOWN;
    }
    return LV2_WORKER_SUCCESS;
}

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

static void map_common_uris(probe *value)
{
    value->atom_object = map_uri(&value->map, LV2_ATOM__Object);
    value->atom_sequence = map_uri(&value->map, LV2_ATOM__Sequence);
    value->atom_float = map_uri(&value->map, LV2_ATOM__Float);
    value->patch_set = map_uri(&value->map, LV2_PATCH__Set);
    value->patch_get = map_uri(&value->map, LV2_PATCH__Get);
    value->patch_put = map_uri(&value->map, LV2_PATCH__Put);
    value->patch_property = map_uri(&value->map, LV2_PATCH__property);
    value->patch_value = map_uri(&value->map, LV2_PATCH__value);
    value->parameter = map_uri(
        &value->map,
        "https://surge-synthesizer.github.io/lv2/surge-xt:a_filter1_cutoff"
    );
}

static void free_map(uri_map *map)
{
    uint32_t index;

    for (index = 0U; index < map->count; ++index) {
        free(map->uris[index]);
    }
}

static int write_patch_set(probe *value, uint8_t *buffer, size_t capacity)
{
    LV2_URID_Map urid_map = {&value->map, map_uri};
    LV2_Atom_Forge forge;
    LV2_Atom_Forge_Frame sequence;
    LV2_Atom_Forge_Frame object;

    lv2_atom_forge_init(&forge, &urid_map);
    lv2_atom_forge_set_buffer(&forge, buffer, capacity);
    lv2_atom_forge_sequence_head(&forge, &sequence, 0U);
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_object(&forge, &object, 0U, value->patch_set);
    lv2_atom_forge_key(&forge, value->patch_property);
    lv2_atom_forge_urid(&forge, value->parameter);
    lv2_atom_forge_key(&forge, value->patch_value);
    lv2_atom_forge_float(&forge, 0.75F);
    lv2_atom_forge_pop(&forge, &object);
    lv2_atom_forge_frame_time(&forge, 1);
    lv2_atom_forge_object(&forge, &object, 0U, value->patch_get);
    lv2_atom_forge_key(&forge, value->patch_property);
    lv2_atom_forge_urid(&forge, value->parameter);
    lv2_atom_forge_pop(&forge, &object);
    lv2_atom_forge_pop(&forge, &sequence);
    return forge.offset != 0U ? 0 : 1;
}

static uint32_t count_patch_responses(
    const probe *value, const LV2_Atom_Sequence *sequence
)
{
    uint32_t count = 0U;
    LV2_ATOM_SEQUENCE_FOREACH(sequence, event) {
        if (event->body.type != value->atom_object) {
            continue;
        }
        {
            const LV2_Atom_Object *object = (const LV2_Atom_Object *)&event->body;
            if (object->body.otype == value->patch_put ||
                object->body.otype == value->patch_set) {
                count += 1U;
            }
        }
    }
    return count;
}

int main(int argc, char **argv)
{
    LV2_Feature feature_map = {LV2_URID__map, NULL};
    LV2_Feature feature_unmap = {LV2_URID__unmap, NULL};
    LV2_Feature feature_options = {LV2_OPTIONS__options, NULL};
    LV2_Feature feature_bounded = {LV2_BUF_SIZE__boundedBlockLength, NULL};
    LV2_Worker_Schedule worker_schedule = {NULL, schedule_work};
    LV2_Feature feature_worker = {LV2_WORKER__schedule, &worker_schedule};
    const LV2_Feature *features[] = {
        &feature_map, &feature_unmap, &feature_options, &feature_bounded,
        &feature_worker, NULL
    };
    const char *plugin_uri =
        "https://surge-synthesizer.github.io/lv2/surge-xt";
    LilvWorld *world;
    LilvPlugins *plugins;
    LilvNode *uri;
    const LilvPlugin *plugin;
    LilvInstance *instance;
    probe value = {0};
    LV2_URID_Map map_feature;
    LV2_URID_Unmap unmap_feature;
    uint8_t input_buffer[ATOM_BUFFER_SIZE] = {0};
    uint8_t output_buffer[ATOM_BUFFER_SIZE] = {0};
    float audio_in[2][AUDIO_BLOCK] = {{0}};
    float audio_out[6][AUDIO_BLOCK] = {{0}};
    float control = 0.0F;
    float enabled = 1.0F;
    uint32_t index;
    uint32_t responses = 0U;
    bool finite = true;
    float sample_rate = 48000.0F;
    int32_t max_block = AUDIO_BLOCK;
    LV2_Options_Option options[3] = {0};

    (void)argc;
    (void)argv;
    map_feature.handle = &value.map;
    map_feature.map = map_uri;
    unmap_feature.handle = &value.map;
    unmap_feature.unmap = unmap_uri;
    feature_map.data = &map_feature;
    feature_unmap.data = &unmap_feature;
    map_common_uris(&value);
    options[0].context = LV2_OPTIONS_INSTANCE;
    options[0].key = map_uri(&value.map, LV2_PARAMETERS__sampleRate);
    options[0].size = sizeof(sample_rate);
    options[0].type = map_uri(&value.map, LV2_ATOM__Float);
    options[0].value = &sample_rate;
    options[1].context = LV2_OPTIONS_INSTANCE;
    options[1].key = map_uri(&value.map, LV2_BUF_SIZE__maxBlockLength);
    options[1].size = sizeof(max_block);
    options[1].type = map_uri(&value.map, LV2_ATOM__Int);
    options[1].value = &max_block;
    feature_options.data = options;
    world = lilv_world_new();
    if (world == NULL) {
        return 1;
    }
    lilv_world_load_all(world);
    uri = lilv_new_uri(world, plugin_uri);
    plugins = (LilvPlugins *)lilv_world_get_all_plugins(world);
    plugin = lilv_plugins_get_by_uri(plugins, uri);
    if (plugin == NULL) {
        fprintf(stderr, "plugin not found: %s\n", plugin_uri);
        lilv_node_free(uri);
        lilv_world_free(world);
        return 1;
    }
    instance = lilv_plugin_instantiate(plugin, 48000.0, features);
    if (instance == NULL) {
        fputs("plugin instantiation failed\n", stderr);
        lilv_node_free(uri);
        lilv_world_free(world);
        return 1;
    }
    value.instance = instance;
    value.worker = (const LV2_Worker_Interface *)
        lilv_instance_get_extension_data(instance, LV2_WORKER__interface);
    value.state = (const LV2_State_Interface *)
        lilv_instance_get_extension_data(instance, LV2_STATE__interface);
    value.input_index = UINT32_MAX;
    value.output_index = UINT32_MAX;
    for (index = 0U; index < lilv_plugin_get_num_ports(plugin); ++index) {
        const LilvPort *port = lilv_plugin_get_port_by_index(plugin, index);
        const LilvNode *symbol = lilv_port_get_symbol(plugin, port);
        const char *name = lilv_node_as_string(symbol);

        if (strcmp(name, "in") == 0) {
            value.input_index = index;
        } else if (strcmp(name, "out") == 0) {
            value.output_index = index;
        } else if (strcmp(name, "enabled") == 0) {
            lilv_instance_connect_port(instance, index, &enabled);
        } else if (strcmp(name, "freeWheeling") == 0) {
            lilv_instance_connect_port(instance, index, &control);
        } else if (lilv_port_is_a(
                plugin, port, lilv_new_uri(world, LILV_URI_AUDIO_PORT)
            )) {
            if (value.audio_in_count < 2U) {
                value.audio_in_index[value.audio_in_count++] = index;
            } else if (value.audio_out_count < 6U) {
                value.audio_out_index[value.audio_out_count++] = index;
            }
        }
    }
    if (value.input_index == UINT32_MAX || value.output_index == UINT32_MAX ||
        write_patch_set(&value, input_buffer, sizeof(input_buffer)) != 0) {
        fputs("LV2 atom setup failed\n", stderr);
        lilv_instance_free(instance);
        lilv_node_free(uri);
        lilv_world_free(world);
        free_map(&value.map);
        return 1;
    }
    for (index = 0U; index < value.audio_in_count; ++index) {
        lilv_instance_connect_port(instance, value.audio_in_index[index],
            audio_in[index]);
    }
    for (index = 0U; index < value.audio_out_count; ++index) {
        lilv_instance_connect_port(instance, value.audio_out_index[index],
            audio_out[index]);
    }
    lilv_instance_connect_port(instance, value.input_index, input_buffer);
    lilv_instance_connect_port(instance, value.output_index, output_buffer);
    {
        LV2_Atom_Sequence *output = (LV2_Atom_Sequence *)output_buffer;
        output->atom.type = value.atom_sequence;
        output->atom.size = sizeof(LV2_Atom_Sequence_Body);
        output->body.unit = 0U;
        output->body.pad = 0U;
    }
    lilv_instance_activate(instance);
    for (index = 0U; index < 8U; ++index) {
        lilv_instance_run(instance, AUDIO_BLOCK);
        responses += count_patch_responses(
            &value, (const LV2_Atom_Sequence *)output_buffer
        );
        for (uint32_t channel = 0U; channel < value.audio_out_count; ++channel) {
            for (uint32_t sample = 0U; sample < AUDIO_BLOCK; ++sample) {
                if (!isfinite(audio_out[channel][sample])) {
                    finite = false;
                }
            }
        }
        memset(input_buffer, 0, sizeof(input_buffer));
    }
    lilv_instance_deactivate(instance);
    {
        LV2_State_Status save_status = value.state == NULL
            ? LV2_STATE_ERR_NO_FEATURE
            : value.state->save(
                instance->lv2_handle,
                state_store,
                &value,
                0U,
                NULL
            );
        LV2_State_Status restore_status = value.state == NULL
            ? LV2_STATE_ERR_NO_FEATURE
            : value.state->restore(
                instance->lv2_handle,
                state_retrieve,
                &value,
                0U,
                NULL
            );
        printf(
            "state_save_status=%u state_restore_status=%u "
            "state_properties=%u\n",
            (unsigned)save_status,
            (unsigned)restore_status,
            value.property_count
        );
    }
    lilv_instance_free(instance);
    lilv_node_free(uri);
    lilv_world_free(world);
    free_map(&value.map);
    printf(
        "parameter_write_sent=1 patch_response_events=%u "
        "output_atom_size=%u output_atom_type=%u finite_audio=%s\n",
        responses,
        ((const LV2_Atom *)output_buffer)->size,
        ((const LV2_Atom *)output_buffer)->type,
        finite ? "true" : "false"
    );
    return finite ? 0 : 1;
}

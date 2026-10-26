#include "music_rig/arturia_carla_host.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

typedef struct fake_carla {
    size_t opens;
    size_t closes;
    size_t idles;
    size_t finds;
    size_t midi_connections;
    size_t activates;
    size_t deactivates;
    size_t volumes;
    size_t reverbs;
    size_t notes;
    size_t controllers;
    size_t pitches;
    size_t master_volume;
    size_t master_mute;
    size_t connections_this_open;
    size_t deactivations_this_open;
    uint16_t last_patch;
    uint8_t last_value;
    bool fail_midi_connection;
    bool bad_startup_order;
} fake_carla;

static music_rig_result open_project(
    void *context, const char *path, const char *driver, const char *client
)
{
    fake_carla *fake = context;

    if (strcmp(path, "shared.uproject") != 0 || strcmp(driver, "wasapi") != 0 ||
        strcmp(client, "music-rig") != 0) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    fake->opens += 1U;
    fake->connections_this_open = 0U;
    fake->deactivations_this_open = 0U;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result close_project(void *context)
{
    ((fake_carla *)context)->closes += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static void idle(void *context)
{
    ((fake_carla *)context)->idles += 1U;
}

static music_rig_result find_patch(void *context, const char *name, uint16_t *id)
{
    fake_carla *fake = context;
    const char *digits;
    unsigned int value = 0U;

    if (name == NULL || id == NULL || strncmp(name, "patch-", 6U) != 0) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    digits = name + 6;
    if (*digits == '\0') {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    while (*digits != '\0') {
        unsigned int digit;

        if (*digits < '0' || *digits > '9') {
            return MUSIC_RIG_RESULT_NOT_FOUND;
        }
        digit = (unsigned int)(*digits - '0');
        if (value > ((unsigned int)UINT16_MAX - digit) / 10U) {
            return MUSIC_RIG_RESULT_NOT_FOUND;
        }
        value = value * 10U + digit;
        digits += 1;
    }
    fake->finds += 1U;
    *id = (uint16_t)value;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result connect_midi_input(void *context, uint16_t id)
{
    fake_carla *fake = context;

    fake->midi_connections += 1U;
    fake->connections_this_open += 1U;
    fake->last_patch = id;
    return fake->fail_midi_connection
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE : MUSIC_RIG_RESULT_OK;
}

static music_rig_result patch_call(
    void *context, uint16_t patch, uint8_t value, size_t *counter
)
{
    fake_carla *fake = context;

    *counter += 1U;
    fake->last_patch = patch;
    fake->last_value = value;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result activate(void *c, uint16_t p) {
    fake_carla *fake = c;
    if (fake->deactivations_this_open < 4U ||
        fake->connections_this_open < 4U) {
        fake->bad_startup_order = true;
    }
    return patch_call(c, p, 0U, &fake->activates);
}
static music_rig_result deactivate(void *c, uint16_t p) {
    fake_carla *fake = c;
    if (fake->connections_this_open < 4U) {
        fake->bad_startup_order = true;
    }
    fake->deactivations_this_open += 1U;
    return patch_call(c, p, 0U, &fake->deactivates);
}
static music_rig_result volume(void *c, uint16_t p, uint8_t v) {
    return patch_call(c, p, v, &((fake_carla *)c)->volumes);
}
static music_rig_result reverb(void *c, uint16_t p, uint8_t v) {
    return patch_call(c, p, v, &((fake_carla *)c)->reverbs);
}
static music_rig_result note(void *c, uint16_t p, bool on, uint8_t n, uint8_t v) {
    fake_carla *fake = c;
    (void)on; (void)n; (void)v;
    fake->notes += 1U; fake->last_patch = p;
    return MUSIC_RIG_RESULT_OK;
}
static music_rig_result controller(void *c, uint16_t p, uint8_t n, uint8_t v) {
    fake_carla *fake = c;
    (void)p; (void)n; (void)v; fake->controllers += 1U;
    return MUSIC_RIG_RESULT_OK;
}
static music_rig_result pitch(void *c, uint16_t p, uint16_t v) {
    fake_carla *fake = c;
    (void)p; (void)v; fake->pitches += 1U;
    return MUSIC_RIG_RESULT_OK;
}
static music_rig_result master_volume(void *c, uint8_t v) {
    fake_carla *fake = c; (void)v; fake->master_volume += 1U;
    return MUSIC_RIG_RESULT_OK;
}
static music_rig_result master_mute(void *c, bool v) {
    fake_carla *fake = c; (void)v; fake->master_mute += 1U;
    return MUSIC_RIG_RESULT_OK;
}

int main(void)
{
    const char *patch_names[4] = {"patch-10", "patch-11", "patch-12", "patch-13"};
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
        0U, 1U, 2U, 3U, 0U, 1U, 2U, 3U, 0U
    };
    const uint8_t organ[] = {8U};
    const uint8_t mode[] = {1U, 2U, 3U};
    uint8_t midi[3] = {UINT8_C(0xb0), UINT8_C(73), UINT8_C(55)};
    fake_carla fake = {0};
    music_rig_arturia_carla_host host;
    music_rig_arturia_carla_ops ops = {
        open_project, close_project, idle, find_patch, connect_midi_input,
        activate, deactivate, volume, reverb, note, controller, pitch,
        master_volume, master_mute
    };

    ops.idle = NULL;
    CHECK(music_rig_arturia_carla_host_open(
        &host, &ops, &fake, "shared.uproject", "wasapi", "music-rig",
        patch_names, 4U, patch_indexes, 4U, organ, sizeof(organ), 100U
    ) == MUSIC_RIG_RESULT_INVALID_ARGUMENT);
    CHECK(fake.opens == 0U);
    ops.idle = idle;
    fake.fail_midi_connection = true;
    CHECK(music_rig_arturia_carla_host_open(
        &host, &ops, &fake, "shared.uproject", "wasapi", "music-rig",
        patch_names, 4U, patch_indexes, 4U, organ, sizeof(organ), 100U
    ) == MUSIC_RIG_RESULT_ADAPTER_FAILURE);
    CHECK(fake.opens == 1U && fake.closes == 1U && fake.activates == 0U &&
        fake.deactivates == 0U);
    fake.fail_midi_connection = false;
    CHECK(music_rig_arturia_carla_host_open(
        &host, &ops, &fake, "shared.uproject", "wasapi", "music-rig",
        patch_names, 4U, patch_indexes, 4U, organ, sizeof(organ), 100U
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(fake.opens == 2U && fake.finds == 5U &&
        fake.midi_connections == 5U && fake.deactivates == 4U &&
        fake.activates == 1U && !fake.bad_startup_order);
    CHECK(music_rig_arturia_carla_host_idle(&host) == MUSIC_RIG_RESULT_OK &&
        fake.idles == 1U);
    CHECK(music_rig_arturia_carla_host_process_midi(&host, midi, sizeof(midi)) ==
        MUSIC_RIG_RESULT_OK && fake.volumes == 1U && fake.last_patch == 13U);
    CHECK(music_rig_arturia_carla_host_select_profile(
        &host, patch_indexes, 4U, mode, sizeof(mode)
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(fake.activates >= 3U && fake.reverbs >= 3U);
    midi[0] = UINT8_C(0x90); midi[1] = UINT8_C(60); midi[2] = UINT8_C(100);
    CHECK(music_rig_arturia_carla_host_process_midi(&host, midi, sizeof(midi)) ==
        MUSIC_RIG_RESULT_OK && fake.notes == 3U);
    CHECK(music_rig_arturia_carla_host_close(&host) == MUSIC_RIG_RESULT_OK);
    CHECK(fake.closes == 2U);
    CHECK(music_rig_arturia_carla_host_idle(&host) ==
        MUSIC_RIG_RESULT_INVALID_STATE);
    return 0;
}

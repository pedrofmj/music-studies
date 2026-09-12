#ifndef MUSIC_RIG_SYNTHV1_PRESET_ADAPTER_H
#define MUSIC_RIG_SYNTHV1_PRESET_ADAPTER_H

#include "music_rig/prepared_engine.h"

#include <stdbool.h>
#include <stddef.h>

typedef music_rig_result (*music_rig_synthv1_process_start_callback)(
    void *context,
    const char *preset_path
);
typedef music_rig_result (*music_rig_synthv1_process_stop_callback)(void *context);

typedef struct music_rig_synthv1_preset_adapter {
    const char *preset_path;
    const char *const *required_parameters;
    size_t required_parameter_count;
    bool staged;
    bool armed;
    bool committed;
    bool process_started;
    music_rig_synthv1_process_start_callback process_start;
    music_rig_synthv1_process_stop_callback process_stop;
    void *process_context;
} music_rig_synthv1_preset_adapter;

music_rig_result music_rig_synthv1_preset_adapter_init(
    music_rig_synthv1_preset_adapter *adapter,
    const char *preset_path,
    const char *const *required_parameters,
    size_t required_parameter_count
);

music_rig_result music_rig_synthv1_preset_adapter_interfaces(
    music_rig_synthv1_preset_adapter *adapter,
    music_rig_prepared_engine_adapter *interfaces
);

void music_rig_synthv1_preset_adapter_set_process_hooks(
    music_rig_synthv1_preset_adapter *adapter,
    void *context,
    music_rig_synthv1_process_start_callback start,
    music_rig_synthv1_process_stop_callback stop
);

#endif

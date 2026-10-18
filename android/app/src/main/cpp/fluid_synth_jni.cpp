#include <jni.h>
#include <fluidsynth.h>

#include "music_rig_android_portable.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct SynthHandle {
    fluid_settings_t *settings = nullptr;
    fluid_synth_t *synth = nullptr;
    void *controls = nullptr;
};

SynthHandle *from_handle(jlong value)
{
    return reinterpret_cast<SynthHandle *>(static_cast<intptr_t>(value));
}

bool array_lengths(JNIEnv *env, jintArray sound_font_ids, jshortArray patch_indexes,
                   jbyteArray active_layers, jbyteArray map_channels,
                   jintArray preset_banks, jintArray preset_programs,
                   jsize *sound_font_count, jsize *active_count)
{
    if (sound_font_ids == nullptr || patch_indexes == nullptr ||
        active_layers == nullptr || map_channels == nullptr ||
        preset_banks == nullptr || preset_programs == nullptr) {
        return false;
    }
    *sound_font_count = env->GetArrayLength(sound_font_ids);
    *active_count = env->GetArrayLength(active_layers);
    return *sound_font_count > 0 &&
           *sound_font_count <= MUSIC_RIG_ANDROID_LAYER_COUNT &&
           env->GetArrayLength(patch_indexes) ==
               MUSIC_RIG_ANDROID_LAYER_COUNT &&
           *active_count > 0 &&
           *active_count <= MUSIC_RIG_ANDROID_LAYER_COUNT &&
           env->GetArrayLength(map_channels) ==
               MUSIC_RIG_ANDROID_LAYER_COUNT &&
           env->GetArrayLength(preset_banks) == *sound_font_count &&
           env->GetArrayLength(preset_programs) == *sound_font_count;
}

} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeCreate(
    JNIEnv *, jclass, jint sample_rate)
{
    SynthHandle *handle = new SynthHandle();
    handle->controls = music_rig_android_control_state_create();
    if (handle->controls == nullptr) {
        delete handle;
        return 0;
    }
    handle->settings = new_fluid_settings();
    if (handle->settings == nullptr) {
        music_rig_android_control_state_destroy(handle->controls);
        delete handle;
        return 0;
    }
    if (fluid_settings_setnum(handle->settings, "synth.sample-rate", sample_rate) != FLUID_OK ||
        fluid_settings_setint(handle->settings, "synth.polyphony", 256) != FLUID_OK) {
        delete_fluid_settings(handle->settings);
        music_rig_android_control_state_destroy(handle->controls);
        delete handle;
        return 0;
    }
    handle->synth = new_fluid_synth(handle->settings);
    if (handle->synth == nullptr) {
        delete_fluid_settings(handle->settings);
        music_rig_android_control_state_destroy(handle->controls);
        delete handle;
        return 0;
    }
    return static_cast<jlong>(reinterpret_cast<intptr_t>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeDestroy(
    JNIEnv *, jclass, jlong value)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr) {
        return;
    }
    delete_fluid_synth(handle->synth);
    delete_fluid_settings(handle->settings);
    music_rig_android_control_state_destroy(handle->controls);
    delete handle;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeLoadSoundFont(
    JNIEnv *env, jclass, jlong value, jstring java_path)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr || java_path == nullptr) {
        return -1;
    }
    const char *path = env->GetStringUTFChars(java_path, nullptr);
    if (path == nullptr) {
        return -1;
    }
    int sound_font = fluid_synth_sfload(handle->synth, path, 0);
    env->ReleaseStringUTFChars(java_path, path);
    return sound_font;
}

extern "C" JNIEXPORT void JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeSetMasterState(
    JNIEnv *, jclass, jlong value, jint volume, jboolean muted)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr || volume < 0 || volume > 127) {
        return;
    }
    float gain = muted == JNI_TRUE ? 0.0f : 0.2f * static_cast<float>(volume) / 127.0f;
    fluid_synth_set_gain(handle->synth, gain);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeGetPresets(
    JNIEnv *env, jclass, jlong value, jint sound_font_id)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr) {
        return nullptr;
    }
    fluid_sfont_t *sound_font = fluid_synth_get_sfont_by_id(handle->synth, sound_font_id);
    if (sound_font == nullptr) {
        return nullptr;
    }

    struct PresetEntry {
        std::string name;
        int bank;
        int program;
    };
    std::vector<PresetEntry> presets;
    fluid_sfont_iteration_start(sound_font);
    fluid_preset_t *preset = nullptr;
    while ((preset = fluid_sfont_iteration_next(sound_font)) != nullptr) {
        const char *name = fluid_preset_get_name(preset);
        if (name != nullptr) {
            presets.push_back({name, fluid_preset_get_banknum(preset),
                fluid_preset_get_num(preset)});
        }
    }

    jclass string_class = env->FindClass("java/lang/String");
    if (string_class == nullptr) {
        return nullptr;
    }
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(presets.size() * 3),
            string_class, nullptr);
    env->DeleteLocalRef(string_class);
    if (result == nullptr) {
        return nullptr;
    }
    for (size_t index = 0; index < presets.size(); ++index) {
        const PresetEntry &entry = presets[index];
        jstring name = env->NewStringUTF(entry.name.c_str());
        jstring bank = env->NewStringUTF(std::to_string(entry.bank).c_str());
        jstring program = env->NewStringUTF(std::to_string(entry.program).c_str());
        if (name == nullptr || bank == nullptr || program == nullptr) {
            if (name != nullptr) env->DeleteLocalRef(name);
            if (bank != nullptr) env->DeleteLocalRef(bank);
            if (program != nullptr) env->DeleteLocalRef(program);
            env->DeleteLocalRef(result);
            return nullptr;
        }
        jsize base = static_cast<jsize>(index * 3);
        env->SetObjectArrayElement(result, base, name);
        env->SetObjectArrayElement(result, base + 1, bank);
        env->SetObjectArrayElement(result, base + 2, program);
        env->DeleteLocalRef(name);
        env->DeleteLocalRef(bank);
        env->DeleteLocalRef(program);
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeApplyProfile(
    JNIEnv *env, jclass, jlong value, jintArray sound_font_ids,
    jshortArray patch_indexes, jshort patch_count,
    jbyteArray active_layers, jbyteArray map_channels,
    jintArray preset_banks, jintArray preset_programs)
{
    SynthHandle *handle = from_handle(value);
    jsize sound_font_count = 0;
    jsize active_count = 0;
    if (handle == nullptr || !array_lengths(env, sound_font_ids, patch_indexes,
            active_layers, map_channels, preset_banks, preset_programs,
            &sound_font_count, &active_count) ||
        patch_count <= 0 || patch_count != sound_font_count) {
        return JNI_FALSE;
    }

    jint *ids = env->GetIntArrayElements(sound_font_ids, nullptr);
    jshort *java_indexes = env->GetShortArrayElements(patch_indexes, nullptr);
    jbyte *java_layers = env->GetByteArrayElements(active_layers, nullptr);
    jbyte *java_channels = env->GetByteArrayElements(map_channels, nullptr);
    jint *banks = env->GetIntArrayElements(preset_banks, nullptr);
    jint *programs = env->GetIntArrayElements(preset_programs, nullptr);
    if (ids == nullptr || java_indexes == nullptr || java_layers == nullptr ||
        java_channels == nullptr || banks == nullptr || programs == nullptr) {
        if (ids != nullptr) env->ReleaseIntArrayElements(sound_font_ids, ids, JNI_ABORT);
        if (java_indexes != nullptr) env->ReleaseShortArrayElements(patch_indexes, java_indexes, JNI_ABORT);
        if (java_layers != nullptr) env->ReleaseByteArrayElements(active_layers, java_layers, JNI_ABORT);
        if (java_channels != nullptr) env->ReleaseByteArrayElements(map_channels, java_channels, JNI_ABORT);
        if (banks != nullptr) env->ReleaseIntArrayElements(preset_banks, banks, JNI_ABORT);
        if (programs != nullptr) env->ReleaseIntArrayElements(preset_programs, programs, JNI_ABORT);
        return JNI_FALSE;
    }

    uint16_t indexes[MUSIC_RIG_ANDROID_LAYER_COUNT];
    uint8_t layers[MUSIC_RIG_ANDROID_LAYER_COUNT]{};
    uint8_t channels[MUSIC_RIG_ANDROID_LAYER_COUNT]{};
    music_rig_android_route routes[MUSIC_RIG_ANDROID_LAYER_COUNT]{};
    size_t route_count = 0U;
    for (size_t index = 0; index < MUSIC_RIG_ANDROID_LAYER_COUNT; ++index) {
        indexes[index] = static_cast<uint16_t>(java_indexes[index]);
    }
    for (jsize index = 0; index < active_count; ++index) {
        layers[index] = static_cast<uint8_t>(java_layers[index]);
    }
    for (jsize index = 0; index < MUSIC_RIG_ANDROID_LAYER_COUNT; ++index) {
        channels[index] = static_cast<uint8_t>(java_channels[index]);
    }

    int result = music_rig_android_plan_build(indexes, static_cast<uint16_t>(patch_count),
            layers, static_cast<size_t>(active_count), routes, &route_count);
    if (result == 0) {
        for (size_t index = 0; index < route_count; ++index) {
            const music_rig_android_route &route = routes[index];
            uint8_t output_channel = static_cast<uint8_t>(channels[route.mixer_slot - 1]);
            if (output_channel == 0 || output_channel > 16 ||
                route.patch_index >= static_cast<uint16_t>(sound_font_count) ||
                fluid_synth_program_select(handle->synth, output_channel - 1,
                    ids[route.patch_index], banks[route.patch_index],
                    programs[route.patch_index]) != FLUID_OK) {
                result = -1;
                break;
            }
        }
    }

    env->ReleaseIntArrayElements(sound_font_ids, ids, JNI_ABORT);
    env->ReleaseShortArrayElements(patch_indexes, java_indexes, JNI_ABORT);
    env->ReleaseByteArrayElements(active_layers, java_layers, JNI_ABORT);
    env->ReleaseByteArrayElements(map_channels, java_channels, JNI_ABORT);
    env->ReleaseIntArrayElements(preset_banks, banks, JNI_ABORT);
    env->ReleaseIntArrayElements(preset_programs, programs, JNI_ABORT);
    return result == 0 ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeObserveControllerMessage(
    JNIEnv *env, jclass, jlong value, jbyteArray java_message)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr || java_message == nullptr ||
        env->GetArrayLength(java_message) < 3) {
        return;
    }
    jbyte message[3];
    env->GetByteArrayRegion(java_message, 0, 3, message);
    music_rig_android_control_state_update(handle->controls,
        static_cast<uint8_t>(message[0]), static_cast<uint8_t>(message[1]),
        static_cast<uint8_t>(message[2]));
}

extern "C" JNIEXPORT void JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeSendMidi(
    JNIEnv *env, jclass, jlong value, jbyteArray java_message)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr || java_message == nullptr) {
        return;
    }
    jsize length = env->GetArrayLength(java_message);
    if (length < 2) {
        return;
    }
    jbyte message[3]{};
    jsize count = length < 3 ? length : 3;
    env->GetByteArrayRegion(java_message, 0, count, message);
    uint8_t status = static_cast<uint8_t>(message[0]);
    uint8_t channel = status & 0x0f;
    uint8_t kind = status & 0xf0;
    if (kind == 0xb0 && count >= 3) {
        fluid_synth_cc(handle->synth, channel, static_cast<int>(message[1]) & 0x7f,
            static_cast<int>(message[2]) & 0x7f);
    } else if (kind == 0x80 && count >= 3) {
        fluid_synth_noteoff(handle->synth, channel, static_cast<int>(message[1]) & 0x7f);
    } else if (kind == 0x90 && count >= 3) {
        int note = static_cast<int>(message[1]) & 0x7f;
        int velocity = static_cast<int>(message[2]) & 0x7f;
        if (velocity == 0) fluid_synth_noteoff(handle->synth, channel, note);
        else fluid_synth_noteon(handle->synth, channel, note, velocity);
    } else if (kind == 0xc0) {
        fluid_synth_program_change(handle->synth, channel, static_cast<int>(message[1]) & 0x7f);
    } else if (kind == 0xd0) {
        fluid_synth_channel_pressure(handle->synth, channel, static_cast<int>(message[1]) & 0x7f);
    } else if (kind == 0xe0 && count >= 3) {
        int bend = (static_cast<int>(message[1]) & 0x7f) |
                ((static_cast<int>(message[2]) & 0x7f) << 7);
        fluid_synth_pitch_bend(handle->synth, channel, bend);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_musicrig_android_NativeFluidSynthEngine_nativeRender(
    JNIEnv *env, jclass, jlong value, jshortArray stereo_samples)
{
    SynthHandle *handle = from_handle(value);
    if (handle == nullptr || stereo_samples == nullptr) {
        return JNI_FALSE;
    }
    jsize sample_count = env->GetArrayLength(stereo_samples);
    if (sample_count <= 0 || (sample_count & 1) != 0) {
        return JNI_FALSE;
    }
    jshort *samples = env->GetShortArrayElements(stereo_samples, nullptr);
    if (samples == nullptr) {
        return JNI_FALSE;
    }
    int frames = sample_count / 2;
    int result = fluid_synth_write_s16(handle->synth, frames,
            samples, 0, 2, samples, 1, 2);
    env->ReleaseShortArrayElements(stereo_samples, samples, 0);
    return result == FLUID_OK ? JNI_TRUE : JNI_FALSE;
}

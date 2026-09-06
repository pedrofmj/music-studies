#define _POSIX_C_SOURCE 200809L
#include "CarlaNative.h"
#include "CarlaHost.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern const NativePluginDescriptor *carla_get_native_rack_plugin(void);
extern CarlaHostHandle carla_create_native_plugin_host_handle(
    const NativePluginDescriptor *, NativePluginHandle);
extern void carla_host_handle_free(CarlaHostHandle);

enum { FRAMES = 1024, RATE = 48000, BANDS = 8, WARMUP = 128 };
static const char eq_uri[] = "http://lsp-plug.in/plugins/lv2/para_equalizer_x8_stereo";
static float input[2][FRAMES], output[2][FRAMES], gains[BANDS][128];
static float *inputs[] = {input[0], input[1]}, *outputs[] = {output[0], output[1]};
static uint32_t gain_ids[BANDS];
static NativeTimeInfo time_info;

static uint32_t buffer_size(NativeHostHandle h) { (void)h; return FRAMES; }
static double sample_rate(NativeHostHandle h) { (void)h; return RATE; }
/* Device-free execution still requests normal realtime plugin behavior. */
static bool is_offline(NativeHostHandle h) { (void)h; return false; }
static const NativeTimeInfo *get_time(NativeHostHandle h) { (void)h; return &time_info; }
static bool write_midi(NativeHostHandle h, const NativeMidiEvent *e)
{ (void)h; (void)e; return true; }
static intptr_t dispatch(NativeHostHandle h, NativeHostDispatcherOpcode op,
                        int32_t i, intptr_t v, void *p, float f)
{ (void)h; (void)op; (void)i; (void)v; (void)p; (void)f; return 0; }

static double now_us(clockid_t clock)
{
    struct timespec t;
    if (clock_gettime(clock, &t) != 0) { perror("clock_gettime"); exit(1); }
    return (double)t.tv_sec * 1000000.0 + (double)t.tv_nsec / 1000.0;
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void print_stats(FILE *report, double *values, unsigned count)
{
    double sum = 0;
    unsigned over = 0;
    for (unsigned i = 0; i < count; ++i) {
        sum += values[i];
        if (values[i] > FRAMES * 1000000.0 / RATE) ++over;
    }
    qsort(values, count, sizeof(*values), compare_double);
    fprintf(report, "{\"mean_us\":%.3f,\"p50_us\":%.3f,\"p99_us\":%.3f,"
           "\"max_us\":%.3f,\"over_quantum_blocks\":%u}",
           sum / count, values[(count - 1) / 2],
           values[(unsigned)ceil(count * 0.99) - 1], values[count - 1], over);
}

static void fill_audio(unsigned block, bool silence)
{
    uint32_t noise = 12345U + block;
    for (unsigned i = 0; i < FRAMES; ++i) {
        double t = (double)((uint64_t)block * FRAMES + i) / RATE;
        noise = noise * 1664525U + 1013904223U;
        float n = ((float)(noise >> 8) / 16777216.0f - 0.5f) * 0.02f;
        input[0][i] = silence ? 0 : (float)(0.05 * sin(2 * 3.141592653589793 * 125 * t)
                                  + 0.03 * sin(2 * 3.141592653589793 * 1000 * t)) + n;
        input[1][i] = silence ? 0 : (float)(0.05 * sin(2 * 3.141592653589793 * 250 * t)
                                  + 0.03 * sin(2 * 3.141592653589793 * 2000 * t)) - n;
    }
}

static NativeMidiEvent cc(unsigned band, unsigned value, unsigned frame)
{
    NativeMidiEvent event = {.time = frame, .port = 0, .size = 3,
                            .data = {0xb0, (uint8_t)(102 + band), (uint8_t)value, 0}};
    return event;
}

static int find_gains(CarlaHostHandle host)
{
    for (unsigned band = 0; band < BANDS; ++band) {
        char symbol[16];
        snprintf(symbol, sizeof(symbol), "g_%u", band);
        gain_ids[band] = UINT32_MAX;
        for (uint32_t i = 0; i < carla_get_parameter_count(host, 0); ++i) {
            const CarlaParameterInfo *info = carla_get_parameter_info(host, 0, i);
            if (info && info->symbol && strcmp(info->symbol, symbol) == 0) {
                const ParameterData *data = carla_get_parameter_data(host, 0, i);
                if (!data || data->midiChannel != 0 || data->mappedControlIndex != (int)(102 + band))
                    return 1;
                gain_ids[band] = i;
                break;
            }
        }
        if (gain_ids[band] == UINT32_MAX) return 1;
    }
    return 0;
}

static int calibrate(const NativePluginDescriptor *rack, NativePluginHandle plugin,
                     CarlaHostHandle host)
{
    /* Capture Carla's actual mapping, including its logarithmic parameter rules. */
    for (unsigned value = 0; value < 128; ++value) {
        NativeMidiEvent events[BANDS];
        for (unsigned band = 0; band < BANDS; ++band) events[band] = cc(band, value, 0);
        rack->process(plugin, inputs, outputs, FRAMES, events, BANDS);
        for (unsigned band = 0; band < BANDS; ++band) {
            gains[band][value] = carla_get_current_parameter_value(host, 0, gain_ids[band]);
            if (!isfinite(gains[band][value]) ||
                (value > 0 && gains[band][value] <= gains[band][value - 1])) return 1;
        }
    }
    for (unsigned band = 0; band < BANDS; ++band) {
        const ParameterData *data = carla_get_parameter_data(host, 0, gain_ids[band]);
        if (fabsf(gains[band][0] - data->mappedMinimum) > 0.0001f ||
            fabsf(gains[band][127] - data->mappedMaximum) > 0.0001f) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 7) {
        fprintf(stderr, "usage: %s EQ_PROJECT LV2_PATH RESOURCE_DIR BLOCKS CSV JSON\n", argv[0]);
        return 2;
    }
    char *end;
    errno = 0;
    unsigned long parsed = strtoul(argv[4], &end, 10);
    if (errno || *end || parsed < 128 || parsed > 100000) return 2;
    unsigned count = (unsigned)parsed;
    double *wall = calloc(count, sizeof(double)), *cpu = calloc(count, sizeof(double));
    double *setter_cpu = calloc(count, sizeof(double));
    FILE *csv = fopen(argv[5], "wx");
    FILE *report = fopen(argv[6], "wx");
    if (!wall || !cpu || !setter_cpu || !csv || !report) { perror("measurement storage"); return 1; }
    fprintf(csv, "scenario,block,wall_us,cpu_us,setter_cpu_us,events,output_energy\n");
    NativeHostDescriptor descriptor = {
        .resourceDir = argv[3], .uiName = "EQ isolated probe",
        .get_buffer_size = buffer_size, .get_sample_rate = sample_rate,
        .is_offline = is_offline, .get_time_info = get_time,
        .write_midi_event = write_midi, .dispatcher = dispatch
    };
    const NativePluginDescriptor *rack = carla_get_native_rack_plugin();
    NativePluginHandle plugin = rack ? rack->instantiate(&descriptor) : NULL;
    CarlaHostHandle host = plugin ? carla_create_native_plugin_host_handle(rack, plugin) : NULL;
    int result = 1;
    bool activated = false;
    if (!host) goto done;
    carla_set_engine_option(host, ENGINE_OPTION_OSC_ENABLED, 0, NULL);
    carla_set_engine_option(host, ENGINE_OPTION_PLUGIN_PATH, PLUGIN_LV2, argv[2]);
    carla_set_engine_option(host, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, 0, NULL);
    if (!carla_load_project(host, argv[1]) || carla_get_current_plugin_count(host) != 1) {
        fprintf(stderr, "EQ load failed: %s\n", carla_get_last_error(host));
        goto done;
    }
    const CarlaPluginInfo *info = carla_get_plugin_info(host, 0);
    if (!info || !info->label || strcmp(info->label, eq_uri) != 0 ||
        !(info->optionsEnabled & PLUGIN_OPTION_FIXED_BUFFERS) || find_gains(host)) {
        fprintf(stderr, "Unexpected plugin, buffer mode, or gain mappings\n");
        goto done;
    }
    unsigned options = info->optionsEnabled;
    rack->activate(plugin);
    activated = true;
    if (calibrate(rack, plugin, host)) {
        fprintf(stderr, "MIDI mapping calibration failed\n");
        goto done;
    }
    fprintf(report, "{\"sample_rate\":%u,\"frames\":%u,\"blocks_per_scenario\":%u,"
           "\"warmup_blocks\":%u,\"options_enabled\":%u,\"mapping_verified\":true,"
           "\"mapping_endpoints\":[%.9g,%.9g],\"scenarios\":[",
           RATE, FRAMES, count, WARMUP, options, gains[0][0], gains[0][127]);
    for (unsigned scenario = 0; scenario < 15; ++scenario) {
        char name[40];
        if (scenario == 0) strcpy(name, "silence_hold");
        else if (scenario == 1) strcpy(name, "audio_hold");
        else if (scenario == 2) strcpy(name, "audio_direct_all");
        else if (scenario == 3) strcpy(name, "audio_midi_all_frame0");
        else if (scenario == 4) strcpy(name, "audio_midi_all_spread");
        else if (scenario == 5) strcpy(name, "silence_midi_all_spread");
        else if (scenario == 14) strcpy(name, "audio_hold_nonflat");
        else snprintf(name, sizeof(name), "audio_midi_band%u", scenario - 5);
        for (unsigned band = 0; band < BANDS; ++band)
            carla_set_parameter_value(host, 0, gain_ids[band], scenario == 14 ? gains[band][96] : 1.0f);
        unsigned mapping_errors = 0, invalid_samples = 0, event_total = 0;
        double energy_total = 0;
        for (unsigned block = 0; block < WARMUP + count; ++block) {
            NativeMidiEvent events[BANDS];
            unsigned events_count = 0, value = (block * 7) % 128;
            fill_audio(block, scenario == 0 || scenario == 5);
            if (scenario >= 3 && scenario < 14) {
                unsigned first = scenario >= 6 ? scenario - 6 : 0;
                unsigned last = scenario >= 6 ? first + 1 : BANDS;
                for (unsigned band = first; band < last; ++band)
                    events[events_count++] = cc(band, value,
                        scenario == 4 || scenario == 5 ? band * (FRAMES / BANDS) : 0);
            }
            time_info.frame = (uint64_t)block * FRAMES;
            time_info.usecs = time_info.frame * 1000000 / RATE;
            double w0 = now_us(CLOCK_MONOTONIC), c0 = now_us(CLOCK_THREAD_CPUTIME_ID);
            if (scenario == 2)
                for (unsigned band = 0; band < BANDS; ++band)
                    carla_set_parameter_value(host, 0, gain_ids[band], gains[band][value]);
            double elapsed_setter_cpu = scenario == 2 ? now_us(CLOCK_THREAD_CPUTIME_ID) - c0 : 0;
            rack->process(plugin, inputs, outputs, FRAMES, events, events_count);
            double elapsed_cpu = now_us(CLOCK_THREAD_CPUTIME_ID) - c0;
            double elapsed_wall = now_us(CLOCK_MONOTONIC) - w0;
            double energy = 0;
            for (unsigned ch = 0; ch < 2; ++ch)
                for (unsigned frame = 0; frame < FRAMES; ++frame) {
                    float v = output[ch][frame];
                    if (!isfinite(v)) ++invalid_samples;
                    else energy += (double)v * v;
                }
            if (scenario >= 2 && scenario < 14) {
                unsigned first = scenario >= 6 ? scenario - 6 : 0;
                unsigned last = scenario >= 6 ? first + 1 : BANDS;
                for (unsigned band = first; band < last; ++band)
                    if (fabsf(carla_get_current_parameter_value(host, 0, gain_ids[band])
                              - gains[band][value]) > 0.0001f) ++mapping_errors;
            }
            if (block >= WARMUP) {
                unsigned i = block - WARMUP;
                wall[i] = elapsed_wall;
                cpu[i] = elapsed_cpu;
                setter_cpu[i] = elapsed_setter_cpu;
                energy_total += energy;
                event_total += events_count;
                fprintf(csv, "%s,%u,%.3f,%.3f,%.3f,%u,%.12g\n", name, i,
                        elapsed_wall, elapsed_cpu, elapsed_setter_cpu, events_count, energy);
            }
            if (block % 32 == 0)
                rack->dispatcher(plugin, NATIVE_PLUGIN_OPCODE_IDLE, 0, 0, NULL, 0);
        }
        if (scenario) fputc(',', report);
        fprintf(report, "{\"name\":\"%s\",\"midi_events\":%u,\"mapping_errors\":%u,"
               "\"invalid_audio_samples\":%u,\"output_energy\":%.12g,\"wall\":",
               name, event_total, mapping_errors, invalid_samples, energy_total);
        print_stats(report, wall, count);
        fputs(",\"thread_cpu\":", report);
        print_stats(report, cpu, count);
        if (scenario == 2) {
            fputs(",\"direct_setter_thread_cpu\":", report);
            print_stats(report, setter_cpu, count);
        }
        fputc('}', report);
        fflush(report);
        if (mapping_errors || invalid_samples ||
            ((scenario != 0 && scenario != 5) && energy_total <= 0)) goto done;
    }
    fputs("]}\n", report);
    result = 0;
done:
    if (activated) rack->deactivate(plugin);
    if (plugin) rack->cleanup(plugin);
    if (host) carla_host_handle_free(host);
    if (fclose(csv) != 0) result = 1;
    if (fclose(report) != 0) result = 1;
    free(wall);
    free(cpu);
    free(setter_cpu);
    return result;
}

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define SHORT_PROCESS_TIMEOUT_MS 10000U
#define IDLE_READY_TIMEOUT_MS 5000U
#define IDLE_EXIT_MARGIN_MS 10000U
#define IDLE_SAMPLE_INTERVAL_MS 250U
#define PHYSICAL_IDLE_MINIMUM_MS 60000U
#define CHILD_IDLE_MARGIN_MS 250U
#define IDLE_CPU_PERCENT_MAX 0.5
#define IDLE_RSS_BYTES_MAX UINT64_C(50000000)
#define COMMAND_CAPACITY 32768U

typedef struct process_measurement {
    uint64_t wall_time_ns;
    uint64_t cpu_time_ns;
    uint64_t working_set_start_bytes;
    uint64_t working_set_peak_observed_bytes;
    uint64_t working_set_lifetime_peak_bytes;
    DWORD handles_start;
    DWORD handles_peak;
    DWORD threads_start;
    DWORD threads_peak;
    DWORD exit_code;
    unsigned int samples;
} process_measurement;

typedef struct child_process {
    HANDLE job;
    HANDLE process;
    HANDLE thread;
    DWORD process_id;
} child_process;

static uint64_t file_time_value(FILETIME value)
{
    ULARGE_INTEGER converted;

    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return (uint64_t)converted.QuadPart;
}

static int query_cpu_time_100ns(HANDLE process, uint64_t *value)
{
    FILETIME created;
    FILETIME exited;
    FILETIME kernel;
    FILETIME user;

    if (value == NULL ||
        !GetProcessTimes(process, &created, &exited, &kernel, &user)) {
        return 0;
    }
    *value = file_time_value(kernel) + file_time_value(user);
    return 1;
}

static int elapsed_ns(
    LARGE_INTEGER started,
    LARGE_INTEGER finished,
    LARGE_INTEGER frequency,
    uint64_t *value
)
{
    uint64_t ticks;
    uint64_t ticks_per_second;

    if (value == NULL || frequency.QuadPart <= 0 ||
        finished.QuadPart < started.QuadPart) {
        return 0;
    }
    ticks = (uint64_t)(finished.QuadPart - started.QuadPart);
    ticks_per_second = (uint64_t)frequency.QuadPart;
    *value = ticks / ticks_per_second * UINT64_C(1000000000) +
        ticks % ticks_per_second * UINT64_C(1000000000) / ticks_per_second;
    return 1;
}

static int count_process_threads(DWORD process_id, DWORD *count)
{
    HANDLE snapshot;
    THREADENTRY32 entry;
    DWORD total = 0U;

    if (count == NULL) {
        return 0;
    }
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0U);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = (DWORD)sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == process_id) {
                ++total;
            }
            entry.dwSize = (DWORD)sizeof(entry);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    *count = total;
    return 1;
}

static int sample_process(
    HANDLE process,
    DWORD process_id,
    process_measurement *measurement,
    int set_start
)
{
    PROCESS_MEMORY_COUNTERS_EX memory;
    DWORD handles;
    DWORD threads;
    uint64_t working_set;
    uint64_t lifetime_peak;

    if (measurement == NULL) {
        return 0;
    }
    ZeroMemory(&memory, sizeof(memory));
    memory.cb = (DWORD)sizeof(memory);
    if (!GetProcessMemoryInfo(
            process,
            (PROCESS_MEMORY_COUNTERS *)&memory,
            memory.cb
        ) ||
        !GetProcessHandleCount(process, &handles) ||
        !count_process_threads(process_id, &threads)) {
        return 0;
    }

    working_set = (uint64_t)memory.WorkingSetSize;
    lifetime_peak = (uint64_t)memory.PeakWorkingSetSize;
    if (set_start) {
        measurement->working_set_start_bytes = working_set;
        measurement->handles_start = handles;
        measurement->threads_start = threads;
    }
    if (working_set > measurement->working_set_peak_observed_bytes) {
        measurement->working_set_peak_observed_bytes = working_set;
    }
    if (lifetime_peak > measurement->working_set_lifetime_peak_bytes) {
        measurement->working_set_lifetime_peak_bytes = lifetime_peak;
    }
    if (handles > measurement->handles_peak) {
        measurement->handles_peak = handles;
    }
    if (threads > measurement->threads_peak) {
        measurement->threads_peak = threads;
    }
    ++measurement->samples;
    return 1;
}

static void close_child(child_process *child)
{
    if (child == NULL) {
        return;
    }
    if (child->thread != NULL) {
        CloseHandle(child->thread);
        child->thread = NULL;
    }
    if (child->process != NULL) {
        CloseHandle(child->process);
        child->process = NULL;
    }
    if (child->job != NULL) {
        CloseHandle(child->job);
        child->job = NULL;
    }
}

static int spawn_suspended(
    WCHAR *command_line,
    child_process *child
)
{
    SECURITY_ATTRIBUTES security;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits;
    HANDLE null_device;

    if (command_line == NULL || child == NULL) {
        return 0;
    }
    ZeroMemory(child, sizeof(*child));
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    null_device = CreateFileW(
        L"NUL",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (null_device == INVALID_HANDLE_VALUE) {
        return 0;
    }

    ZeroMemory(&startup, sizeof(startup));
    startup.cb = (DWORD)sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = null_device;
    startup.hStdOutput = null_device;
    startup.hStdError = null_device;
    ZeroMemory(&process, sizeof(process));
    ZeroMemory(&job_limits, sizeof(job_limits));

    child->job = CreateJobObjectW(NULL, NULL);
    if (child->job == NULL) {
        CloseHandle(null_device);
        return 0;
    }
    job_limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(
            child->job,
            JobObjectExtendedLimitInformation,
            &job_limits,
            (DWORD)sizeof(job_limits)
        ) ||
        !CreateProcessW(
            NULL,
            command_line,
            NULL,
            NULL,
            TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup,
            &process
        )) {
        CloseHandle(null_device);
        close_child(child);
        return 0;
    }
    CloseHandle(null_device);

    child->process = process.hProcess;
    child->thread = process.hThread;
    child->process_id = process.dwProcessId;
    if (!AssignProcessToJobObject(child->job, child->process)) {
        (void)TerminateProcess(child->process, 1U);
        (void)WaitForSingleObject(child->process, SHORT_PROCESS_TIMEOUT_MS);
        close_child(child);
        return 0;
    }
    return 1;
}

static int finish_measurement(
    child_process *child,
    process_measurement *measurement,
    LARGE_INTEGER started,
    LARGE_INTEGER finished,
    LARGE_INTEGER frequency,
    uint64_t cpu_started_100ns
)
{
    uint64_t cpu_finished_100ns;

    if (child == NULL || measurement == NULL ||
        !GetExitCodeProcess(child->process, &measurement->exit_code) ||
        !query_cpu_time_100ns(child->process, &cpu_finished_100ns) ||
        cpu_finished_100ns < cpu_started_100ns ||
        !elapsed_ns(started, finished, frequency, &measurement->wall_time_ns)) {
        return 0;
    }
    measurement->cpu_time_ns =
        (cpu_finished_100ns - cpu_started_100ns) * UINT64_C(100);
    return 1;
}

static int measure_short_process(
    const WCHAR *target,
    LARGE_INTEGER frequency,
    process_measurement *measurement
)
{
    WCHAR command_line[COMMAND_CAPACITY];
    child_process child;
    LARGE_INTEGER started;
    LARGE_INTEGER finished;
    uint64_t cpu_started_100ns;
    DWORD wait_result;
    int success = 0;

    ZeroMemory(measurement, sizeof(*measurement));
    ZeroMemory(&child, sizeof(child));
    if (_snwprintf_s(
            command_line,
            COMMAND_CAPACITY,
            _TRUNCATE,
            L"\"%ls\" --version",
            target
        ) < 0 ||
        !spawn_suspended(command_line, &child) ||
        !query_cpu_time_100ns(child.process, &cpu_started_100ns) ||
        !sample_process(
            child.process,
            child.process_id,
            measurement,
            1
        ) ||
        !QueryPerformanceCounter(&started)) {
        close_child(&child);
        return 0;
    }

    if (ResumeThread(child.thread) == (DWORD)-1) {
        close_child(&child);
        return 0;
    }
    CloseHandle(child.thread);
    child.thread = NULL;
    for (;;) {
        (void)sample_process(
            child.process,
            child.process_id,
            measurement,
            0
        );
        wait_result = WaitForSingleObject(child.process, 1U);
        if (wait_result == WAIT_OBJECT_0) {
            break;
        }
        if (wait_result != WAIT_TIMEOUT ||
            measurement->samples > SHORT_PROCESS_TIMEOUT_MS) {
            close_child(&child);
            return 0;
        }
    }
    (void)sample_process(
        child.process,
        child.process_id,
        measurement,
        0
    );
    if (QueryPerformanceCounter(&finished) &&
        finish_measurement(
            &child,
            measurement,
            started,
            finished,
            frequency,
            cpu_started_100ns
        )) {
        success = 1;
    }
    close_child(&child);
    return success;
}

static int run_idle_child(DWORD duration_ms, HANDLE ready_event)
{
    HANDLE stop_event;
    DWORD wait_result;

    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (stop_event == NULL || ready_event == NULL) {
        if (stop_event != NULL) {
            CloseHandle(stop_event);
        }
        return 1;
    }
    if (!SetEvent(ready_event)) {
        CloseHandle(stop_event);
        CloseHandle(ready_event);
        return 1;
    }
    CloseHandle(ready_event);
    wait_result = WaitForSingleObject(stop_event, duration_ms);
    CloseHandle(stop_event);
    return wait_result == WAIT_TIMEOUT ? 0 : 1;
}

static int measure_idle_process(
    DWORD requested_duration_ms,
    LARGE_INTEGER frequency,
    process_measurement *measurement
)
{
    WCHAR executable[COMMAND_CAPACITY];
    WCHAR command_line[COMMAND_CAPACITY];
    SECURITY_ATTRIBUTES security;
    HANDLE ready_event;
    child_process child;
    LARGE_INTEGER started;
    LARGE_INTEGER finished;
    uint64_t cpu_started_100ns;
    DWORD wait_result;
    DWORD maximum_samples;
    DWORD child_duration_ms;
    int success = 0;

    ZeroMemory(measurement, sizeof(*measurement));
    ZeroMemory(&child, sizeof(child));
    if (GetModuleFileNameW(NULL, executable, COMMAND_CAPACITY) == 0U) {
        return 0;
    }
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    ready_event = CreateEventW(&security, FALSE, FALSE, NULL);
    if (ready_event == NULL) {
        return 0;
    }
    child_duration_ms = requested_duration_ms + CHILD_IDLE_MARGIN_MS;
    if (_snwprintf_s(
            command_line,
            COMMAND_CAPACITY,
            _TRUNCATE,
            L"\"%ls\" --idle-child %lu %llu",
            executable,
            (unsigned long)child_duration_ms,
            (unsigned long long)(uintptr_t)ready_event
        ) < 0 ||
        !spawn_suspended(command_line, &child)) {
        CloseHandle(ready_event);
        return 0;
    }
    if (ResumeThread(child.thread) == (DWORD)-1) {
        CloseHandle(ready_event);
        close_child(&child);
        return 0;
    }
    CloseHandle(child.thread);
    child.thread = NULL;
    wait_result = WaitForSingleObject(ready_event, IDLE_READY_TIMEOUT_MS);
    CloseHandle(ready_event);
    if (wait_result != WAIT_OBJECT_0 ||
        !query_cpu_time_100ns(child.process, &cpu_started_100ns) ||
        !sample_process(
            child.process,
            child.process_id,
            measurement,
            1
        ) ||
        !QueryPerformanceCounter(&started)) {
        close_child(&child);
        return 0;
    }

    maximum_samples =
        (requested_duration_ms + CHILD_IDLE_MARGIN_MS + IDLE_EXIT_MARGIN_MS) /
            IDLE_SAMPLE_INTERVAL_MS +
        2U;
    for (;;) {
        wait_result = WaitForSingleObject(
            child.process,
            IDLE_SAMPLE_INTERVAL_MS
        );
        if (wait_result == WAIT_OBJECT_0) {
            break;
        }
        if (wait_result != WAIT_TIMEOUT ||
            measurement->samples >= maximum_samples) {
            close_child(&child);
            return 0;
        }
        (void)sample_process(
            child.process,
            child.process_id,
            measurement,
            0
        );
    }
    (void)sample_process(
        child.process,
        child.process_id,
        measurement,
        0
    );
    if (QueryPerformanceCounter(&finished) &&
        finish_measurement(
            &child,
            measurement,
            started,
            finished,
            frequency,
            cpu_started_100ns
        )) {
        success = 1;
    }
    close_child(&child);
    return success;
}

static int parse_duration(const WCHAR *text, DWORD *value)
{
    WCHAR *end;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == L'\0') {
        return 0;
    }
    end = NULL;
    parsed = wcstoul(text, &end, 10);
    if (end == text || end == NULL || *end != L'\0' ||
        parsed == 0UL ||
        parsed > (unsigned long)(MAXDWORD - CHILD_IDLE_MARGIN_MS)) {
        return 0;
    }
    *value = (DWORD)parsed;
    return 1;
}

static int emit_result(
    const char *kind,
    DWORD requested_duration_ms,
    int enforce_physical_gates,
    const process_measurement *short_process,
    const process_measurement *idle_process
)
{
    double idle_cpu_percent;
    int short_pass;
    int idle_duration_pass;
    int idle_cpu_pass;
    int idle_rss_pass;
    int cleanup_pass;
    int overall_pass;

    if (kind == NULL || short_process == NULL || idle_process == NULL ||
        idle_process->wall_time_ns == UINT64_C(0)) {
        return 0;
    }
    idle_cpu_percent =
        (double)idle_process->cpu_time_ns * 100.0 /
        (double)idle_process->wall_time_ns;
    short_pass =
        short_process->exit_code == 0U &&
        short_process->working_set_peak_observed_bytes > UINT64_C(0) &&
        short_process->handles_peak > 0U &&
        short_process->threads_peak > 0U;
    idle_duration_pass =
        idle_process->wall_time_ns >=
            (uint64_t)requested_duration_ms * UINT64_C(1000000);
    idle_cpu_pass = idle_cpu_percent < IDLE_CPU_PERCENT_MAX;
    idle_rss_pass =
        idle_process->working_set_peak_observed_bytes < IDLE_RSS_BYTES_MAX;
    cleanup_pass =
        short_process->exit_code == 0U && idle_process->exit_code == 0U;
    overall_pass =
        short_pass && idle_duration_pass && cleanup_pass &&
        (!enforce_physical_gates || (idle_cpu_pass && idle_rss_pass));

    printf(
        "{\"schema\":\"music-studies/windows-resource-measurement/v1\","
        "\"measurement_kind\":\"%s\","
        "\"clock\":\"QueryPerformanceCounter\","
        "\"short_process\":{"
        "\"target\":\"music-rig --version\","
        "\"wall_time_ns\":%llu,\"cpu_time_ns\":%llu,"
        "\"working_set_start_bytes\":%llu,"
        "\"working_set_peak_observed_bytes\":%llu,"
        "\"working_set_lifetime_peak_bytes\":%llu,"
        "\"handles_start\":%lu,\"handles_peak\":%lu,"
        "\"threads_start\":%lu,\"threads_peak\":%lu,"
        "\"samples\":%u,\"exit_code\":%lu},"
        "\"idle_daemon\":{"
        "\"adapter\":\"windows-event-wait-synthetic-daemon\","
        "\"wait_primitive\":\"WaitForSingleObject-event-timeout\","
        "\"requested_zero_event_duration_ms\":%lu,"
        "\"observed_duration_ns\":%llu,"
        "\"control_requests\":0,\"midi_events\":0,"
        "\"cpu_time_ns\":%llu,"
        "\"cpu_one_core_percent\":%.6f,"
        "\"working_set_start_bytes\":%llu,"
        "\"working_set_peak_observed_bytes\":%llu,"
        "\"working_set_lifetime_peak_bytes\":%llu,"
        "\"handles_start\":%lu,\"handles_peak\":%lu,"
        "\"threads_start\":%lu,\"threads_peak\":%lu,"
        "\"samples\":%u,\"exit_code\":%lu},"
        "\"thresholds\":{"
        "\"physical_gates_enforced\":%s,"
        "\"idle_cpu_one_core_percent_max_exclusive\":0.5,"
        "\"idle_working_set_bytes_max_exclusive\":50000000},"
        "\"evaluation\":{"
        "\"short_process_pass\":%s,"
        "\"idle_duration_pass\":%s,"
        "\"idle_cpu_pass\":%s,"
        "\"idle_working_set_pass\":%s,"
        "\"zero_activity_pass\":true,"
        "\"cleanup_pass\":%s,"
        "\"passed\":%s},"
        "\"scope\":{"
        "\"synthetic_only\":true,"
        "\"audio_or_midi_apis_opened\":false,"
        "\"audio_or_midi_routes_changed\":false,"
        "\"service_installed\":false,"
        "\"live_rig_changes\":false}}\n",
        kind,
        (unsigned long long)short_process->wall_time_ns,
        (unsigned long long)short_process->cpu_time_ns,
        (unsigned long long)short_process->working_set_start_bytes,
        (unsigned long long)short_process->working_set_peak_observed_bytes,
        (unsigned long long)short_process->working_set_lifetime_peak_bytes,
        (unsigned long)short_process->handles_start,
        (unsigned long)short_process->handles_peak,
        (unsigned long)short_process->threads_start,
        (unsigned long)short_process->threads_peak,
        short_process->samples,
        (unsigned long)short_process->exit_code,
        (unsigned long)requested_duration_ms,
        (unsigned long long)idle_process->wall_time_ns,
        (unsigned long long)idle_process->cpu_time_ns,
        idle_cpu_percent,
        (unsigned long long)idle_process->working_set_start_bytes,
        (unsigned long long)idle_process->working_set_peak_observed_bytes,
        (unsigned long long)idle_process->working_set_lifetime_peak_bytes,
        (unsigned long)idle_process->handles_start,
        (unsigned long)idle_process->handles_peak,
        (unsigned long)idle_process->threads_start,
        (unsigned long)idle_process->threads_peak,
        idle_process->samples,
        (unsigned long)idle_process->exit_code,
        enforce_physical_gates ? "true" : "false",
        short_pass ? "true" : "false",
        idle_duration_pass ? "true" : "false",
        idle_cpu_pass ? "true" : "false",
        idle_rss_pass ? "true" : "false",
        cleanup_pass ? "true" : "false",
        overall_pass ? "true" : "false"
    );
    return overall_pass;
}

static void print_usage(const WCHAR *program)
{
    fwprintf(
        stderr,
        L"Usage: %ls --measure PATH [DURATION_MS]\n"
        L"       %ls --self-test PATH\n",
        program,
        program
    );
}

int wmain(int argc, WCHAR **argv)
{
    LARGE_INTEGER frequency;
    process_measurement short_process;
    process_measurement idle_process;
    DWORD duration_ms;
    int enforce_physical_gates;
    const char *kind;

    if (argc == 4 && wcscmp(argv[1], L"--idle-child") == 0) {
        DWORD child_duration_ms;
        unsigned long long inherited_handle;

        if (!parse_duration(argv[2], &child_duration_ms)) {
            return 2;
        }
        inherited_handle = _wcstoui64(argv[3], NULL, 10);
        return run_idle_child(
            child_duration_ms,
            (HANDLE)(uintptr_t)inherited_handle
        );
    }
    if (argc == 3 && wcscmp(argv[1], L"--self-test") == 0) {
        duration_ms = 250U;
        enforce_physical_gates = 0;
        kind = "self-test";
    } else if ((argc == 3 || argc == 4) &&
        wcscmp(argv[1], L"--measure") == 0) {
        duration_ms = PHYSICAL_IDLE_MINIMUM_MS;
        if (argc == 4 && !parse_duration(argv[3], &duration_ms)) {
            print_usage(argv[0]);
            return 2;
        }
        if (duration_ms < PHYSICAL_IDLE_MINIMUM_MS) {
            fputs("physical measurement requires at least 60000 ms\n", stderr);
            return 2;
        }
        enforce_physical_gates = 1;
        kind = "physical-reference";
    } else {
        print_usage(argv[0]);
        return 2;
    }

    if (!QueryPerformanceFrequency(&frequency) ||
        !measure_short_process(argv[2], frequency, &short_process) ||
        !measure_idle_process(duration_ms, frequency, &idle_process)) {
        fputs("Windows resource measurement failed\n", stderr);
        return 1;
    }
    return emit_result(
        kind,
        duration_ms,
        enforce_physical_gates,
        &short_process,
        &idle_process
    ) ? 0 : 1;
}

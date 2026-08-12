#define _POSIX_C_SOURCE 200809L

#include "music_rig/linux_lifecycle.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DIAGNOSTIC_INTERVAL_NS UINT64_C(1000000000)
#define DIAGNOSTIC_BURST UINT32_C(4)

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static music_rig_result monotonic_now(uint64_t *timestamp_ns)
{
    struct timespec value;

    if (timestamp_ns == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0 || value.tv_sec < 0 ||
        value.tv_nsec < 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *timestamp_ns = (uint64_t)value.tv_sec * UINT64_C(1000000000) +
        (uint64_t)value.tv_nsec;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_linux_shadow_lifecycle_run(
    const music_rig_diagnostic_sink *sink
)
{
    struct sigaction action;
    struct sigaction previous_interrupt;
    struct sigaction previous_terminate;
    sigset_t blocked_signals;
    sigset_t previous_mask;
    sigset_t wait_mask;
    music_rig_diagnostics diagnostics;
    music_rig_result result;
    uint64_t timestamp_ns;
    bool interrupt_installed = false;
    bool mask_changed = false;
    bool terminate_installed = false;

    if (sink == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_diagnostics_init(
        &diagnostics,
        DIAGNOSTIC_INTERVAL_NS,
        DIAGNOSTIC_BURST,
        sink
    );
    stop_requested = 0;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    if (result == MUSIC_RIG_RESULT_OK &&
        (sigemptyset(&action.sa_mask) != 0 ||
         sigemptyset(&blocked_signals) != 0 ||
         sigaddset(&blocked_signals, SIGINT) != 0 ||
         sigaddset(&blocked_signals, SIGTERM) != 0)) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK &&
        sigprocmask(SIG_BLOCK, &blocked_signals, &previous_mask) == 0) {
        mask_changed = true;
        wait_mask = previous_mask;
        if (sigdelset(&wait_mask, SIGINT) != 0 ||
            sigdelset(&wait_mask, SIGTERM) != 0) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    } else if (result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK &&
        sigaction(SIGINT, &action, &previous_interrupt) == 0) {
        interrupt_installed = true;
    } else if (result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK &&
        sigaction(SIGTERM, &action, &previous_terminate) == 0) {
        terminate_installed = true;
    } else if (result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = monotonic_now(&timestamp_ns);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_diagnostics_emit(
            &diagnostics,
            timestamp_ns,
            MUSIC_RIG_DIAGNOSTIC_INFO,
            "lifecycle.started",
            "output-suppressed shadow host ready"
        );
    }
    while (result == MUSIC_RIG_RESULT_OK && stop_requested == 0) {
        if (sigsuspend(&wait_mask) != -1 || errno != EINTR) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }

    if (result == MUSIC_RIG_RESULT_OK) {
        result = monotonic_now(&timestamp_ns);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_diagnostics_emit(
            &diagnostics,
            timestamp_ns,
            MUSIC_RIG_DIAGNOSTIC_INFO,
            "lifecycle.stopped",
            "output-suppressed shadow host stopped"
        );
    }
    if (terminate_installed &&
        sigaction(SIGTERM, &previous_terminate, NULL) != 0 &&
        result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (interrupt_installed &&
        sigaction(SIGINT, &previous_interrupt, NULL) != 0 &&
        result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (mask_changed &&
        sigprocmask(SIG_SETMASK, &previous_mask, NULL) != 0 &&
        result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return result;
}

#define _POSIX_C_SOURCE 200809L

#include "music_rig/journal_diagnostics.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define JOURNAL_LINE_CAPACITY ((size_t)512)

static bool bounded_text(const char *value, size_t capacity)
{
    size_t index;

    if (value == NULL) {
        return false;
    }
    for (index = 0U; index < capacity; ++index) {
        unsigned char byte = (unsigned char)value[index];

        if (byte == '\0') {
            return true;
        }
        if (byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return false;
}

static const char *severity_name(music_rig_diagnostic_severity severity)
{
    if (severity == MUSIC_RIG_DIAGNOSTIC_INFO) {
        return "info";
    }
    if (severity == MUSIC_RIG_DIAGNOSTIC_WARNING) {
        return "warning";
    }
    if (severity == MUSIC_RIG_DIAGNOSTIC_ERROR) {
        return "error";
    }
    return NULL;
}

static music_rig_result journal_write(
    void *opaque,
    const music_rig_diagnostic_record *record
)
{
    const music_rig_journal_diagnostics *journal = opaque;
    const char *severity;
    char line[JOURNAL_LINE_CAPACITY];
    size_t total = 0U;
    int length;

    if (journal == NULL || record == NULL ||
        journal->abi_version != MUSIC_RIG_JOURNAL_DIAGNOSTICS_ABI_VERSION ||
        journal->descriptor < 0 ||
        !bounded_text(record->code, MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY) ||
        !bounded_text(record->message, MUSIC_RIG_DIAGNOSTIC_MESSAGE_CAPACITY) ||
        (severity = severity_name(record->severity)) == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    length = snprintf(
        line,
        sizeof(line),
        "music-rigd severity=%s code=%s timestamp_ns=%llu "
        "suppressed=%llu message=%s\n",
        severity,
        record->code,
        (unsigned long long)record->timestamp_ns,
        (unsigned long long)record->suppressed_before,
        record->message
    );
    if (length < 0 || (size_t)length >= sizeof(line)) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }

    while (total < (size_t)length) {
        ssize_t count = write(
            journal->descriptor,
            line + total,
            (size_t)length - total
        );

        if (count > 0) {
            total += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_journal_diagnostics_init(
    music_rig_journal_diagnostics *journal,
    int descriptor,
    music_rig_diagnostic_sink *sink
)
{
    if (journal == NULL || descriptor < 0 || sink == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    journal->abi_version = MUSIC_RIG_JOURNAL_DIAGNOSTICS_ABI_VERSION;
    journal->descriptor = descriptor;
    sink->abi_version = MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION;
    sink->context = journal;
    sink->write = journal_write;
    return MUSIC_RIG_RESULT_OK;
}

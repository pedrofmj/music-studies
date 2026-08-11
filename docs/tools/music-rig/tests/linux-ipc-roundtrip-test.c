#include "music_rig/control.h"
#include "music_rig/protocol.h"
#include "compiled-tables-fixture.h"
#include "protocol-golden.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define ROUND_TRIPS ((size_t)1000)
#define EXPECTED_GENERATION UINT64_C(41)
#define GOLDEN_REQUEST_ID UINT64_C(73)
#define ROUND_TRIP_LIMIT_NS UINT64_C(20000000)

typedef struct server_context {
    int descriptor;
    int failed;
    music_rig_control_snapshot snapshot;
} server_context;

static uint64_t monotonic_ns(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &value) != 0) {
        return UINT64_C(0);
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
        (uint64_t)value.tv_nsec;
}

static void build_request(size_t index, music_rig_protocol_request *request)
{
    memset(request, 0, sizeof(*request));
    request->protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    request->request_id = index == 0U
        ? GOLDEN_REQUEST_ID
        : (uint64_t)index + UINT64_C(1000);
    request->expected_generation = EXPECTED_GENERATION;
    switch (index % 4U) {
    case 0U:
        request->operation = (uint32_t)MUSIC_RIG_OPERATION_STATUS;
        break;
    case 1U:
        request->operation = (uint32_t)MUSIC_RIG_OPERATION_LIST_PROFILES;
        fixture_copy(request->device_slot, "smc-mixer-main");
        break;
    case 2U:
        request->operation = (uint32_t)MUSIC_RIG_OPERATION_VALIDATE_ACTIVE;
        break;
    default:
        request->operation = (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE;
        request->flags = MUSIC_RIG_REQUEST_DRY_RUN;
        fixture_copy(request->device_slot, "smc-mixer-main");
        fixture_copy(request->profile, "eight-band-eq");
        break;
    }
}

static void *serve_requests(void *opaque)
{
    server_context *context = opaque;
    size_t index;

    for (index = 0; index < ROUND_TRIPS; ++index) {
        uint8_t request_bytes[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
        uint8_t response_bytes[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
        music_rig_protocol_request request;
        music_rig_protocol_response response;
        ssize_t received;
        ssize_t sent;

        received = recv(context->descriptor, request_bytes,
            sizeof(request_bytes), 0);
        if (received != (ssize_t)sizeof(request_bytes) ||
            (index == 0U && memcmp(
                request_bytes,
                MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN_PREFIX,
                MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN_PREFIX_SIZE
            ) != 0) ||
            music_rig_protocol_decode_request(
                request_bytes,
                sizeof(request_bytes),
                &request
            ) != MUSIC_RIG_RESULT_OK ||
            music_rig_control_dispatch(
                &context->snapshot,
                &request,
                &response
            ) != MUSIC_RIG_RESULT_OK ||
            response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
            music_rig_protocol_encode_response(
                &response,
                response_bytes,
                sizeof(response_bytes)
            ) != MUSIC_RIG_RESULT_OK ||
            (index == 0U && memcmp(
                response_bytes,
                MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN_PREFIX,
                MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN_PREFIX_SIZE
            ) != 0)) {
            context->failed = 1;
            return NULL;
        }

        sent = send(context->descriptor, response_bytes,
            sizeof(response_bytes), 0);
        if (sent != (ssize_t)sizeof(response_bytes)) {
            context->failed = 1;
            return NULL;
        }
    }
    return NULL;
}

static int compare_u64(const void *left, const void *right)
{
    const uint64_t left_value = *(const uint64_t *)left;
    const uint64_t right_value = *(const uint64_t *)right;

    if (left_value < right_value) {
        return -1;
    }
    return left_value > right_value;
}

int main(void)
{
    static music_rig_compiled_tables tables;
    int descriptors[2];
    pthread_t server_thread;
    server_context context;
    uint64_t latencies[ROUND_TRIPS];
    size_t index;

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK ||
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, descriptors) != 0) {
        fputs("could not initialize Linux mock control transport\n", stderr);
        return 1;
    }

    memset(&context, 0, sizeof(context));
    context.descriptor = descriptors[1];
    context.snapshot.generation_id = EXPECTED_GENERATION;
    context.snapshot.active_rig_profile = "full-live-rack";
    context.snapshot.tables = &tables;
    context.snapshot.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    if (pthread_create(&server_thread, NULL, serve_requests, &context) != 0) {
        fputs("could not start IPC server thread\n", stderr);
        return 1;
    }

    for (index = 0; index < ROUND_TRIPS; ++index) {
        music_rig_protocol_request request;
        music_rig_protocol_response response;
        uint8_t request_bytes[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
        uint8_t response_bytes[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
        uint64_t started_ns;
        uint64_t finished_ns;
        ssize_t transferred;

        build_request(index, &request);
        if (music_rig_protocol_encode_request(
                &request,
                request_bytes,
                sizeof(request_bytes)
            ) != MUSIC_RIG_RESULT_OK) {
            return 1;
        }
        started_ns = monotonic_ns();
        transferred = send(descriptors[0], request_bytes,
            sizeof(request_bytes), 0);
        if (transferred != (ssize_t)sizeof(request_bytes)) {
            return 1;
        }
        transferred = recv(descriptors[0], response_bytes,
            sizeof(response_bytes), 0);
        finished_ns = monotonic_ns();
        if (transferred != (ssize_t)sizeof(response_bytes) ||
            music_rig_protocol_decode_response(
                response_bytes,
                sizeof(response_bytes),
                &response
            ) != MUSIC_RIG_RESULT_OK ||
            response.request_id != request.request_id ||
            response.operation != request.operation ||
            response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
            response.previous_generation != EXPECTED_GENERATION ||
            response.resulting_generation != EXPECTED_GENERATION) {
            fputs("invalid IPC response\n", stderr);
            return 1;
        }
        latencies[index] = finished_ns - started_ns;
    }

    if (pthread_join(server_thread, NULL) != 0 || context.failed != 0) {
        fputs("IPC server failed\n", stderr);
        return 1;
    }
    close(descriptors[0]);
    close(descriptors[1]);

    qsort(latencies, ROUND_TRIPS, sizeof(latencies[0]), compare_u64);
    if (latencies[989] >= ROUND_TRIP_LIMIT_NS) {
        fputs("IPC p99 round trip exceeded 20 ms\n", stderr);
        return 1;
    }
    printf(
        "{\"schema\":\"music-studies/ipc-round-trip-spike/v1\","
        "\"transport\":\"linux-sock-seqpacket-mock-control\","
        "\"golden_frames\":true,"
        "\"round_trips\":%zu,\"p50_ns\":%" PRIu64 ","
        "\"p95_ns\":%" PRIu64 ",\"p99_ns\":%" PRIu64 ","
        "\"maximum_ns\":%" PRIu64 ",\"failures\":0}\n",
        ROUND_TRIPS,
        latencies[499],
        latencies[949],
        latencies[989],
        latencies[999]
    );
    return 0;
}

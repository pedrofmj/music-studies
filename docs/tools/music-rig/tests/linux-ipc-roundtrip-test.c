#include "music_rig/protocol.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define ROUND_TRIPS ((size_t)1000)
#define EXPECTED_GENERATION UINT64_C(41)
#define ROUND_TRIP_LIMIT_NS UINT64_C(20000000)

typedef struct server_context {
    int descriptor;
    int failed;
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

        received = recv(
            context->descriptor,
            request_bytes,
            sizeof(request_bytes),
            0
        );
        if (received != (ssize_t)sizeof(request_bytes) ||
            music_rig_protocol_decode_request(
                request_bytes,
                sizeof(request_bytes),
                &request
            ) != MUSIC_RIG_RESULT_OK ||
            request.operation != (uint32_t)MUSIC_RIG_OPERATION_STATUS) {
            context->failed = 1;
            return NULL;
        }

        response.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
        response.result_code = (uint32_t)MUSIC_RIG_RESULT_OK;
        response.request_id = request.request_id;
        response.previous_generation = EXPECTED_GENERATION;
        response.resulting_generation = EXPECTED_GENERATION;
        if (music_rig_protocol_encode_response(
                &response,
                response_bytes,
                sizeof(response_bytes)
            ) != MUSIC_RIG_RESULT_OK) {
            context->failed = 1;
            return NULL;
        }

        sent = send(
            context->descriptor,
            response_bytes,
            sizeof(response_bytes),
            0
        );
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
    int descriptors[2];
    pthread_t server_thread;
    server_context context;
    uint64_t latencies[ROUND_TRIPS];
    size_t index;

    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, descriptors) != 0) {
        fputs("could not create SOCK_SEQPACKET pair\n", stderr);
        return 1;
    }

    context.descriptor = descriptors[1];
    context.failed = 0;
    if (pthread_create(&server_thread, NULL, serve_requests, &context) != 0) {
        fputs("could not start IPC server thread\n", stderr);
        return 1;
    }

    for (index = 0; index < ROUND_TRIPS; ++index) {
        const music_rig_protocol_request request = {
            MUSIC_RIG_PROTOCOL_VERSION,
            (uint32_t)MUSIC_RIG_OPERATION_STATUS,
            (uint64_t)index + UINT64_C(1),
            EXPECTED_GENERATION
        };
        music_rig_protocol_response response;
        uint8_t request_bytes[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
        uint8_t response_bytes[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
        uint64_t started_ns;
        uint64_t finished_ns;
        ssize_t transferred;

        if (music_rig_protocol_encode_request(
                &request,
                request_bytes,
                sizeof(request_bytes)
            ) != MUSIC_RIG_RESULT_OK) {
            fputs("could not encode IPC request\n", stderr);
            return 1;
        }

        started_ns = monotonic_ns();
        transferred = send(
            descriptors[0],
            request_bytes,
            sizeof(request_bytes),
            0
        );
        if (transferred != (ssize_t)sizeof(request_bytes)) {
            fputs("could not send complete IPC request\n", stderr);
            return 1;
        }

        transferred = recv(
            descriptors[0],
            response_bytes,
            sizeof(response_bytes),
            0
        );
        finished_ns = monotonic_ns();
        if (transferred != (ssize_t)sizeof(response_bytes) ||
            music_rig_protocol_decode_response(
                response_bytes,
                sizeof(response_bytes),
                &response
            ) != MUSIC_RIG_RESULT_OK ||
            response.request_id != request.request_id ||
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
        "\"transport\":\"linux-sock-seqpacket\","
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

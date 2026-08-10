#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>

#include "music_rig/protocol.h"
#include "protocol-golden.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define ROUND_TRIPS ((size_t)1000)
#define EXPECTED_GENERATION UINT64_C(41)
#define GOLDEN_REQUEST_ID UINT64_C(73)
#define ROUND_TRIP_LIMIT_NS UINT64_C(20000000)
#define SERVER_WAIT_MS 5000U

typedef struct server_context {
    HANDLE pipe;
    volatile LONG failed;
} server_context;

static DWORD fail_server(server_context *context)
{
    InterlockedExchange(&context->failed, 1L);
    DisconnectNamedPipe(context->pipe);
    return 1U;
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

    if (finished.QuadPart < started.QuadPart || frequency.QuadPart <= 0 ||
        value == NULL) {
        return 0;
    }

    ticks = (uint64_t)(finished.QuadPart - started.QuadPart);
    ticks_per_second = (uint64_t)frequency.QuadPart;
    *value = ticks / ticks_per_second * UINT64_C(1000000000) +
        ticks % ticks_per_second * UINT64_C(1000000000) / ticks_per_second;
    return 1;
}

static HANDLE create_server_pipe(const wchar_t *pipe_name)
{
    HANDLE process_token = NULL;
    TOKEN_USER *token_user = NULL;
    DWORD token_user_size = 0U;
    EXPLICIT_ACCESSW access;
    PACL access_control_list = NULL;
    SECURITY_DESCRIPTOR security_descriptor;
    SECURITY_ATTRIBUTES security_attributes;
    HANDLE pipe = INVALID_HANDLE_VALUE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
        goto cleanup;
    }
    if (GetTokenInformation(
            process_token,
            TokenUser,
            NULL,
            0U,
            &token_user_size
        ) || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        goto cleanup;
    }

    token_user = malloc((size_t)token_user_size);
    if (token_user == NULL ||
        !GetTokenInformation(
            process_token,
            TokenUser,
            token_user,
            token_user_size,
            &token_user_size
        )) {
        goto cleanup;
    }

    ZeroMemory(&access, sizeof(access));
    access.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = token_user->User.Sid;
    if (SetEntriesInAclW(
            1U,
            &access,
            NULL,
            &access_control_list
        ) != ERROR_SUCCESS ||
        !InitializeSecurityDescriptor(
            &security_descriptor,
            SECURITY_DESCRIPTOR_REVISION
        ) ||
        !SetSecurityDescriptorDacl(
            &security_descriptor,
            TRUE,
            access_control_list,
            FALSE
        )) {
        goto cleanup;
    }

    security_attributes.nLength = (DWORD)sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = &security_descriptor;
    security_attributes.bInheritHandle = FALSE;
    pipe = CreateNamedPipeW(
        pipe_name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS,
        1U,
        (DWORD)MUSIC_RIG_PROTOCOL_RESPONSE_SIZE,
        (DWORD)MUSIC_RIG_PROTOCOL_REQUEST_SIZE,
        0U,
        &security_attributes
    );

cleanup:
    if (access_control_list != NULL) {
        LocalFree(access_control_list);
    }
    free(token_user);
    if (process_token != NULL) {
        CloseHandle(process_token);
    }
    return pipe;
}

static DWORD WINAPI serve_requests(LPVOID opaque)
{
    server_context *context = opaque;
    size_t index;

    if (!ConnectNamedPipe(context->pipe, NULL) &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
        return fail_server(context);
    }

    for (index = 0; index < ROUND_TRIPS; ++index) {
        uint8_t request_bytes[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
        uint8_t response_bytes[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
        music_rig_protocol_request request;
        music_rig_protocol_response response;
        DWORD transferred;

        if (!ReadFile(
                context->pipe,
                request_bytes,
                (DWORD)sizeof(request_bytes),
                &transferred,
                NULL
            ) || transferred != (DWORD)sizeof(request_bytes) ||
            (index == 0U &&
                memcmp(
                    request_bytes,
                    MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN,
                    sizeof(request_bytes)
                ) != 0) ||
            music_rig_protocol_decode_request(
                request_bytes,
                sizeof(request_bytes),
                &request
            ) != MUSIC_RIG_RESULT_OK ||
            request.operation != (uint32_t)MUSIC_RIG_OPERATION_STATUS ||
            request.expected_generation != EXPECTED_GENERATION) {
            return fail_server(context);
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
            ) != MUSIC_RIG_RESULT_OK ||
            (index == 0U &&
                memcmp(
                    response_bytes,
                    MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN,
                    sizeof(response_bytes)
                ) != 0) ||
            !WriteFile(
                context->pipe,
                response_bytes,
                (DWORD)sizeof(response_bytes),
                &transferred,
                NULL
            ) || transferred != (DWORD)sizeof(response_bytes)) {
            return fail_server(context);
        }
    }

    return 0U;
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
    wchar_t pipe_name[128];
    HANDLE server_pipe;
    HANDLE client_pipe;
    HANDLE server_thread;
    server_context context;
    LARGE_INTEGER frequency;
    uint64_t latencies[ROUND_TRIPS];
    size_t index;
    int failed = 0;
    DWORD wait_result;
    DWORD read_mode = PIPE_READMODE_MESSAGE;

    if (_snwprintf_s(
            pipe_name,
            _countof(pipe_name),
            _TRUNCATE,
            L"\\\\.\\pipe\\music-rig-ipc-spike-%lu",
            GetCurrentProcessId()
        ) < 0 || !QueryPerformanceFrequency(&frequency)) {
        fputs("could not initialize Windows IPC test\n", stderr);
        return 1;
    }

    server_pipe = create_server_pipe(pipe_name);
    if (server_pipe == INVALID_HANDLE_VALUE) {
        fputs("could not create current-user-only named pipe\n", stderr);
        return 1;
    }

    client_pipe = CreateFileW(
        pipe_name,
        GENERIC_READ | GENERIC_WRITE,
        0U,
        NULL,
        OPEN_EXISTING,
        0U,
        NULL
    );
    if (client_pipe == INVALID_HANDLE_VALUE ||
        !SetNamedPipeHandleState(client_pipe, &read_mode, NULL, NULL)) {
        fputs("could not connect named-pipe client in message mode\n", stderr);
        if (client_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(client_pipe);
        }
        CloseHandle(server_pipe);
        return 1;
    }

    context.pipe = server_pipe;
    context.failed = 0L;
    server_thread = CreateThread(NULL, 0U, serve_requests, &context, 0U, NULL);
    if (server_thread == NULL) {
        fputs("could not start named-pipe server thread\n", stderr);
        CloseHandle(client_pipe);
        CloseHandle(server_pipe);
        return 1;
    }

    for (index = 0; index < ROUND_TRIPS; ++index) {
        const uint64_t request_id = index == 0U
            ? GOLDEN_REQUEST_ID
            : (uint64_t)index + UINT64_C(1000);
        const music_rig_protocol_request request = {
            MUSIC_RIG_PROTOCOL_VERSION,
            (uint32_t)MUSIC_RIG_OPERATION_STATUS,
            request_id,
            EXPECTED_GENERATION
        };
        music_rig_protocol_response response;
        uint8_t request_bytes[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
        uint8_t response_bytes[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
        LARGE_INTEGER started;
        LARGE_INTEGER finished;
        DWORD transferred;

        if (music_rig_protocol_encode_request(
                &request,
                request_bytes,
                sizeof(request_bytes)
            ) != MUSIC_RIG_RESULT_OK ||
            (index == 0U &&
                memcmp(
                    request_bytes,
                    MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN,
                    sizeof(request_bytes)
                ) != 0) ||
            !QueryPerformanceCounter(&started) ||
            !WriteFile(
                client_pipe,
                request_bytes,
                (DWORD)sizeof(request_bytes),
                &transferred,
                NULL
            ) || transferred != (DWORD)sizeof(request_bytes) ||
            !ReadFile(
                client_pipe,
                response_bytes,
                (DWORD)sizeof(response_bytes),
                &transferred,
                NULL
            ) || transferred != (DWORD)sizeof(response_bytes) ||
            !QueryPerformanceCounter(&finished) ||
            (index == 0U &&
                memcmp(
                    response_bytes,
                    MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN,
                    sizeof(response_bytes)
                ) != 0) ||
            music_rig_protocol_decode_response(
                response_bytes,
                sizeof(response_bytes),
                &response
            ) != MUSIC_RIG_RESULT_OK ||
            response.request_id != request.request_id ||
            response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
            response.previous_generation != EXPECTED_GENERATION ||
            response.resulting_generation != EXPECTED_GENERATION ||
            !elapsed_ns(started, finished, frequency, &latencies[index])) {
            fputs("named-pipe request/response validation failed\n", stderr);
            failed = 1;
            break;
        }
    }

    if (failed != 0) {
        CloseHandle(client_pipe);
        client_pipe = INVALID_HANDLE_VALUE;
    }
    wait_result = WaitForSingleObject(server_thread, SERVER_WAIT_MS);
    if (wait_result != WAIT_OBJECT_0 ||
        InterlockedCompareExchange(&context.failed, 0L, 0L) != 0L) {
        fputs("named-pipe server failed or timed out\n", stderr);
        failed = 1;
    }

    if (client_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(client_pipe);
    }
    CloseHandle(server_thread);
    CloseHandle(server_pipe);
    if (failed != 0) {
        return 1;
    }

    qsort(latencies, ROUND_TRIPS, sizeof(latencies[0]), compare_u64);
    if (latencies[989] >= ROUND_TRIP_LIMIT_NS) {
        fputs("named-pipe p99 round trip exceeded 20 ms\n", stderr);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/ipc-round-trip-spike/v1\","
        "\"transport\":\"windows-named-pipe-message\","
        "\"security\":\"current-user-only\","
        "\"round_trips\":%zu,\"p50_ns\":%" PRIu64 ","
        "\"p95_ns\":%" PRIu64 ",\"p99_ns\":%" PRIu64 ","
        "\"maximum_ns\":%" PRIu64 ",\"golden_frames\":true,"
        "\"failures\":0}\n",
        ROUND_TRIPS,
        latencies[499],
        latencies[949],
        latencies[989],
        latencies[999]
    );
    return 0;
}

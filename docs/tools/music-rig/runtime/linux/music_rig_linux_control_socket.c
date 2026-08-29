#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "music_rig/linux_control_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <poll.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

#define CONTROL_IO_TIMEOUT_MS 2000

static music_rig_result wait_for_io(int descriptor, short events)
{
    struct pollfd descriptor_state;
    int result;

    descriptor_state.fd = descriptor;
    descriptor_state.events = events;
    descriptor_state.revents = 0;
    do {
        result = poll(&descriptor_state, 1, CONTROL_IO_TIMEOUT_MS);
    } while (result < 0 && errno == EINTR);
    return result == 1 ? MUSIC_RIG_RESULT_OK
        : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result send_frame(
    int descriptor,
    const uint8_t *frame,
    size_t frame_size
)
{
    ssize_t transferred;

    for (;;) {
        do {
            transferred = send(
                descriptor, frame, frame_size, MSG_NOSIGNAL | MSG_DONTWAIT
            );
        } while (transferred < 0 && errno == EINTR);
        if (transferred == (ssize_t)frame_size) {
            return MUSIC_RIG_RESULT_OK;
        }
        if (transferred < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
            wait_for_io(descriptor, POLLOUT) == MUSIC_RIG_RESULT_OK) {
            continue;
        }
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
}

static music_rig_result receive_frame(
    int descriptor,
    uint8_t *frame,
    size_t frame_size
)
{
    ssize_t received;

    for (;;) {
        do {
            received = recv(descriptor, frame, frame_size,
                MSG_DONTWAIT | MSG_TRUNC);
        } while (received < 0 && errno == EINTR);
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
            wait_for_io(descriptor, POLLIN) == MUSIC_RIG_RESULT_OK) {
            continue;
        }
        break;
    }
    if (received == 0) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    return received == (ssize_t)frame_size
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_INVALID_DATA;
}

static music_rig_result set_nonblocking(int descriptor)
{
    int flags = fcntl(descriptor, F_GETFL, 0);

    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result remove_stale_socket(
    const char *path,
    const struct sockaddr_un *address
)
{
    struct stat existing;
    int descriptor;

    if (lstat(path, &existing) != 0) {
        return errno == ENOENT
            ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!S_ISSOCK(existing.st_mode) || existing.st_uid != geteuid()) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    descriptor = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (descriptor < 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (connect(descriptor, (const struct sockaddr *)address,
            sizeof(address->sun_family) + strlen(path) + 1U) == 0) {
        (void)close(descriptor);
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (errno != ECONNREFUSED && errno != ENOENT) {
        (void)close(descriptor);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    (void)close(descriptor);
    return unlink(path) == 0 || errno == ENOENT
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

music_rig_result music_rig_linux_control_client_connect(
    music_rig_linux_control_client *client,
    const char *path
)
{
    struct sockaddr_un address;
    size_t path_size;
    int descriptor;

    if (client == NULL || path == NULL || path[0] != '/') {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    path_size = strlen(path);
    if (path_size >= sizeof(address.sun_path)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    client->descriptor = -1;
    descriptor = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (descriptor < 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, path_size + 1U);
    if (connect(descriptor, (const struct sockaddr *)&address,
            sizeof(address.sun_family) + path_size + 1U) != 0) {
        (void)close(descriptor);
        return errno == ENOENT || errno == ECONNREFUSED
            ? MUSIC_RIG_RESULT_NOT_FOUND : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    client->descriptor = descriptor;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_linux_control_client_exchange(
    void *context,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_linux_control_client *client = context;
    uint8_t request_frame[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
    uint8_t response_frame[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
    music_rig_result result;

    if (client == NULL || request == NULL || response == NULL ||
        client->descriptor < 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_protocol_encode_request(
        request, request_frame, sizeof(request_frame)
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = send_frame(client->descriptor, request_frame,
            sizeof(request_frame));
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = receive_frame(client->descriptor, response_frame,
            sizeof(response_frame));
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_protocol_decode_response(
            response_frame, sizeof(response_frame), response
        );
    }
    return result;
}

music_rig_result music_rig_linux_control_client_close(
    music_rig_linux_control_client *client
)
{
    if (client == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (client->descriptor >= 0 && close(client->descriptor) != 0) {
        client->descriptor = -1;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    client->descriptor = -1;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_linux_control_server_open(
    music_rig_linux_control_server *server,
    const char *path
)
{
    struct sockaddr_un address;
    size_t path_size;
    int descriptor;
    music_rig_result result;

    if (server == NULL || path == NULL || path[0] != '/') {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    path_size = strlen(path);
    if (path_size >= sizeof(address.sun_path)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, path_size + 1U);
    result = remove_stale_socket(path, &address);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    memset(server, 0, sizeof(*server));
    server->listener = -1;
    server->client = -1;
    server->owner_uid = geteuid();
    server->path = path;
    descriptor = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (descriptor < 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (bind(descriptor, (const struct sockaddr *)&address,
            sizeof(address.sun_family) + path_size + 1U) != 0) {
        (void)close(descriptor);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (chmod(path, S_IRUSR | S_IWUSR) != 0 ||
        set_nonblocking(descriptor) != MUSIC_RIG_RESULT_OK ||
        listen(descriptor, 8) != 0) {
        (void)close(descriptor);
        (void)unlink(path);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    server->listener = descriptor;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_linux_control_server_start(void *context)
{
    music_rig_linux_control_server *server = context;

    return server == NULL || server->listener < 0
        ? MUSIC_RIG_RESULT_INVALID_ARGUMENT : MUSIC_RIG_RESULT_OK;
}

music_rig_control_poll music_rig_linux_control_server_poll(
    void *context,
    music_rig_protocol_request *request
)
{
    music_rig_linux_control_server *server = context;
    struct ucred credentials;
    socklen_t credential_size = sizeof(credentials);
    uint8_t frame[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
    ssize_t received;

    if (stop_requested != 0) {
        return MUSIC_RIG_CONTROL_STOP;
    }
    if (server == NULL || request == NULL || server->listener < 0) {
        return MUSIC_RIG_CONTROL_ERROR;
    }
    if (server->client < 0) {
        server->client = accept(server->listener, NULL, NULL);
        if (server->client < 0) {
            return errno == EAGAIN || errno == EWOULDBLOCK
                ? MUSIC_RIG_CONTROL_IDLE : MUSIC_RIG_CONTROL_ERROR;
        }
        if (getsockopt(server->client, SOL_SOCKET, SO_PEERCRED,
                &credentials, &credential_size) != 0 ||
            credentials.uid != server->owner_uid ||
            set_nonblocking(server->client) != MUSIC_RIG_RESULT_OK) {
            (void)close(server->client);
            server->client = -1;
            return MUSIC_RIG_CONTROL_IDLE;
        }
    }
    received = recv(server->client, frame, sizeof(frame), MSG_DONTWAIT | MSG_TRUNC);
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return MUSIC_RIG_CONTROL_IDLE;
    }
    if (received == 0) {
        (void)close(server->client);
        server->client = -1;
        return MUSIC_RIG_CONTROL_IDLE;
    }
    if (received != (ssize_t)sizeof(frame) ||
        music_rig_protocol_decode_request(frame, sizeof(frame), request) !=
            MUSIC_RIG_RESULT_OK) {
        (void)close(server->client);
        server->client = -1;
        return MUSIC_RIG_CONTROL_IDLE;
    }
    return MUSIC_RIG_CONTROL_REQUEST;
}

music_rig_result music_rig_linux_control_server_wait(void *context)
{
    music_rig_linux_control_server *server = context;
    fd_set descriptors;
    struct timeval timeout = {0, 100000};
    int maximum;
    int result;

    if (server == NULL || server->listener < 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    FD_ZERO(&descriptors);
    FD_SET(server->listener, &descriptors);
    maximum = server->listener;
    if (server->client >= 0) {
        FD_SET(server->client, &descriptors);
        if (server->client > maximum) {
            maximum = server->client;
        }
    }
    result = select(maximum + 1, &descriptors, NULL, NULL, &timeout);
    return result < 0 && errno != EINTR
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE : MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_linux_control_server_respond(
    void *context,
    const music_rig_protocol_response *response
)
{
    music_rig_linux_control_server *server = context;
    uint8_t frame[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
    music_rig_result result;

    if (server == NULL || response == NULL || server->client < 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_protocol_encode_response(
        response, frame, sizeof(frame)
    );
    return result == MUSIC_RIG_RESULT_OK
        ? send_frame(server->client, frame, sizeof(frame)) : result;
}

music_rig_result music_rig_linux_control_server_stop(void *context)
{
    music_rig_linux_control_server *server = context;
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    if (server == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (server->client >= 0 && close(server->client) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (server->listener >= 0 && close(server->listener) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    server->client = -1;
    server->listener = -1;
    if (server->path != NULL && unlink(server->path) != 0 && errno != ENOENT) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return result;
}

void music_rig_linux_control_server_request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

#ifndef MUSIC_RIG_LINUX_CONTROL_SOCKET_H
#define MUSIC_RIG_LINUX_CONTROL_SOCKET_H

#include "music_rig/protocol.h"
#include "music_rig/runtime.h"

#include <sys/types.h>

typedef struct music_rig_linux_control_client {
    int descriptor;
} music_rig_linux_control_client;

typedef struct music_rig_linux_control_server {
    int listener;
    int client;
    uid_t owner_uid;
    const char *path;
} music_rig_linux_control_server;

music_rig_result music_rig_linux_control_client_connect(
    music_rig_linux_control_client *client,
    const char *path
);

music_rig_result music_rig_linux_control_client_exchange(
    void *context,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
);

music_rig_result music_rig_linux_control_client_close(
    music_rig_linux_control_client *client
);

music_rig_result music_rig_linux_control_server_open(
    music_rig_linux_control_server *server,
    const char *path
);

music_rig_result music_rig_linux_control_server_start(void *context);
music_rig_control_poll music_rig_linux_control_server_poll(
    void *context,
    music_rig_protocol_request *request
);
music_rig_result music_rig_linux_control_server_wait(void *context);
music_rig_result music_rig_linux_control_server_respond(
    void *context,
    const music_rig_protocol_response *response
);
music_rig_result music_rig_linux_control_server_stop(void *context);

void music_rig_linux_control_server_request_stop(int signal_number);

#endif

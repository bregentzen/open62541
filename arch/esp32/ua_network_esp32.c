#include "ua_eventloop_esp32.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define TAG "UA_Network_ESP32"

static UA_StatusCode
ESP32_recv(UA_Connection *connection, UA_ByteString *response,
           UA_UInt32 timeout) {
    UA_SOCKET sockfd = (UA_SOCKET)(uintptr_t)connection->handle;

    if(response->data == NULL) {
        response->data = (UA_Byte *)UA_malloc(connection->remoteConf.recvBufferSize);
        if(!response->data)
            return UA_STATUSCODE_BADOUTOFMEMORY;
    }

    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t recvLen = recv(sockfd, response->data,
                           connection->remoteConf.recvBufferSize, 0);

    if(recvLen == 0)
        return UA_STATUSCODE_BADCONNECTIONCLOSED;
    else if(recvLen < 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN)
            return UA_STATUSCODE_BADCOMMUNICATIONERROR;
        return UA_STATUSCODE_BADCONNECTIONCLOSED;
    }

    response->length = (size_t)recvLen;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
ESP32_send(UA_Connection *connection, UA_ByteString *msg) {
    UA_SOCKET sockfd = (UA_SOCKET)(uintptr_t)connection->handle;
    size_t bytesSent = 0;

    while(bytesSent < msg->length) {
        ssize_t sent = send(sockfd, msg->data + bytesSent,
                            msg->length - bytesSent, MSG_NOSIGNAL);
        if(sent <= 0)
            return UA_STATUSCODE_BADCONNECTIONCLOSED;
        bytesSent += (size_t)sent;
    }

    return UA_STATUSCODE_GOOD;
}

static void
ESP32_close(UA_Connection *connection) {
    UA_SOCKET sockfd = (UA_SOCKET)(uintptr_t)connection->handle;
    if(sockfd != UA_INVALID_SOCKET) {
        lwip_close(sockfd);
        connection->handle = (uintptr_t)UA_INVALID_SOCKET;
    }
}

static UA_Connection
ESP32_createConnection(UA_SOCKET sockfd, UA_ConnectionConfig conf) {
    UA_Connection conn;
    UA_Connection_init(&conn);

    conn.handle = (uintptr_t)sockfd;
    conn.send = ESP32_send;
    conn.recv = ESP32_recv;
    conn.close = ESP32_close;
    conn.getSendBuffer = UA_Connection_defaultGetSendBuffer;
    conn.releaseSendBuffer = UA_Connection_defaultReleaseSendBuffer;
    conn.remoteConf = conf;

    return conn;
}

static UA_StatusCode
acceptNewConnections(UA_Server *server, UA_ESP32ConnectionManager *cm) {
    struct sockaddr_storage clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    UA_SOCKET clientfd;

    while((clientfd = accept(cm->listenFd,
                              (struct sockaddr*)&clientAddr,
                              &addrlen)) >= 0) {
        lwip_fcntl(clientfd, F_SETFL, O_NONBLOCK);

        UA_Connection conn = ESP32_createConnection(clientfd, cm->conf);
        UA_Server_addNetworkConnection(server, &conn);
    }

    if(errno == EWOULDBLOCK || errno == EAGAIN)
        return UA_STATUSCODE_GOOD;

    return UA_STATUSCODE_BADCOMMUNICATIONERROR;
}

static UA_StatusCode
ESP32_listen(UA_ConnectionManager *cmBase, UA_Server *server, UA_DateTime timeout) {
    UA_ESP32ConnectionManager *cm = (UA_ESP32ConnectionManager *)cmBase;
    return acceptNewConnections(server, cm);
}

UA_ConnectionManager *
UA_ConnectionManager_new_ESP32_TCP(const char *portStr,
                                   const UA_ConnectionConfig *conf,
                                   UA_Logger logger) {
    UA_ESP32ConnectionManager *cm = (UA_ESP32ConnectionManager *)UA_calloc(1, sizeof(UA_ESP32ConnectionManager));
    if(!cm)
        return NULL;

    cm->conf = *conf;
    cm->base.listen = ESP32_listen;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(portStr));
    addr.sin_addr.s_addr = INADDR_ANY;

    int sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        goto fail;

    int opt = 1;
    lwip_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    lwip_fcntl(sockfd, F_SETFL, O_NONBLOCK);

    if(lwip_bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        goto fail;

    if(lwip_listen(sockfd, UA_MAXBACKLOG) < 0)
        goto fail;

    cm->listenFd = sockfd;
    return (UA_ConnectionManager *)cm;

fail:
    if(sockfd >= 0)
        lwip_close(sockfd);
    UA_free(cm);
    return NULL;
}
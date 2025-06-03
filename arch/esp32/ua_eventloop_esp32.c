#include "ua_eventloop_esp32.h"
#include <string.h>

#define TAG "UA_EventLoop_ESP32"

UA_StatusCode
UA_EventLoopESP32_addDelayedCallback(UA_EventLoopESP32 *el, UA_DelayedCallback *dc) {
    return UA_Timer_addDelayedCallback(&el->timer, dc);
}

/* --- Helper: Poll active FDs with lwip_select --- */
UA_StatusCode
UA_EventLoopESP32_pollFDs(UA_EventLoopESP32 *el, UA_DateTime listenTimeout) {
    fd_set readfds;
    int maxfd = -1;
    struct timeval timeout;
    UA_UInt64 now = UA_DateTime_nowMonotonic();
    UA_UInt64 timeout_us = (listenTimeout > now) ? (listenTimeout - now) / 10 : 0;

    timeout.tv_sec = timeout_us / 1000000;
    timeout.tv_usec = timeout_us % 1000000;

    FD_ZERO(&readfds);

    for(size_t i = 0; i < el->fdList.size; ++i) {
        UA_RegisteredFD *rfd = el->fdList.entries[i];
        if(!rfd) continue;
        FD_SET(rfd->fd, &readfds);
        if((int)rfd->fd > maxfd)
            maxfd = rfd->fd;
    }

    int ret = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
    if(ret < 0)
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;

    for(size_t i = 0; i < el->fdList.size; ++i) {
        UA_RegisteredFD *rfd = el->fdList.entries[i];
        if(!rfd) continue;
        if(FD_ISSET(rfd->fd, &readfds)) {
            rfd->eventSourceCB(rfd->es, rfd, UA_FDEVENT_IN);
        }
    }

    return UA_STATUSCODE_GOOD;
}

/* --- Register an FD --- */
UA_StatusCode
UA_EventLoopESP32_registerFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd) {
    if(el->fdList.size >= el->fdList.capacity)
        return UA_STATUSCODE_BADINTERNALERROR;

    el->fdList.entries[el->fdList.size++] = rfd;
    return UA_STATUSCODE_GOOD;
}

/* --- Modify FD: no-op in simple polling --- */
UA_StatusCode
UA_EventLoopESP32_modifyFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd) {
    return UA_STATUSCODE_GOOD;
}

/* --- Deregister FD --- */
void
UA_EventLoopESP32_deregisterFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd) {
    for(size_t i = 0; i < el->fdList.size; ++i) {
        if(el->fdList.entries[i] == rfd) {
            el->fdList.entries[i] = NULL;
            break;
        }
    }
}

/* --- UA_EventLoop interface --- */
static UA_StatusCode
run(UA_EventLoop *loop, UA_UInt32 listenTimeout) {
    UA_EventLoopESP32 *el = (UA_EventLoopESP32 *)loop;

    el->executing = true;
    UA_Timer_process(&el->timer);
    UA_EventLoopESP32_pollFDs(el, listenTimeout);
    el->executing = false;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
addDelayedCallback(UA_EventLoop *loop, UA_DelayedCallback *dc) {
    return UA_EventLoopESP32_addDelayedCallback((UA_EventLoopESP32 *)loop, dc);
}

/* Add additional methods as needed... */

/* --- Constructor --- */
UA_EventLoop *
UA_EventLoop_Platform_new(UA_Server *server, UA_ServerConfig *config) {
    UA_EventLoopESP32 *el = (UA_EventLoopESP32 *)UA_calloc(1, sizeof(UA_EventLoopESP32));
    if(!el)
        return NULL;

    el->eventLoop.run = run;
    el->eventLoop.addDelayedCallback = addDelayedCallback;
    /* assign other ops if needed */

    el->fdList.capacity = 10;
    el->fdList.entries = (UA_RegisteredFD **)UA_calloc(el->fdList.capacity, sizeof(UA_RegisteredFD*));
    el->fdList.size = 0;

#if UA_MULTITHREADING >= 100
    UA_Lock_init(&el->elMutex);
#endif

    UA_Timer_init(&el->timer, config->logger, config->allocator);
    return (UA_EventLoop *)el;
}
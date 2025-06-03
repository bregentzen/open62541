/* Adapted for ESP32 (FreeRTOS + lwIP)
 * License: MPL v2.0
 */

#ifndef UA_EVENTLOOP_ESP32_H_
#define UA_EVENTLOOP_ESP32_H_

#include <open62541/types.h>
#include <open62541/plugin/log.h>
#include <open62541/plugin/network.h>
#include <open62541/server.h>
#include <open62541/server_config.h>

#include <open62541/config.h>
#include <open62541/plugin/eventloop.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/err.h"

#include "../deps/mp_printf.h"
#include "../common/ua_timer.h"

_UA_BEGIN_DECLS

#define UA_MAXBACKLOG 5
#define UA_MAXHOSTNAME_LENGTH 64
#define UA_MAXPORTSTR_LENGTH 6

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

/* Event mask */
#define UA_FDEVENT_IN  1
#define UA_FDEVENT_OUT 2
#define UA_FDEVENT_ERR 4

typedef int UA_FD;
#define UA_INVALID_FD (-1)

/* Forward declarations */
struct UA_RegisteredFD;
typedef struct UA_RegisteredFD UA_RegisteredFD;

typedef void (*UA_FDCallback)(UA_EventSource *es, UA_RegisteredFD *rfd, short event);

/* Registered socket or netconn (depending on backend choice) */
struct UA_RegisteredFD {
    UA_DelayedCallback dc;
    UA_FD fd;  // lwIP socket fd or netconn handle (casted)
    short listenEvents;
    UA_EventSource *es;
    UA_FDCallback eventSourceCB;
};

typedef struct {
    size_t size;
    size_t capacity;
    UA_RegisteredFD **entries;
} UA_RegisteredFDList;

/* Tree or list to manage active FDs – placeholder for now */
typedef struct {
    UA_RegisteredFD **entries;
    size_t size;
    size_t capacity;
} UA_ESP32FDList;

/* TCP-only ConnectionManager for lwIP */
typedef struct {
    UA_ConnectionManager base;
    UA_ConnectionConfig conf;
    UA_SOCKET listenFd;
} UA_ESP32ConnectionManager;

/* Eventloop type for ESP32 */
typedef struct {
    UA_EventLoop eventLoop;
    UA_RegisteredFDList fdList;
    UA_Timer timer;
    UA_Boolean executing;
#if UA_MULTITHREADING >= 100
    UA_Lock elMutex;
#endif
} UA_EventLoopESP32;

/* API Functions – Implement in ua_eventloop_esp32.c */
UA_StatusCode
UA_EventLoopESP32_registerFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd);

UA_StatusCode
UA_EventLoopESP32_modifyFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd);

void
UA_EventLoopESP32_deregisterFD(UA_EventLoopESP32 *el, UA_RegisteredFD *rfd);

UA_StatusCode
UA_EventLoopESP32_pollFDs(UA_EventLoopESP32 *el, UA_DateTime listenTimeout);

UA_StatusCode
UA_EventLoopESP32_allocateRXBuffer(UA_ESP32ConnectionManager *cm);

UA_StatusCode
UA_EventLoopESP32_allocNetworkBuffer(UA_ConnectionManager *cm,
                                     uintptr_t connectionId,
                                     UA_ByteString *buf,
                                     size_t bufSize);

void
UA_EventLoopESP32_freeNetworkBuffer(UA_ConnectionManager *cm,
                                    uintptr_t connectionId,
                                    UA_ByteString *buf);

/* Helpers for socket behavior (nonblocking, etc.) */
UA_StatusCode
UA_EventLoopESP32_setNonBlocking(UA_FD sockfd);

UA_StatusCode
UA_EventLoopESP32_setNoSigPipe(UA_FD sockfd);

UA_StatusCode
UA_EventLoopESP32_setReusable(UA_FD sockfd);

_UA_END_DECLS

#endif /* UA_EVENTLOOP_ESP32_H_ */
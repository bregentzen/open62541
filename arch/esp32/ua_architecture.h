#ifndef UA_ARCHITECTURE_ESP32_H_
#define UA_ARCHITECTURE_ESP32_H_

/* Architekturkennung setzen */
#define UA_ARCHITECTURE_ESP32

/* Standardplattformen ausschließen */
#undef UA_ARCHITECTURE_POSIX
#undef UA_ARCHITECTURE_WIN32

/* Standard Includes */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* FreeRTOS-spezifisch */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"

/* Logging optional: */
#include "esp_log.h"

/* LwIP für Networking */
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"

#define UA_PLATFORM_INLINE inline
#define UA_THREAD_LOCAL _Thread_local

/* Memory */
#define UA_malloc(size)           malloc(size)
#define UA_free(ptr)              free(ptr)
#define UA_calloc(nmemb, size)    calloc(nmemb, size)
#define UA_realloc(ptr, size)     realloc(ptr, size)

/* Logging */
#define UA_log(logCtx, level, msg, ...) \
    ESP_LOG_LEVEL_LOCAL(level, "UA", msg, ##__VA_ARGS__)

/* Sleep in ms */
#define UA_sleep_ms(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

/* Locks */
typedef SemaphoreHandle_t UA_Lock;

#define UA_LOCK_INIT(m)    *(m) = xSemaphoreCreateMutex()
#define UA_LOCK(m)         xSemaphoreTake(*(m), portMAX_DELAY)
#define UA_UNLOCK(m)       xSemaphoreGive(*(m))
#define UA_LOCK_DESTROY(m) vSemaphoreDelete(*(m))

#endif /* UA_ARCHITECTURE_ESP32_H_ */
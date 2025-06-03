#include <open62541/types.h>
#include <open62541/plugin/log.h>
#include "esp_timer.h"

/* Monotonic time: basiert auf Mikrosekunden seit Boot */
UA_DateTime
UA_DateTime_nowMonotonic(void) {
    int64_t micros = esp_timer_get_time(); // µs seit Start
    return ((UA_DateTime)micros) * 10;     // OPC UA-Zeit ist in 100ns
}

/* Wall time: optional – hier leer, falls nicht gebraucht */
UA_DateTime
UA_DateTime_now(void) {
    return UA_DateTime_nowMonotonic(); // oder system time via gettimeofday
}
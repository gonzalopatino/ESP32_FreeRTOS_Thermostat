#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float temp_inside_c;
    float temp_outside_c;
    uint32_t timestamp_ms;
} sensor_sample_t;

#endif

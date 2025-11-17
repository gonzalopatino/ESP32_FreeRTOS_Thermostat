#ifndef DRV_TEMP_SENSORS_H
#define DRV_TEMP_SENSORS_H

#include "core/app_types.h"
#include "core/error.h"

app_error_t drv_temp_sensors_init(void);

app_error_t drv_temp_read(sensor_sample_t *out_sample);

#endif

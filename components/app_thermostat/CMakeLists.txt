idf_component_register(
    SRCS
        "src/task_heartbeat.c"
        "src/task_logger.c"
        "src/task_common.c"
        "src/task_sensors.c"
        "src/task_control.c"
    INCLUDE_DIRS "include"
    REQUIRES core drivers_thermostat
    PRIV_REQUIRES esp_driver_gpio
)

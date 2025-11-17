idf_component_register(
    SRCS
        "src/task_heartbeat.c"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES esp_driver_gpio
)

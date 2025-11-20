idf_component_register(
    SRCS
        "src/logging.c"
        "src/error.c"
        "src/watchdog.c"
        "src/thermostat_config.c"
        "src/thermostat.c"
        "src/timeutil.c"
    INCLUDE_DIRS "include"
    REQUIRES
        freertos
        esp_system
        log
        lwip
        esp_netif
)

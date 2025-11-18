idf_component_register(
    SRCS
        "src/task_heartbeat.c"
        "src/task_logger.c"
        "src/task_common.c"
        "src/task_sensors.c"
        "src/task_control.c"
        "src/task_display.c"
        "src/task_buttons.c"
        "src/task_net.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        core
        drivers_thermostat
    PRIV_REQUIRES 
        esp_driver_gpio
        esp_wifi
        esp_netif
        nvs_flash
        esp_http_client 
        esp_system
        mbedtls
        
)

idf_component_register(
    SRCS
        "src/drv_temp_sensors.c"
        "src/drv_display.c"
        "src/drv_buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES core # consumers of this component also see core's headers
    PRIV_REQUIRES
    driver # internal dependency for driver/i2c.h, driver/gpio.h
)
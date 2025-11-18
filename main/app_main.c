#include "core/config.h"            // Global configuration: app name, version, stack sizes, priorities, periods
#include "core/logging.h"           // Logging system (queue + log_post)
#include "core/error.h"             // Error handling utilities (fatal + non-fatal)
#include "core/watchdog.h"          // Watchdog framework for monitoring task health

#include "app/task_common.h"        // Shared inter-task queues and helpers
#include "app/task_sensors.h"       // Sensor task (temperature acquisition)
#include "app/task_logger.h"        // Logger task (consumes log queue)
#include "app/task_heartbeat.h"     // Heartbeat task (LED blink + alive message)
#include "app/task_control.h"       // Control task (hysteresis, heater output)

#include "core/thermostat.h"        // Thermostat core (decision logic)
// #include "core/thermostat_config.h" // No longer needed here, core_init handles it

#include "app/task_display.h"

#include "app/task_buttons.h"

#include "app/task_net.h"

// Gonzalo Patino

/**
 * @brief Entry point of the application.
 *
 * app_main() is called once by the ESP-IDF runtime after boot.
 * This function performs global system initialization and creates
 * all top-level FreeRTOS tasks that make up the thermostat firmware.
 *
 * High-level responsibilities:
 *   - Bring up logging and watchdog services
 *   - Create shared queues used by multiple tasks
 *   - Initialize thermostat core (which loads configuration)
 *   - Emit startup information (name, version)
 *   - Launch tasks in the correct dependency order
 */
void app_main(void) {
    // Initialize logging subsystem and create the log queue.
    // Must be called early, before any task tries to log messages.
    logging_init();

    // Start watchdog system. If it fails (rare), abort immediately because
    // running a system without a watchdog in production is unsafe.
    if (watchdog_init() != ESP_OK) {
        error_fatal(ERR_WATCHDOG_INIT_FAILED, "watchdog_init");
    }

    // Create common queues used for inter-task communication.
    // Example: sensor samples → control task.
    tasks_common_init_queues();

    // Initialize thermostat core (which also initializes thermostat_config
    // and loads setpoint + hysteresis from defaults or stored values).
    if (thermostat_core_init() != ERR_OK) {
        error_fatal(ERR_GENERIC, "thermostat_core_init");
    }

    // Start NET (Wi-Fi) before any task that might need connectivity.
    task_net_start();

    // Emit startup message with application name and version.
    // Helpful for debugging, logs, and verifying firmware updates.
    log_post(LOG_LEVEL_INFO, "APP",
             "%s v%s starting", APP_NAME, APP_FW_VERSION);

    // ------------------------------
    //      Task Startup Order
    // ------------------------------

    // 1. Start the logger task FIRST.
    //    Every task in the system may attempt to post logs,
    //    so the logger must be running before others start.
    task_logger_start();

    // 2. Start the sensor task.
    //    This begins producing temperature samples, which other
    //    components (control, display, telemetry) rely on.
    task_sensors_start();

    // 3. Start the control task (consumes samples + drives heater GPIO).
    task_control_start();

    // Buttons start
    task_buttons_start();

    //3 Start the display task
    task_display_start();

    // 4. Start the heartbeat task last.
    //    This task blinks the status LED and logs periodic
    //    "alive" messages, confirming the scheduler is running.
    //    Additional tasks (display, telemetry, etc.) can be
    //    launched here as the system grows.
    task_heartbeat_start();

    thermostat_set_mode(THERMOSTAT_MODE_AUTO);
}

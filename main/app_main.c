#include "core/config.h"            // Global configuration constants (names, versions, stack sizes, etc.)
#include "core/logging.h"           // Logging system (queue + Log_post)
#include "core/error.h"             // Error handling utilities (faltal + non-fatal)
#include "core/watchdog.h"          // Watchdog framework for monitoring task health

#include "app/task_logger.h"        // Logger task (consumes Log queue)
#include "app/task_heartbeat.h"     // Heartbeat task (LED blink + alive message)

/*
 * @brief Entry point of the application
 *
 * This runs once after ESP_IDF boot, before any FreeRTOS tasks are started.
 * Here we bring up all global services (Logging, watchdog, etc.)
 * and then Launch the main application tasks.
*/
void app_main(void) {
    // Initialize Logging subsystem and create the Log queue
    // Must be called early, before any task tries to Log messages.
    logging_init();

    // Start watchdog system. If it fails (rare), abort immediately because
    // running a system without a watchdog in production is unsafe
    if (watchdog_init() != ESP_OK) {
        error_fatal(ERR_WATCHDOG_INIT_FAILED, "watchdog_init");
    }

    // Log application name and version at boot, using structured Logging.
    log_post(LOG_LEVEL_INFO, "APP",
        "%s v%s starting", APP_NAME, APP_FW_VERSION);

    // --- Task Startup Order --
    //
    // Start the Logger task FIRST so that all other tasks can safely
    // push Log messages into the Log queue from the moment they start.
    task_logger_start();

    // Start the heartbeat task which blinks the LED and Logs "alive" messages.
    // Later, other tasks will be initialized here (sensors, control, display, telemetry).
    task_heartbeat_start();
}

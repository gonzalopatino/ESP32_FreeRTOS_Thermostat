#include "core/config.h"
#include "core/logging.h"
#include "app/task_heartbeat.h"

void app_main(void) {
    logging_init();

    log_post(LOG_LEVEL_INFO, "APP",
             "%s v%s starting", APP_NAME, APP_FW_VERSION);

    task_heartbeat_start();
}

#include "core/error.h"
#include "core/logging.h"
#include <stdlib.h>

void error_report(app_error_t err, const char *context) {
    log_post(LOG_LEVEL_ERROR, "ERROR", "err=%d context=%s", (int)err, context);
}

void error_fatal(app_error_t err, const char *context) {
    log_post(LOG_LEVEL_ERROR, "FATAL", "err=%d context=%s", (int)err, context);

    // For now abort. Later you can call esp_restart() instead.
    abort();
}
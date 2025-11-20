#ifndef ERROR_H
#define ERROR_H

typedef enum {
    
    APP_ERR_OK = 0,
    ERR_GENERIC = 1,
    ERR_WATCHDOG_INIT_FAILED = 2,
    ERR_QUEUE_CREATE_FAILED = 3,
    
   
    // Add more error codes as needed
    
} app_error_t;

// Non fatal error reporting
void error_report(app_error_t err, const char *context);

// Fatal error: Log and abort or reset
void error_fatal(app_error_t err, const char *context);

#endif
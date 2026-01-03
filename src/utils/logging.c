/**
 * @file logging.c
 * @brief Logging implementation
 */

#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/**
 * @brief Current log level
 */
static LogLevel_t g_log_level = LOG_LEVEL_INFO;

/**
 * @brief Log level names
 */
static const char* log_level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL"
};

void Log_SetLevel(LogLevel_t level)
{
    g_log_level = level;
}

LogLevel_t Log_GetLevel(void)
{
    return g_log_level;
}

void Log_Message(LogLevel_t level, const char *format, ...)
{
    if (level < g_log_level) {
        return;
    }

    /* Print log level prefix */
    fprintf(stderr, "[%s] ", log_level_names[level]);

    /* Print the actual message */
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    /* Flush to ensure output is written immediately */
    fflush(stderr);
}

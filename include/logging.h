/**
 * @file logging.h
 * @brief Logging interface for debugging and diagnostics
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log level enumeration
 */
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel_t;

/**
 * @brief Set global log level
 *
 * @param level Log level
 */
void Log_SetLevel(LogLevel_t level);

/**
 * @brief Get current log level
 *
 * @return Current log level
 */
LogLevel_t Log_GetLevel(void);

/**
 * @brief Log a message
 *
 * @param level Log level
 * @param format Format string
 * @param ... Format arguments
 */
void Log_Message(LogLevel_t level, const char *format, ...);

/**
 * @brief Convenience macros for logging
 */
#define LOG_TRACE(fmt, ...) Log_Message(LOG_LEVEL_TRACE, "[TRACE] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Log_Message(LOG_LEVEL_DEBUG, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Log_Message(LOG_LEVEL_INFO,  "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Log_Message(LOG_LEVEL_WARN,  "[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Log_Message(LOG_LEVEL_ERROR, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) Log_Message(LOG_LEVEL_FATAL, "[FATAL] " fmt "\n", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_H */

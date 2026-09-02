/*
 * plant_error.c — v0.49.56/57: Unified Error Management & Logging
 *
 * Centralized logging and error-handling utility with severity levels.
 * This module is intentionally lightweight: it wraps fprintf to stderr
 * with color-coded severity prefixes and provides a variadic printf-
 * style interface for structured error reporting.
 *
 * plant_error():  logs a message in red, then exits (1)
 * plant_fatal():  same visual as plant_error — reserved for critical
 *                 system-level failures (malloc failures, corrupted
 *                 state) where the semantic distinction matters to
 *                 callers and static analysis tooling.
 * plant_warning(): logs a message in yellow (non-fatal)
 * plant_info():    logs a message in blue (diagnostic)
 * plant_log():     general-purpose severity-filtered logger
 */

#include "plant_compat.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

PlantLogLevel plant_log_level = PLANT_WARNING;  /* default: warnings + errors */

void plant_log(PlantLogLevel level, const char* format, ...) {
    if (level > plant_log_level) return;

    const char* prefix = NULL;
    const char* color  = NULL;

    switch (level) {
        case PLANT_ERROR:   prefix = "ERROR";   color = COLOR_RED;    break;
        case PLANT_WARNING: prefix = "WARNING"; color = COLOR_YELLOW; break;
        case PLANT_INFO:    prefix = "INFO";    color = COLOR_CYAN;   break;
        case PLANT_DEBUG:   prefix = "DEBUG";   color = COLOR_BLUE;   break;
        default:            prefix = "LOG";     color = COLOR_RESET;  break;
    }

    va_list args;
    va_start(args, format);

    if (isatty(fileno(stderr)))
        fprintf(stderr, "%s[%s] ", color, prefix);
    else
        fprintf(stderr, "[%s] ", prefix);

    va_list args2;
    va_copy(args2, args);
    vfprintf(stderr, format, args2);
    va_end(args2);

    if (isatty(fileno(stderr)))
        fprintf(stderr, "%s", COLOR_RESET);

    fprintf(stderr, "\n");
    fflush(stderr);
}

void plant_error(const char* msg) {
    fprintf(stderr, "%s%s%s\n", COLOR_RED, msg, COLOR_RESET);
    exit(1);
}

void plant_fatal(const char* msg) {
    fprintf(stderr, "%s%s%s\n", COLOR_RED, msg, COLOR_RESET);
    exit(1);
}

void plant_warning(const char* msg) {
    fprintf(stderr, "%s%s%s\n", COLOR_YELLOW, msg, COLOR_RESET);
}

void plant_info(const char* msg) {
    fprintf(stderr, "%s%s%s\n", COLOR_BLUE, msg, COLOR_RESET);
}

void plant_debug(const char* msg) {
    plant_log(PLANT_DEBUG, "%s", msg);
}

void plant_set_log_level(PlantLogLevel level) {
    plant_log_level = level;
}

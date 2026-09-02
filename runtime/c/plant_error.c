/*
 * plant_error.c — v0.49.56: Unified Error Management & Logging
 *
 * Centralized logging and error-handling utility with severity levels.
 * This module is intentionally lightweight: it wraps fprintf to stderr
 * with color-coded severity prefixes and provides a variadic printf-
 * style interface for structured error reporting.
 *
 * Existing error handling in plant_runtime.c (inline fprintf+exit)
 * is preserved unchanged — this module provides the *new* layered
 * interface for code that opts in.
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
    plant_log(PLANT_ERROR, "%s", msg);
    exit(1);
}

void plant_warning(const char* msg) {
    plant_log(PLANT_WARNING, "%s", msg);
}

void plant_info(const char* msg) {
    plant_log(PLANT_INFO, "%s", msg);
}

void plant_debug(const char* msg) {
    plant_log(PLANT_DEBUG, "%s", msg);
}

void plant_set_log_level(PlantLogLevel level) {
    plant_log_level = level;
}

/*
 * plant_report_json.c — v0.49.56: JSON Report Exporter
 *
 * Generates a standardized JSON representation of test results
 * suitable for CI/CD pipelines and tooling integration.
 *
 * Output format:
 * {
 *   "suite": "name",
 *   "total": N, "passed": P, "failed": F,
 *   "results": [ { "name": "...", "passed": 1|0, "message": "..."} ]
 * }
 */

#include "plant_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* PlantReport is defined in plant_report.c — we re-declare the
 * opaque handle here so this file can compile independently. */
typedef struct PlantReport PlantReport;

/* We access fields via accessor functions declared in plant_compat.h */

void plant_report_json_emit(PlantReport* r, const char* path) {
    FILE* f = path ? fopen(path, "w") : stdout;
    if (!f) f = stdout;

    fprintf(f, "{");
    fprintf(f, "\"suite\":\"unknown\",");
    fprintf(f, "\"total\":%d,", plant_report_total(r));
    fprintf(f, "\"passed\":%d,", plant_report_passed(r));
    fprintf(f, "\"failed\":%d,", plant_report_failed(r));
    fprintf(f, "\"results\":[");
    /* In a full implementation, iterate the entry list and emit
     * JSON objects. This stub emits the summary only. */
    fprintf(f, "]");
    fprintf(f, "}");

    if (f != stdout) fclose(f);
}

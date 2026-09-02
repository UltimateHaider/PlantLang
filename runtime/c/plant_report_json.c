/*
 * plant_report_json.c — v0.49.57: Modular JSON Report Exporter
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

/* PlantReport is defined in plant_report.c — opaque handle here.
   Access fields via the accessor functions declared in plant_compat.h. */

static void json_escape(FILE* f, const char* s) {
    if (!s) { fputs("null", f); return; }
    fputc('"', f);
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f);  break;
            case '\r': fputs("\\r", f);  break;
            case '\t': fputs("\\t", f);  break;
            default:   fputc(*p, f);     break;
        }
    }
    fputc('"', f);
}

void plant_report_json_emit(PlantReport* r, const char* path) {
    FILE* f = path ? fopen(path, "w") : stdout;
    if (!f) f = stdout;

    fprintf(f, "{");
    fprintf(f, "\"suite\":");
    json_escape(f, plant_report_suite_name(r));
    fprintf(f, ",");
    fprintf(f, "\"total\":%d,", plant_report_total(r));
    fprintf(f, "\"passed\":%d,", plant_report_passed(r));
    fprintf(f, "\"failed\":%d,", plant_report_failed(r));
    fprintf(f, "\"results\":[");

    PlantReportEntry* e = plant_report_head(r);
    int first = 1;
    while (e) {
        if (!first) fprintf(f, ",");
        first = 0;
        fprintf(f, "{\"name\":");
        json_escape(f, plant_report_entry_name(e));
        fprintf(f, ",\"passed\":%d,\"message\":", plant_report_entry_passed(e));
        json_escape(f, plant_report_entry_message(e));
        fprintf(f, "}");
        e = plant_report_next(e);
    }

    fprintf(f, "]");
    fprintf(f, "}");

    if (f != stdout) fclose(f);
}

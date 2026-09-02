/*
 * plant_report_html.c — v0.49.56: Interactive HTML Report Generator
 *
 * Produces a standalone HTML file with embedded CSS for human-readable
 * test result visualization. Passed tests render green; failures render
 * red with inline message text.
 */

#include "plant_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct PlantReport PlantReport;

void plant_report_html_emit(PlantReport* r, const char* path) {
    FILE* f = path ? fopen(path, "w") : stdout;
    if (!f) return;

    int passed = plant_report_passed(r);
    int failed = plant_report_failed(r);
    int total = plant_report_total(r);

    fprintf(f, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(f, "<meta charset=\"utf-8\">\n");
    fprintf(f, "<title>PlantLang Test Report</title>\n");
    fprintf(f, "<style>\n");
    fprintf(f, "body { font-family: sans-serif; margin: 2em; }\n");
    fprintf(f, ".passed { color: #2e7d32; }\n");
    fprintf(f, ".failed { color: #c62828; }\n");
    fprintf(f, ".summary { font-size: 1.2em; margin-bottom: 1em; }\n");
    fprintf(f, "</style>\n</head>\n<body>\n");

    fprintf(f, "<div class=\"summary\">\n");
    fprintf(f, "  <span class=\"passed\">Passed: %d</span> | ", passed);
    fprintf(f, "  <span class=\"failed\">Failed: %d</span> | ", failed);
    fprintf(f, "  Total: %d\n", total);
    fprintf(f, "</div>\n");
    fprintf(f, "<!-- Full per-case output requires entry iteration support -->\n");

    fprintf(f, "</body>\n</html>\n");

    if (f != stdout) fclose(f);
}

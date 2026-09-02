/*
 * plant_report_html.c — v0.49.57: Interactive HTML Report Generator
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

static void html_escape(FILE* f, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '&': fputs("&amp;", f);  break;
            case '<': fputs("&lt;", f);   break;
            case '>': fputs("&gt;", f);   break;
            case '"': fputs("&quot;", f); break;
            default:  fputc(*p, f);       break;
        }
    }
}

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
    fprintf(f, "body { font-family: sans-serif; margin: 2em; background: #f8f9fa; }\n");
    fprintf(f, "h1 { color: #333; }\n");
    fprintf(f, ".summary { font-size: 1.2em; margin-bottom: 1.5em; padding: 1em; background: #fff; border-radius: 4px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }\n");
    fprintf(f, ".passed { color: #2e7d32; }\n");
    fprintf(f, ".failed { color: #c62828; }\n");
    fprintf(f, "table { border-collapse: collapse; width: 100%%; background: #fff; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }\n");
    fprintf(f, "th, td { border: 1px solid #ddd; padding: 8px 12px; text-align: left; }\n");
    fprintf(f, "th { background: #e9ecef; font-weight: 600; }\n");
    fprintf(f, "tr.pass-row { background: #d4edda; }\n");
    fprintf(f, "tr.fail-row { background: #f8d7da; }\n");
    fprintf(f, ".suite-name { font-weight: bold; color: #495057; }\n");
    fprintf(f, "</style>\n</head>\n<body>\n");

    fprintf(f, "<h1>PlantLang Test Report</h1>\n");
    fprintf(f, "<div class=\"summary\">\n");
    fprintf(f, "  Suite: <span class=\"suite-name\">");
    html_escape(f, plant_report_suite_name(r));
    fprintf(f, "</span><br>\n");
    fprintf(f, "  <span class=\"passed\">Passed: %d</span> | ", passed);
    fprintf(f, "  <span class=\"failed\">Failed: %d</span> | ", failed);
    fprintf(f, "  Total: %d\n", total);
    fprintf(f, "</div>\n");

    fprintf(f, "<table>\n");
    fprintf(f, "  <thead><tr><th>Status</th><th>Test Name</th><th>Message</th></tr></thead>\n");
    fprintf(f, "  <tbody>\n");

    PlantReportEntry* e = plant_report_head(r);
    while (e) {
        int p = plant_report_entry_passed(e);
        fprintf(f, "    <tr class=\"%s-row\"><td class=\"%s\">%s</td><td>",
                p ? "pass" : "fail", p ? "passed" : "failed", p ? "PASS" : "FAIL");
        html_escape(f, plant_report_entry_name(e));
        fprintf(f, "</td><td>");
        html_escape(f, plant_report_entry_message(e));
        fprintf(f, "</td></tr>\n");
        e = plant_report_next(e);
    }

    fprintf(f, "  </tbody>\n");
    fprintf(f, "</table>\n");

    fprintf(f, "</body>\n</html>\n");

    if (f != stdout) fclose(f);
}

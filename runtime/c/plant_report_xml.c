/*
 * plant_report_xml.c — v0.49.57: JUnit-Compliant XML Report Exporter
 *
 * Generates JUnit-compatible XML for CI/CD test runners (Jenkins,
 * GitLab CI, GitHub Actions). Each test case becomes a <testcase>
 * element inside a <testsuite>; failures carry <failure> children.
 */

#include "plant_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct PlantReport PlantReport;

static void xml_escape(FILE* f, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '<':  fputs("&lt;", f);   break;
            case '>':  fputs("&gt;", f);   break;
            case '&':  fputs("&amp;", f);  break;
            case '"':  fputs("&quot;", f); break;
            case '\'': fputs("&apos;", f); break;
            default:   fputc(*p, f);       break;
        }
    }
}

void plant_report_xml_emit(PlantReport* r, const char* path) {
    FILE* f = path ? fopen(path, "w") : stdout;
    if (!f) return;

    int total = plant_report_total(r);
    int failed = plant_report_failed(r);

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuites tests=\"%d\" failures=\"%d\">\n", total, failed);
    fprintf(f, "  <testsuite name=\"");
    xml_escape(f, plant_report_suite_name(r));
    fprintf(f, "\" tests=\"%d\" failures=\"%d\" errors=\"0\">\n", total, failed);

    PlantReportEntry* e = plant_report_head(r);
    while (e) {
        fprintf(f, "    <testcase name=\"");
        xml_escape(f, plant_report_entry_name(e));
        fprintf(f, "\"");
        if (!plant_report_entry_passed(e)) {
            fprintf(f, "><failure message=\"");
            xml_escape(f, plant_report_entry_message(e));
            fprintf(f, "\"/></testcase>\n");
        } else {
            fprintf(f, "/>\n");
        }
        e = plant_report_next(e);
    }

    fprintf(f, "  </testsuite>\n");
    fprintf(f, "</testsuites>\n");

    if (f != stdout) fclose(f);
}

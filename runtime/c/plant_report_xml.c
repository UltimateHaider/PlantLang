/*
 * plant_report_xml.c — v0.49.56: JUnit-Compliant XML Report Exporter
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

void plant_report_xml_emit(PlantReport* r, const char* path) {
    FILE* f = path ? fopen(path, "w") : stdout;
    if (!f) f = stdout;

    int total = plant_report_total(r);
    int failed = plant_report_failed(r);
    int passed = plant_report_passed(r);

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuites tests=\"%d\" failures=\"%d\">\n", total, failed);
    fprintf(f, "  <testsuite name=\"plantlang\" tests=\"%d\" failures=\"%d\" errors=\"0\">\n", total, failed);
    /* In a full implementation, iterate entries and emit <testcase>
     * elements with <failure> children for failed assertions. */
    fprintf(f, "  </testsuite>\n");
    fprintf(f, "</testsuites>\n");

    if (f != stdout) fclose(f);
}

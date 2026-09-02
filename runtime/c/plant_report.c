/*
 * plant_report.c — v0.49.56: Reporting Subsystem Core
 *
 * Central orchestrator for test results reporting. Delegates to
 * format-specific generators (JSON, HTML, XML). The report
 * structure collects test results in a simple format suitable
 * for all output backends.
 *
 * This is the foundation for the layered reporting architecture;
 * concrete exporters live in plant_report_json.c, plant_report_xml.c,
 * and plant_report_html.c.
 */

#include "plant_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Result record: one per test assertion or suite */
typedef struct PlantReportEntry {
    char* name;
    int   passed;
    char* message;
    struct PlantReportEntry* next;
} PlantReportEntry;

/* Report context: accumulates entries for a single run */
typedef struct PlantReport {
    char* suite_name;
    PlantReportEntry* head;
    PlantReportEntry* tail;
    int  total;
    int  passed;
    int  failed;
} PlantReport;

PlantReport* plant_report_create(const char* suite_name) {
    PlantReport* r = (PlantReport*)malloc(sizeof(PlantReport));
    if (!r) return NULL;
    r->suite_name = suite_name ? strdup(suite_name) : strdup("unnamed");
    r->head = r->tail = NULL;
    r->total = r->passed = r->failed = 0;
    return r;
}

void plant_report_add(PlantReport* r, const char* test_name, int passed, const char* message) {
    if (!r) return;
    PlantReportEntry* e = (PlantReportEntry*)malloc(sizeof(PlantReportEntry));
    if (!e) return;
    e->name = strdup(test_name ? test_name : "");
    e->passed = passed;
    e->message = message ? strdup(message) : strdup("");
    e->next = NULL;
    if (r->tail) r->tail->next = e;
    else r->head = e;
    r->tail = e;
    r->total++;
    if (passed) r->passed++;
    else r->failed++;
}

int plant_report_total(PlantReport* r) { return r ? r->total : 0; }
int plant_report_passed(PlantReport* r) { return r ? r->passed : 0; }
int plant_report_failed(PlantReport* r) { return r ? r->failed : 0; }

void plant_report_free(PlantReport* r) {
    if (!r) return;
    PlantReportEntry* e = r->head;
    while (e) {
        PlantReportEntry* next = e->next;
        free(e->name);
        free(e->message);
        free(e);
        e = next;
    }
    free(r->suite_name);
    free(r);
}

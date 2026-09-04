/*
 * plant_report.c — v0.49.57: Reporting Subsystem Core
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

/* v0.49.57: Entry iteration accessors for modular exporters.
   The entry struct is defined here (not in the header) so exporters
   only see an opaque handle. */

PlantReportEntry* plant_report_head(PlantReport* r) {
    return r ? r->head : NULL;
}

PlantReportEntry* plant_report_next(PlantReportEntry* e) {
    return e ? e->next : NULL;
}

const char* plant_report_entry_name(PlantReportEntry* e) {
    return e ? (e->name ? e->name : "") : "";
}

int plant_report_entry_passed(PlantReportEntry* e) {
    return e ? e->passed : 0;
}

const char* plant_report_entry_message(PlantReportEntry* e) {
    return e ? (e->message ? e->message : "") : "";
}

const char* plant_report_suite_name(PlantReport* r) {
    return r ? (r->suite_name ? r->suite_name : "unnamed") : "unnamed";
}

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

/* ═══════════════════════════════════════════════════════════════
   v0.49.59a — Abstract Reporting Interface (IReport) Implementation
   Wraps the concrete PlantReport in the IReport vtable so callers
   can interact through the abstract interface without coupling to
   a particular exporter format.
   ═══════════════════════════════════════════════════════════════ */

#include <stdio.h>

/* Forward declarations for format-specific generators */
extern void plant_report_json_emit(PlantReport* r, const char* path);
extern void plant_report_html_emit(PlantReport* r, const char* path);
extern void plant_report_xml_emit(PlantReport* r, const char* path);

static void _ireport_print(void* ctx, const char* message) {
    PlantReport* r = (PlantReport*)ctx;
    if (!r || !message) return;
    printf("[%s] %s\n", plant_report_suite_name(r), message);
}

static void _ireport_summary(void* ctx, int total, int passed, int failed) {
    (void)ctx;
    printf("Total: %d  Passed: %d  Failed: %d\n", total, passed, failed);
}

static void _ireport_begin(void* ctx) {
    PlantReport* r = (PlantReport*)ctx;
    if (r) plant_report_add(r, "__begin__", 1, "suite started");
}

static void _ireport_end(void* ctx) {
    PlantReport* r = (PlantReport*)ctx;
    if (r) plant_report_add(r, "__end__", 1, "suite completed");
}

static void _ireport_add_result(void* ctx, const char* name, int passed, double time) {
    PlantReport* r = (PlantReport*)ctx;
    if (!r || !name) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "%.4fs", time);
    plant_report_add(r, name, passed, buf);
}

/* Helper: write report to a file via the format-specific generator,
   then read the entire file into a heap-allocated string. Returns
   NULL on any failure. */
static char* _report_to_string(PlantReport* r, void (*emit_fn)(PlantReport*, const char*)) {
    if (!r || !emit_fn) return NULL;
    char* buf = NULL;
    size_t buflen = 0;
    FILE* f = open_memstream(&buf, &buflen);
    if (!f) return NULL;

    /* Redirect: write to a temp path, then read back.
       open_memstream is simpler — but the emit functions expect
       a path. Use a temp file approach. */
    fclose(f);
    free(buf);

    const char* tmpl = "/tmp/plant_report_XXXXXX";
    char tmppath[256];
    snprintf(tmppath, sizeof(tmppath), "%s", tmpl);
    int fd = mkstemp(tmppath);
    if (fd < 0) return NULL;
    close(fd);

    emit_fn(r, tmppath);

    /* Read the file into a heap string */
    FILE* rf = fopen(tmppath, "rb");
    if (!rf) { unlink(tmppath); return NULL; }
    fseek(rf, 0, SEEK_END);
    long sz = ftell(rf);
    fseek(rf, 0, SEEK_SET);
    char* result = NULL;
    if (sz > 0) {
        result = (char*)malloc((size_t)sz + 1);
        if (result) {
            size_t rd = fread(result, 1, (size_t)sz, rf);
            result[rd] = '\0';
        }
    } else {
        result = strdup("");
    }
    fclose(rf);
    unlink(tmppath);
    return result;
}

static char* _ireport_to_json(void* ctx) {
    PlantReport* r = (PlantReport*)ctx;
    return _report_to_string(r, plant_report_json_emit);
}

static char* _ireport_to_html(void* ctx) {
    PlantReport* r = (PlantReport*)ctx;
    return _report_to_string(r, plant_report_html_emit);
}

static char* _ireport_to_xml(void* ctx) {
    PlantReport* r = (PlantReport*)ctx;
    return _report_to_string(r, plant_report_xml_emit);
}

IReport* plant_report_default(const char* suite_name) {
    PlantReport* r = plant_report_create(suite_name);
    if (!r) return NULL;

    IReport* rp = (IReport*)malloc(sizeof(IReport));
    if (!rp) { plant_report_free(r); return NULL; }

    rp->context    = r;
    rp->print      = _ireport_print;
    rp->summary    = _ireport_summary;
    rp->to_json    = _ireport_to_json;
    rp->to_html    = _ireport_to_html;
    rp->to_xml     = _ireport_to_xml;
    rp->begin      = _ireport_begin;
    rp->end        = _ireport_end;
    rp->add_result = _ireport_add_result;

    return rp;
}

void plant_report_interface_free(IReport* rp) {
    if (!rp) return;
    if (rp->context) plant_report_free((PlantReport*)rp->context);
    free(rp);
}

/* ── Convenience helpers with null-safety ── */

void plant_iReport_print(IReport* rp, const char* message) {
    if (rp && rp->print) rp->print(rp->context, message);
}

void plant_iReport_summary(IReport* rp, int total, int passed, int failed) {
    if (rp && rp->summary) rp->summary(rp->context, total, passed, failed);
}

char* plant_iReport_to_json(IReport* rp) {
    if (rp && rp->to_json) return rp->to_json(rp->context);
    return NULL;
}

char* plant_iReport_to_html(IReport* rp) {
    if (rp && rp->to_html) return rp->to_html(rp->context);
    return NULL;
}

char* plant_iReport_to_xml(IReport* rp) {
    if (rp && rp->to_xml) return rp->to_xml(rp->context);
    return NULL;
}

void plant_iReport_begin(IReport* rp) {
    if (rp && rp->begin) rp->begin(rp->context);
}

void plant_iReport_end(IReport* rp) {
    if (rp && rp->end) rp->end(rp->context);
}

void plant_iReport_add_result(IReport* rp, const char* name, int passed, double time) {
    if (rp && rp->add_result) rp->add_result(rp->context, name, passed, time);
}

/* ═══════════════════════════════════════════════════════════════
   v0.49.60a — IReport Factory Binding (DIP Compliance)
   PlantReport_create() binds the plant_iReport_* helpers directly
   to the IReport vtable, providing a clean factory pattern that
   decouples callers from the concrete PlantReport implementation.
   ═══════════════════════════════════════════════════════════════ */

static IReport* _default_report = NULL;

IReport* PlantReport_create(void* context) {
    IReport* rep = (IReport*)malloc(sizeof(IReport));
    if (!rep) return NULL;
    rep->context    = context;
    rep->print      = plant_iReport_print;
    rep->summary    = plant_iReport_summary;
    rep->to_json    = plant_iReport_to_json;
    rep->to_html    = plant_iReport_to_html;
    rep->to_xml     = plant_iReport_to_xml;
    rep->begin      = plant_iReport_begin;
    rep->end        = plant_iReport_end;
    rep->add_result = plant_iReport_add_result;
    return rep;
}

void PlantReport_destroy(IReport* rep) {
    if (!rep) return;
    if (rep == _default_report) _default_report = NULL;
    free(rep);
}

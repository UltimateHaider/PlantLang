#ifndef PLANT_REPORT_H
#define PLANT_REPORT_H

#include <plant_types.h>

/* ═══════════════════════════════════════════════════════════════
   v0.49.59a — Abstract Reporting Interface (IReport)
   Polymorphic exporter contract. Defines a unified reporting
   interface supporting direct text logging, statistical summaries,
   serialization (to_json, to_html, to_xml), and structured result
   tracking. Concrete reporters (JSON, HTML, JUnit XML) implement
   these pointers; callers interact through the abstract interface.

   The context pointer carries format-specific state (file handles,
   buffers, tree pointers) so callers never see the internals of
   any particular exporter.
   ═══════════════════════════════════════════════════════════════ */

typedef struct IReport IReport;
struct IReport {
    void* context;
    void  (*print)(void* ctx, const char* message);
    void  (*summary)(void* ctx, int total, int passed, int failed);
    char* (*to_json)(void* ctx);
    char* (*to_html)(void* ctx);
    char* (*to_xml)(void* ctx);
    void  (*begin)(void* ctx);
    void  (*end)(void* ctx);
    void  (*add_result)(void* ctx, const char* name, int passed, double time);
};

/* Default reporter: wraps the existing PlantReport infrastructure.
   Delegates to_json/to_html/to_xml through the format-specific
   generators (plant_report_json_emit, etc.) into heap-allocated
   strings returned to the caller. */
IReport* plant_report_default(const char* suite_name);
void      plant_report_interface_free(IReport* rp);

/* Convenience helpers that null-check before dereferencing vtable
   pointers — safe to call with a NULL IReport*. */
void  plant_iReport_print(IReport* rp, const char* message);
void  plant_iReport_summary(IReport* rp, int total, int passed, int failed);
char* plant_iReport_to_json(IReport* rp);
char* plant_iReport_to_html(IReport* rp);
char* plant_iReport_to_xml(IReport* rp);
void  plant_iReport_begin(IReport* rp);
void  plant_iReport_end(IReport* rp);
void  plant_iReport_add_result(IReport* rp, const char* name, int passed, double time);

#endif

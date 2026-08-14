#ifndef PLANT_TYPES_H
#define PLANT_TYPES_H

/* ── foundational type (v0.48.29) ─────────────────────────────
   Every script value crosses FFI boundaries as an opaque pointer.
   Declared once here so plant_runtime.h and plant_compat.h share a
   single authoritative definition; the typedef must be visible
   before any tx_t-bearing declaration in either header. */
typedef void* tx_t;

#endif /* PLANT_TYPES_H */

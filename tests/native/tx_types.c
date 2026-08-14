/* tx_types probe (v0.48.29): validates that the foundational tx_t
 * typedef is centralized in plant_types.h and remains available from
 * BOTH plant_runtime.h (whose declarations now use tx_t) and
 * plant_compat.h (whose FFI statics take/return tx_t).
 *
 * This TU includes plant_runtime.h directly (the exact chain the
 * runtime implementation uses) and plant_compat.h (the chain every
 * generated program uses); compile success plus the static assertions
 * below prove the decoupled header layering holds. */
#include <plant_runtime.h>
#include <plant_compat.h>
#include <stddef.h>

/* tx_t must be an opaque pointer (same size as void*). */
typedef char tx_t_size_is_void_ptr[(sizeof(tx_t) == sizeof(void*)) ? 1 : -1];

/* plant_runtime.h declares plant_sort/plant_shuffle with tx_t
 * signatures (not raw void*): assigning their addresses to tx_t
 * function-pointer types is a compile-time check of that contract. */
static tx_t (*probe_sort_sig)(tx_t list, tx_t spec) = plant_sort;
static tx_t (*probe_shuffle_sig)(tx_t list) = plant_shuffle;

/* plant_compat.h statics must accept and return tx_t. */
static tx_t probe_compat_call(tx_t s) { return strings_UPPER(s); }

/* plant_runtime.c-style access: the runtime itself builds lists and
 * hands them across the boundary as tx_t. */
static tx_t probe_runtime_chain(tx_t a, tx_t b) {
    tx_t sorted = plant_sort(a, b);
    tx_t shaken = plant_shuffle(sorted);
    return shaken;
}

int main(void) {
    tx_t nil = 0;
    return nil != 0; /* always 0: probe is a compile + link check */
}

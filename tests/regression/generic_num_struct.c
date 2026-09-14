#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_STRUCT_plant_Box_NUM 1
typedef struct {
  long val;
} plant_Box_NUM;
#endif
/*__PLANT_TYPES_END__*/

static plant_Box_NUM plant_map_to_plant_Box_NUM_d(tx_t m, long depth);
static plant_Box_NUM plant_map_to_plant_Box_NUM(tx_t m);
static void* plant_struct_alloc_copy_plant_Box_NUM(plant_Box_NUM v);
static plant_Box_NUM* plant_map_to_ref_plant_Box_NUM(tx_t m);
static tx_t plant_plant_Box_NUM_to_map(plant_Box_NUM v);
static plant_Box_NUM plant_map_to_plant_Box_NUM_d(tx_t m, long depth) {
  plant_Box_NUM r;
  memset(&r, 0, sizeof(r));
  if (depth > 3) { plant_ffi_errno = FFI_ERR_DEPTH; plant_ffi_debug_print("map_to_plant_Box_NUM: depth limit"); return r; }
  if (!m) { plant_ffi_errno = FFI_ERR_TYPE; return r; }
  r.val = (long)(intptr_t)plant_map_get(m, "val");
  return r;
}
static plant_Box_NUM plant_map_to_plant_Box_NUM(tx_t m) { return plant_map_to_plant_Box_NUM_d(m, 1); }
static void* plant_struct_alloc_copy_plant_Box_NUM(plant_Box_NUM v) { plant_Box_NUM* r = (plant_Box_NUM*)plant_alloc(sizeof(plant_Box_NUM)); *r = v; return (void*)r; }
static plant_Box_NUM* plant_map_to_ref_plant_Box_NUM(tx_t m) { return (plant_Box_NUM*)plant_struct_alloc_copy_plant_Box_NUM(plant_map_to_plant_Box_NUM(m)); }
static tx_t plant_plant_Box_NUM_to_map(plant_Box_NUM v) {
  tx_t r = (tx_t)plant_map_hash_create(8);
  plant_map_hash_set(r, "val", (void*)(intptr_t)v.val);
  return r;
}

tx_t main();


tx_t main() {
  tx_t b = "";
  tx_t _ = "";
  tx_t v = "";
  tx_t b2 = "";
  tx_t v2 = "";
    b = ffi_make_box("0");
    ffi_box_write(b, 42);
    v = ffi_box_read(b);
    plant_iReport_print(get_report(), _cat("box=", v));
    b2 = ffi_make_box("1");
    v2 = ffi_box_read(b2);
    plant_iReport_print(get_report(), _cat("seed=", v2));
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_STRUCT_plant_Point 1
typedef struct {
  long x;
  long y;
} plant_Point;
plant_Point ffi_get_point();
#endif
/*__PLANT_TYPES_END__*/

static plant_Point plant_map_to_plant_Point_d(tx_t m, long depth);
static plant_Point plant_map_to_plant_Point(tx_t m);
static void* plant_struct_alloc_copy_plant_Point(plant_Point v);
static plant_Point* plant_map_to_ref_plant_Point(tx_t m);
static tx_t plant_plant_Point_to_map(plant_Point v);
static plant_Point plant_map_to_plant_Point_d(tx_t m, long depth) {
  plant_Point r;
  memset(&r, 0, sizeof(r));
  if (depth > 3) { plant_ffi_errno = FFI_ERR_DEPTH; plant_ffi_debug_print("map_to_plant_Point: depth limit"); return r; }
  if (!m) { plant_ffi_errno = FFI_ERR_TYPE; return r; }
  r.x = (long)(intptr_t)plant_map_get(m, "x");
  r.y = (long)(intptr_t)plant_map_get(m, "y");
  return r;
}
static plant_Point plant_map_to_plant_Point(tx_t m) { return plant_map_to_plant_Point_d(m, 1); }
static void* plant_struct_alloc_copy_plant_Point(plant_Point v) { plant_Point* r = (plant_Point*)plant_alloc(sizeof(plant_Point)); *r = v; return (void*)r; }
static plant_Point* plant_map_to_ref_plant_Point(tx_t m) { return (plant_Point*)plant_struct_alloc_copy_plant_Point(plant_map_to_plant_Point(m)); }
static tx_t plant_plant_Point_to_map(plant_Point v) {
  tx_t r = (tx_t)plant_map_hash_create(8);
  plant_map_hash_set(r, "x", (void*)(intptr_t)v.x);
  plant_map_hash_set(r, "y", (void*)(intptr_t)v.y);
  return r;
}

tx_t main();


tx_t main() {
  tx_t g = "";
  tx_t gx = "";
    plant_ffi_errno = 0;
    g = plant_plant_Point_to_map(ffi_get_point());
    gx = _from_long(plant_map_get ( g , "x" ));
    long gy = 0;
    gy = plant_map_get(g, "y");
    plant_iReport_print(get_report(), _cat4("x=", gx, " y=", _from_long(gy)));
    plant_iReport_print(get_report(), _from_long(gy));
    plant_iReport_print(get_report(), _from_long(gy+1));
    return 0;
}

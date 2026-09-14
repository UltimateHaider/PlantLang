#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Color 1
typedef enum {
  RED,
  GREEN,
  BLUE
} Color;
#define PLANT_STRUCT_plant_Ball 1
typedef struct {
  long weight;
  tx_t color;
} plant_Ball;
plant_Ball ffi_get_ball();
#endif
/*__PLANT_TYPES_END__*/

static plant_Ball plant_map_to_plant_Ball_d(tx_t m, long depth);
static plant_Ball plant_map_to_plant_Ball(tx_t m);
static void* plant_struct_alloc_copy_plant_Ball(plant_Ball v);
static plant_Ball* plant_map_to_ref_plant_Ball(tx_t m);
static tx_t plant_plant_Ball_to_map(plant_Ball v);
static plant_Ball plant_map_to_plant_Ball_d(tx_t m, long depth) {
  plant_Ball r;
  memset(&r, 0, sizeof(r));
  if (depth > 3) { plant_ffi_errno = FFI_ERR_DEPTH; plant_ffi_debug_print("map_to_plant_Ball: depth limit"); return r; }
  if (!m) { plant_ffi_errno = FFI_ERR_TYPE; return r; }
  r.weight = (long)(intptr_t)plant_map_get(m, "weight");
  r.color = plant_map_get(m, "color");
  return r;
}
static plant_Ball plant_map_to_plant_Ball(tx_t m) { return plant_map_to_plant_Ball_d(m, 1); }
static void* plant_struct_alloc_copy_plant_Ball(plant_Ball v) { plant_Ball* r = (plant_Ball*)plant_alloc(sizeof(plant_Ball)); *r = v; return (void*)r; }
static plant_Ball* plant_map_to_ref_plant_Ball(tx_t m) { return (plant_Ball*)plant_struct_alloc_copy_plant_Ball(plant_map_to_plant_Ball(m)); }
static tx_t plant_plant_Ball_to_map(plant_Ball v) {
  tx_t r = (tx_t)plant_map_hash_create(8);
  plant_map_hash_set(r, "weight", (void*)(intptr_t)v.weight);
  plant_map_hash_set(r, "color", v.color);
  return r;
}

tx_t main();


tx_t main() {
  tx_t b = "";
    plant_ffi_errno = 0;
    b = plant_plant_Ball_to_map(ffi_get_ball());
    plant_iReport_print(get_report(), _cat("w=", _from_long ( plant_map_get ( b , "weight" ) )));
    plant_iReport_print(get_report(), _from_enum(plant_map_get ( b , "color" ), "RED,GREEN,BLUE"));
    return 0;
}

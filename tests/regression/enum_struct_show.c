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
#define PLANT_ENUM_State 1
typedef enum {
  IDLE,
  RUN,
  DONE
} State;
#define PLANT_STRUCT_plant_Ball 1
typedef struct {
  long weight;
  tx_t color;
} plant_Ball;
#define PLANT_STRUCT_plant_Game 1
typedef struct {
  tx_t state;
  tx_t color;
} plant_Game;
plant_Ball ffi_get_ball();
plant_Game ffi_get_game();
#endif
/*__PLANT_TYPES_END__*/

static plant_Ball plant_map_to_plant_Ball_d(tx_t m, long depth);
static plant_Ball plant_map_to_plant_Ball(tx_t m);
static void* plant_struct_alloc_copy_plant_Ball(plant_Ball v);
static plant_Ball* plant_map_to_ref_plant_Ball(tx_t m);
static tx_t plant_plant_Ball_to_map(plant_Ball v);
static plant_Game plant_map_to_plant_Game_d(tx_t m, long depth);
static plant_Game plant_map_to_plant_Game(tx_t m);
static void* plant_struct_alloc_copy_plant_Game(plant_Game v);
static plant_Game* plant_map_to_ref_plant_Game(tx_t m);
static tx_t plant_plant_Game_to_map(plant_Game v);
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

static plant_Game plant_map_to_plant_Game_d(tx_t m, long depth) {
  plant_Game r;
  memset(&r, 0, sizeof(r));
  if (depth > 3) { plant_ffi_errno = FFI_ERR_DEPTH; plant_ffi_debug_print("map_to_plant_Game: depth limit"); return r; }
  if (!m) { plant_ffi_errno = FFI_ERR_TYPE; return r; }
  r.state = plant_map_get(m, "state");
  r.color = plant_map_get(m, "color");
  return r;
}
static plant_Game plant_map_to_plant_Game(tx_t m) { return plant_map_to_plant_Game_d(m, 1); }
static void* plant_struct_alloc_copy_plant_Game(plant_Game v) { plant_Game* r = (plant_Game*)plant_alloc(sizeof(plant_Game)); *r = v; return (void*)r; }
static plant_Game* plant_map_to_ref_plant_Game(tx_t m) { return (plant_Game*)plant_struct_alloc_copy_plant_Game(plant_map_to_plant_Game(m)); }
static tx_t plant_plant_Game_to_map(plant_Game v) {
  tx_t r = (tx_t)plant_map_hash_create(8);
  plant_map_hash_set(r, "state", v.state);
  plant_map_hash_set(r, "color", v.color);
  return r;
}

tx_t peek(tx_t p);
tx_t main();


tx_t peek(tx_t p) {
    plant_iReport_print(get_report(), _from_enum(plant_map_get ( p , "state" ), "IDLE,RUN,DONE"));
    plant_iReport_print(get_report(), _from_enum(plant_map_get ( p , "color" ), "RED,GREEN,BLUE"));
  return peek;
}
tx_t main() {
  tx_t b = "";
  tx_t g = "";
  tx_t r = "";
    plant_ffi_errno = 0;
    b = plant_plant_Ball_to_map(ffi_get_ball());
    plant_iReport_print(get_report(), _from_enum(plant_map_get ( b , "color" ), "RED,GREEN,BLUE"));
    plant_ffi_errno = 0;
    g = plant_plant_Game_to_map(ffi_get_game());
    r = peek(g);
    return 0;
}

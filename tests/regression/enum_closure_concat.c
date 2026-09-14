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
#endif
/*__PLANT_TYPES_END__*/

typedef struct {
  tx_t c;
} plant_Env_0;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, tx_t col);

tx_t plant_Closure_0_fn(tx_t env, tx_t col) {
  tx_t c = ((plant_Env_0*)env)->c;
    tx_t cc = BLUE;
    return _cat3(_cat4("p=", _from_enum(col, "RED,GREEN,BLUE"), " cap=", _from_enum(c, "RED,GREEN,BLUE")), " loc=", _from_enum(cc, "RED,GREEN,BLUE"));
}

tx_t main() {
  tx_t r = "";
    tx_t c = GREEN;
    tx_t f = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      __env_0->c = c;
      f = (tx_t)__env_0;
    }
    c = 0;
    r = plant_Closure_0_fn((tx_t)f, RED);
    plant_iReport_print(get_report(), r);
  return main;
}

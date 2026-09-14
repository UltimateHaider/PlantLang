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
#endif
/*__PLANT_TYPES_END__*/

typedef struct {
  tx_t c;
} plant_Env_0;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, tx_t st);

tx_t plant_Closure_0_fn(tx_t env, tx_t st) {
  tx_t c = ((plant_Env_0*)env)->c;
    plant_iReport_print(get_report(), _from_enum(st, "IDLE,RUN,DONE"));
    plant_iReport_print(get_report(), _from_enum(c, "RED,GREEN,BLUE"));
    tx_t cc = RED;
    plant_iReport_print(get_report(), _from_enum(cc, "RED,GREEN,BLUE"));
    return "ok";
}

tx_t main() {
  tx_t r = "";
    tx_t c = BLUE;
    tx_t f = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      __env_0->c = c;
      f = (tx_t)__env_0;
    }
    c = 0;
    r = plant_Closure_0_fn((tx_t)f, RUN);
    return 0;
}

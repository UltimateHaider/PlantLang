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
typedef struct {
  tx_t c2;
} plant_Env_1;
typedef struct {
} plant_Env_2;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, tx_t v);
tx_t plant_Closure_1_fn(tx_t env, tx_t v);
tx_t plant_Closure_2_fn(tx_t env, tx_t col);

tx_t plant_Closure_0_fn(tx_t env, tx_t v) {
  tx_t c = ((plant_Env_0*)env)->c;
  return _cat4("moved:", _from_enum(c, "RED,GREEN,BLUE"), ":", v);
}
tx_t plant_Closure_1_fn(tx_t env, tx_t v) {
  tx_t c2 = *(( tx_t*)((plant_Env_1*)env)->c2);
  return _cat4("ref:", _from_enum(c2, "RED,GREEN,BLUE"), ":", v);
}
tx_t plant_Closure_2_fn(tx_t env, tx_t col) {
  return _cat("param:", _from_enum(col, "RED,GREEN,BLUE"));
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
    r = plant_Closure_0_fn((tx_t)f, "a");
    plant_iReport_print(get_report(), r);
    tx_t c2 = BLUE;
    tx_t g = (tx_t)0;
    { plant_Env_1* __env_1 = (plant_Env_1*)plant_env_alloc(sizeof(plant_Env_1));
      __env_1->c2 = &c2;
      g = (tx_t)__env_1;
    }
    r = plant_Closure_1_fn((tx_t)g, "b");
    plant_iReport_print(get_report(), r);
    c2 = RED;
    r = plant_Closure_1_fn((tx_t)g, "c");
    plant_iReport_print(get_report(), r);
    tx_t h = (tx_t)0;
    { plant_Env_2* __env_2 = (plant_Env_2*)plant_env_alloc(sizeof(plant_Env_2));
      h = (tx_t)__env_2;
    }
    r = plant_Closure_2_fn((tx_t)h, GREEN);
    plant_iReport_print(get_report(), r);
  return main;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

typedef struct {
} plant_Env_0;
typedef struct {
} plant_Env_1;
typedef struct {
} plant_Env_2;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, long x);
tx_t plant_Closure_1_fn(tx_t env, long n);
tx_t plant_Closure_2_fn(tx_t env, long a);

tx_t plant_Closure_0_fn(tx_t env, long x) {
  return x+1;
}
tx_t plant_Closure_1_fn(tx_t env, long n) {
    long acc = n * 3;
    return acc;
}
tx_t plant_Closure_2_fn(tx_t env, long a) {
  return a - 10;
}

tx_t main() {
  tx_t r = "";
  tx_t r2 = "";
  tx_t r3 = "";
    tx_t f = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      f = (tx_t)__env_0;
    }
    r = plant_Closure_0_fn((tx_t)f, 5);
    plant_iReport_print(get_report(), _cat("one=", _from_long ( r )));
    tx_t g = (tx_t)0;
    { plant_Env_1* __env_1 = (plant_Env_1*)plant_env_alloc(sizeof(plant_Env_1));
      g = (tx_t)__env_1;
    }
    r2 = plant_Closure_1_fn((tx_t)g, 7);
    plant_iReport_print(get_report(), _cat("block=", _from_long ( r2 )));
    tx_t h = (tx_t)0;
    { plant_Env_2* __env_2 = (plant_Env_2*)plant_env_alloc(sizeof(plant_Env_2));
      h = (tx_t)__env_2;
    }
    r3 = plant_Closure_2_fn((tx_t)h, 50);
    plant_iReport_print(get_report(), _cat("neg=", _from_long ( r3 )));
    return 0;
}

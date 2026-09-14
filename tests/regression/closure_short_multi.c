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
typedef struct {
} plant_Env_3;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, long x, long y, long z);
tx_t plant_Closure_1_fn(tx_t env, long a, long b);
tx_t plant_Closure_2_fn(tx_t env, long p, long q);
tx_t plant_Closure_3_fn(tx_t env, long u, long v, long w);

tx_t plant_Closure_0_fn(tx_t env, long x, long y, long z) {
  return x+y+z;
}
tx_t plant_Closure_1_fn(tx_t env, long a, long b) {
  return a * b;
}
tx_t plant_Closure_2_fn(tx_t env, long p, long q) {
  return p - q;
}
tx_t plant_Closure_3_fn(tx_t env, long u, long v, long w) {
    long acc = ( u + v ) * w;
    return acc;
}

tx_t main() {
  tx_t r = "";
  tx_t r2 = "";
  tx_t r3 = "";
  tx_t r4 = "";
    tx_t f = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      f = (tx_t)__env_0;
    }
    r = plant_Closure_0_fn((tx_t)f, 1, 2, 3);
    plant_iReport_print(get_report(), _cat("sum=", _from_long ( r )));
    tx_t g = (tx_t)0;
    { plant_Env_1* __env_1 = (plant_Env_1*)plant_env_alloc(sizeof(plant_Env_1));
      g = (tx_t)__env_1;
    }
    r2 = plant_Closure_1_fn((tx_t)g, 6, 7);
    plant_iReport_print(get_report(), _cat("prod=", _from_long ( r2 )));
    tx_t h = (tx_t)0;
    { plant_Env_2* __env_2 = (plant_Env_2*)plant_env_alloc(sizeof(plant_Env_2));
      h = (tx_t)__env_2;
    }
    r3 = plant_Closure_2_fn((tx_t)h, 30, 8);
    plant_iReport_print(get_report(), _cat("diff=", _from_long ( r3 )));
    tx_t k = (tx_t)0;
    { plant_Env_3* __env_3 = (plant_Env_3*)plant_env_alloc(sizeof(plant_Env_3));
      k = (tx_t)__env_3;
    }
    r4 = plant_Closure_3_fn((tx_t)k, 2, 3, 4);
    plant_iReport_print(get_report(), _cat("mul3=", _from_long ( r4 )));
    return 0;
}

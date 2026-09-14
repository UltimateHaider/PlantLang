#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

typedef struct {
} plant_Env_0;
typedef struct {
  long x;
} plant_Env_1;
typedef struct {
} plant_Env_2;
typedef struct {
  long a;
  long b;
} plant_Env_3;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, long x);
tx_t plant_Closure_1_fn(tx_t env, long v);
tx_t plant_Closure_2_fn(tx_t env, long p, long q);
tx_t plant_Closure_3_fn(tx_t env, long w);

tx_t plant_Closure_0_fn(tx_t env, long x) {
  return x * 2;
}
tx_t plant_Closure_1_fn(tx_t env, long v) {
  long x = ((plant_Env_1*)env)->x;
  return v+x;
}
tx_t plant_Closure_2_fn(tx_t env, long p, long q) {
  return p+q;
}
tx_t plant_Closure_3_fn(tx_t env, long w) {
  long a = ((plant_Env_3*)env)->a;
  long b = ((plant_Env_3*)env)->b;
  return w+a+b;
}

tx_t main() {
  tx_t r1 = "";
  tx_t r2 = "";
  tx_t r3 = "";
  tx_t r4 = "";
  tx_t as = "";
  tx_t bs = "";
    tx_t f1 = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      f1 = (tx_t)__env_0;
    }
    r1 = plant_Closure_0_fn((tx_t)f1, 21);
    plant_iReport_print(get_report(), _cat("short=", _from_long ( r1 )));
    long x = 20;
    tx_t f2 = (tx_t)0;
    { plant_Env_1* __env_1 = (plant_Env_1*)plant_env_alloc(sizeof(plant_Env_1));
      __env_1->x = x;
      f2 = (tx_t)__env_1;
    }
    x = 0;
    r2 = plant_Closure_1_fn((tx_t)f2, 1);
    plant_iReport_print(get_report(), _cat("long=", _from_long ( r2 )));
    tx_t f3 = (tx_t)0;
    { plant_Env_2* __env_2 = (plant_Env_2*)plant_env_alloc(sizeof(plant_Env_2));
      f3 = (tx_t)__env_2;
    }
    r3 = plant_Closure_2_fn((tx_t)f3, 10, 32);
    plant_iReport_print(get_report(), _cat("short2=", _from_long ( r3 )));
    long a = 10;
    long b = 32;
    tx_t f4 = (tx_t)0;
    { plant_Env_3* __env_3 = (plant_Env_3*)plant_env_alloc(sizeof(plant_Env_3));
      __env_3->a = a;
      __env_3->b = b;
      f4 = (tx_t)__env_3;
    }
    a = 0;
    b = 0;
    r4 = plant_Closure_3_fn((tx_t)f4, 0);
    plant_iReport_print(get_report(), _cat("long2=", _from_long ( r4 )));
    as = _from_long(a);
    bs = _from_long(b);
    plant_iReport_print(get_report(), _cat4("consumed=", as, ",", bs));
    return 0;
}

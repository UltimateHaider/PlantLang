#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

typedef struct {
} plant_Env_0;
tx_t main();
tx_t plant_Closure_0_fn(tx_t env, long x);

tx_t plant_Closure_0_fn(tx_t env, long x) {
  return x+1;
}

tx_t main() {
  tx_t undefined_var = "";
    plant_iReport_print(get_report(), plant_typeof(_from_long(42)));
    plant_iReport_print(get_report(), plant_typeof("abc"));
    long n = 5;
    plant_iReport_print(get_report(), plant_typeof(_from_long(n)));
    tx_t s = "hello";
    plant_iReport_print(get_report(), plant_typeof(s));
    plant_iReport_print(get_report(), plant_typeof(NULL));
    plant_iReport_print(get_report(), plant_typeof(undefined_var));
    PlantArray* l = plant_list_make ( 3 , "a" , "b" , "c" );
    plant_iReport_print(get_report(), plant_typeof(l));
    PlantArray* m = plant_list_make ( 2 , "k" , "v" );
    plant_iReport_print(get_report(), plant_typeof(m));
    tx_t f = (tx_t)0;
    { plant_Env_0* __env_0 = (plant_Env_0*)plant_env_alloc(sizeof(plant_Env_0));
      f = (tx_t)__env_0;
    }
    plant_iReport_print(get_report(), plant_typeof(f));
    plant_iReport_print(get_report(), _cat("t", plant_typeof(n)));
    plant_iReport_print(get_report(), _cat("u", plant_typeof(NULL)));
    return 0;
}

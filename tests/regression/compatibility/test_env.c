#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_print("env_test_start");
    plant_print(_from_long(1+2));
    plant_print(_from_long(3 * 4));
    plant_print("env_test_end");
  return main;
}

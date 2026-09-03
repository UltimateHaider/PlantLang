#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  plant_verify_begin();
    plant_verify("runtime_compat_check", _from_long(1 == 1));
    plant_print("runtime_ok");
  plant_verify_end();
  return main;
}

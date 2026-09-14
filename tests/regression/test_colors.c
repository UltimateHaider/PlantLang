#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  plant_iRuntime_verify_begin(get_runtime());
    plant_iRuntime_verify(get_runtime(), "one equals one", _from_long(1 == 1));
    plant_iRuntime_verify(get_runtime(), "two equals two", _from_long(2 == 2));
    plant_iRuntime_verify(get_runtime(), "three equals three", _from_long(3 == 3));
  plant_iRuntime_verify_end(get_runtime());
  return main;
}

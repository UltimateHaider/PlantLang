#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iRuntime_suite_setup(get_runtime());
  plant_iRuntime_verify_begin(get_runtime());
    plant_suite_setup_hook("init");
    plant_iRuntime_verify(get_runtime(), "assert true", _from_long(1 == 1));
    plant_suite_teardown_hook("cleanup");
  plant_iRuntime_verify_end(get_runtime());
  plant_iRuntime_suite_teardown(get_runtime());
  return main;
}

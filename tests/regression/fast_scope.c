#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t fast_work();
tx_t main();


tx_t fast_work() {
  if (plant_boundary_block("fast_work", "FAST")) return "";
  plant_fast_enter("fast_work");
    tx_t h = plant_fast_alloc ( 64 );
    plant_fast_reset();
  plant_fast_exit();
  return h;
}
tx_t main() {
    tx_t h = fast_work ( );
    plant_iReport_print(get_report(), _cat("used_after_scope=", ffi_fast_used ( )));
  return main;
}

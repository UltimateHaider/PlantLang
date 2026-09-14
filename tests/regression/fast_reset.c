#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  if (plant_boundary_block("main", "FAST")) return "";
  plant_fast_enter("main");
    tx_t h = plant_fast_alloc ( 64 );
    plant_iReport_print(get_report(), _cat("used_before=", ffi_fast_used ( )));
    plant_fast_reset();
    plant_iReport_print(get_report(), _cat("used_after=", ffi_fast_used ( )));
  plant_fast_reset();
  plant_fast_exit();
  return main;
}

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
    plant_iReport_print(get_report(), _cat("read=", ffi_cap_check ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("net=", ffi_cap_check ( "NET_CONNECT" )));
    plant_iReport_print(get_report(), _cat("sys=", ffi_cap_check ( "SHUTDOWN_ANY" )));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  plant_fast_reset();
  plant_fast_exit();
  return main;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t grant_check();
tx_t store();
tx_t safeleak();
tx_t plant_main();


tx_t grant_check() {
  if (plant_boundary_block("grant_check", "PERSISTENT")) return "";
  plant_persist_enter("grant_check");
    plant_iReport_print(get_report(), _cat("read=", ffi_cap_check ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("write=", ffi_cap_check ( "FILE_WRITE" )));
    plant_iReport_print(get_report(), _cat("net=", ffi_cap_check ( "NET_CONNECT" )));
    plant_iReport_print(get_report(), _cat("listen=", ffi_cap_check ( "NET_LISTEN" )));
  plant_persist_exit();
  return grant_check;
}
tx_t store() {
  if (plant_boundary_block("store", "PERSISTENT")) return "";
  plant_persist_enter("store");
    long o = ffi_arc_alloc ( 8 );
    plant_iReport_print(get_report(), _cat("persist_ok=", ffi_arc_persist ( o )));
  plant_persist_exit();
  return store;
}
tx_t safeleak() {
  if (plant_boundary_block("safeleak", "SAFE")) return "";
  plant_safe_enter("safeleak");
  plant_safe_channel_init("safeleak");
    long o = ffi_arc_alloc ( 8 );
    plant_iReport_print(get_report(), _cat("persist_safe=", ffi_arc_persist ( o )));
  plant_safe_exit();
  return safeleak;
}
tx_t plant_main() {
    grant_check();
    store();
    plant_safe_call("safeleak", 0);
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_safeleak(int argc, tx_t* argv) {
  return (tx_t)safeleak();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("safeleak", plant_safe_adapter_safeleak);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

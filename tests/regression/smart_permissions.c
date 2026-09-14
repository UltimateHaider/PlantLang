#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t sa();
tx_t fa();
tx_t pa();
tx_t ba();
tx_t plant_main();


tx_t sa() {
  if (plant_boundary_block("sa", "SAFE")) return "";
  plant_safe_enter("sa");
  plant_safe_channel_init("sa");
    plant_iReport_print(get_report(), "safe_ok");
  plant_safe_exit();
  return sa;
}
tx_t fa() {
  if (plant_boundary_block("fa", "FAST")) return "";
  plant_fast_enter("fa");
    plant_iReport_print(get_report(), "fast_ok");
  plant_fast_reset();
  plant_fast_exit();
  return fa;
}
tx_t pa() {
  if (plant_boundary_block("pa", "PERSISTENT")) return "";
  plant_persist_enter("pa");
    plant_iReport_print(get_report(), "persist_ok");
  plant_persist_exit();
  return pa;
}
tx_t ba() {
    plant_iReport_print(get_report(), "balanced_ok");
  return ba;
}
tx_t plant_main() {
  if (plant_boundary_block("main", "SMART")) return "";
  plant_smart_enter("main", 0);
    plant_iReport_print(get_report(), _cat("read=", ffi_cap_check ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("write=", ffi_cap_check ( "FILE_WRITE" )));
    plant_iReport_print(get_report(), _cat("net=", ffi_cap_check ( "NET_CONNECT" )));
    plant_safe_call("sa", 0);
    fa();
    pa();
    ba();
    plant_iReport_print(get_report(), "smart_done");
  plant_smart_exit("main");
  return plant_main;
}
tx_t plant_safe_adapter_sa(int argc, tx_t* argv) {
  return (tx_t)sa();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("sa", plant_safe_adapter_sa);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

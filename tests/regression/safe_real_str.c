#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t echo_str(tx_t s);
tx_t plant_main();


tx_t echo_str(tx_t s) {
  if (plant_boundary_block("echo_str", "SAFE")) return "";
  plant_safe_enter("echo_str");
  plant_safe_channel_init("echo_str");
    plant_safe_exit();
  return s;
}
tx_t plant_main() {
  tx_t out = "";
    out = plant_safe_call("echo_str", 1, "real-worker-string");
    plant_iReport_print(get_report(), _cat("out=", out));
    plant_iReport_print(get_report(), _cat("len=", ffi_str_len ( out )));
    plant_iReport_print(get_report(), _cat("eq=", ffi_str_eq ( out , "real-worker-string" )));
    plant_iReport_print(get_report(), ffi_safe_status ( ));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_echo_str(int argc, tx_t* argv) {
  tx_t s = (argc > 0) ? argv[0] : NULL;
  return (tx_t)echo_str(s);
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("echo_str", plant_safe_adapter_echo_str);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

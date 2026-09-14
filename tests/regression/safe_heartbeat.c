#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t beat();
tx_t plant_main();


tx_t beat() {
  if (plant_boundary_block("beat", "SAFE")) return "";
  plant_safe_enter("beat");
  plant_safe_channel_init("beat");
    plant_iReport_print(get_report(), "beat run");
  plant_safe_exit();
  return beat;
}
tx_t plant_main() {
    plant_safe_call("beat", 0);
    plant_iReport_print(get_report(), _cat("stall=", ffi_safe_stall ( "beat" )));
    plant_iReport_print(get_report(), _cat("tick=", ffi_safe_heartbeat_tick ( )));
    plant_safe_call("beat", 0);
    plant_iReport_print(get_report(), ffi_safe_status ( ));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_beat(int argc, tx_t* argv) {
  return (tx_t)beat();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("beat", plant_safe_adapter_beat);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t fast_worker();
tx_t safe_worker();
tx_t plant_main();


tx_t fast_worker() {
  if (plant_boundary_block("fast_worker", "FAST")) return "";
  plant_fast_enter("fast_worker");
    plant_iReport_print(get_report(), "FAST RAN");
  plant_fast_reset();
  plant_fast_exit();
  return fast_worker;
}
tx_t safe_worker() {
  if (plant_boundary_block("safe_worker", "SAFE")) return "";
  plant_safe_enter("safe_worker");
  plant_safe_channel_init("safe_worker");
    plant_iReport_print(get_report(), "SAFE RAN");
  plant_safe_exit();
  return safe_worker;
}
tx_t plant_main() {
  if (plant_boundary_block("main", "FAST")) return "";
  plant_fast_enter("main");
    fast_worker();
    plant_safe_call("safe_worker", 0);
    plant_iReport_print(get_report(), "done");
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  plant_fast_reset();
  plant_fast_exit();
  return plant_main;
}
tx_t plant_safe_adapter_safe_worker(int argc, tx_t* argv) {
  return (tx_t)safe_worker();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("safe_worker", plant_safe_adapter_safe_worker);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

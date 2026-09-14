#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t fast_action();
tx_t smart_action();
tx_t persist_action();
tx_t safe_action();
tx_t plant_main();


tx_t fast_action() {
  if (plant_boundary_block("fast_action", "FAST")) return "";
  plant_fast_enter("fast_action");
    plant_iReport_print(get_report(), "FAST RAN");
  plant_fast_reset();
  plant_fast_exit();
  return fast_action;
}
tx_t smart_action() {
  if (plant_boundary_block("smart_action", "SMART")) return "";
  plant_smart_enter("smart_action", 0);
    plant_iReport_print(get_report(), "SMART RAN");
  plant_smart_exit("smart_action");
  return smart_action;
}
tx_t persist_action() {
  if (plant_boundary_block("persist_action", "PERSISTENT")) return "";
  plant_persist_enter("persist_action");
    plant_iReport_print(get_report(), "PERSISTENT RAN");
  plant_persist_exit();
  return persist_action;
}
tx_t safe_action() {
  if (plant_boundary_block("safe_action", "SAFE")) return "";
  plant_safe_enter("safe_action");
  plant_safe_channel_init("safe_action");
    plant_iReport_print(get_report(), "SAFE RAN");
  plant_safe_exit();
  return safe_action;
}
tx_t plant_main() {
  if (plant_boundary_block("main", "SAFE")) return "";
  plant_safe_enter("main");
  plant_safe_channel_init("main");
    fast_action();
    smart_action();
    persist_action();
    plant_safe_call("safe_action", 0);
    plant_iReport_print(get_report(), "done");
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  plant_safe_exit();
  return plant_main;
}
tx_t plant_safe_adapter_safe_action(int argc, tx_t* argv) {
  return (tx_t)safe_action();
}
tx_t plant_safe_adapter_main(int argc, tx_t* argv) {
  return (tx_t)main();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("safe_action", plant_safe_adapter_safe_action);
  plant_safe_register("main", plant_safe_adapter_main);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

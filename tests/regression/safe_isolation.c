#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t isolated_worker();
tx_t plant_main();


tx_t isolated_worker() {
  if (plant_boundary_block("isolated_worker", "SAFE")) return "";
  plant_safe_enter("isolated_worker");
  plant_safe_channel_init("isolated_worker");
    plant_iReport_print(get_report(), "isolated ran");
  plant_safe_exit();
  return isolated_worker;
}
tx_t plant_main() {
    plant_safe_call("isolated_worker", 0);
    plant_safe_call("isolated_worker", 0);
    plant_iReport_print(get_report(), ffi_safe_status ( ));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_isolated_worker(int argc, tx_t* argv) {
  return (tx_t)isolated_worker();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("isolated_worker", plant_safe_adapter_isolated_worker);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

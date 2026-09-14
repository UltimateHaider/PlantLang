#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t worker();
tx_t plant_main();


tx_t worker() {
  if (plant_boundary_block("worker", "SAFE")) return "";
  plant_safe_enter("worker");
  plant_safe_channel_init("worker");
    plant_iReport_print(get_report(), "w");
  plant_safe_exit();
  return worker;
}
tx_t plant_main() {
    plant_iReport_print(get_report(), _cat("starve=", ffi_safe_starve ( 100 )));
    plant_safe_call("worker", 0);
    plant_iReport_print(get_report(), ffi_safe_status ( ));
    plant_iReport_print(get_report(), _cat("starve2=", ffi_safe_starve ( 100 )));
    plant_safe_call("worker", 0);
    plant_iReport_print(get_report(), ffi_safe_status ( ));
  return plant_main;
}
tx_t plant_safe_adapter_worker(int argc, tx_t* argv) {
  return (tx_t)worker();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("worker", plant_safe_adapter_worker);
  plant_maybe_run_worker();
  plant_async_config("SAFE_POOL_CAPACITY", "1");
  plant_async_config("SAFE_POOL_EXPAND", "2");
  plant_async_config("SAFE_STARVATION_MS", "50");
  plant_main();
  plant_async_drain();
  return 0;
}

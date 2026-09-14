#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
  if (plant_boundary_block("main", "SAFE")) return "";
  plant_safe_enter("main");
  plant_safe_channel_init("main");
    long ch = ffi_safe_channel_open ( );
    plant_iReport_print(get_report(), _cat("send=", ffi_safe_send ( ch , "hello channel" )));
    plant_iReport_print(get_report(), _cat("recv=", ffi_safe_recv ( ch )));
    plant_iReport_print(get_report(), _cat("big=", ffi_safe_send_big ( ch , 1200000 )));
    plant_iReport_print(get_report(), ffi_safe_stats ( ch ));
  plant_safe_exit();
  return plant_main;
}
tx_t plant_safe_adapter_main(int argc, tx_t* argv) {
  return (tx_t)main();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("main", plant_safe_adapter_main);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t big(long n);
tx_t plant_main();


tx_t big(long n) {
  if (plant_boundary_block("big", "SAFE")) return "";
  plant_safe_enter("big");
  plant_safe_channel_init("big");
    plant_safe_exit();
  return ffi_make_big ( n );
}
tx_t plant_main() {
  tx_t out = "";
    out = plant_safe_call("big", 1, _from_long(1572864));
    plant_iReport_print(get_report(), _cat("len=", ffi_str_len ( out )));
    plant_iReport_print(get_report(), _cat("ok=", ffi_big_ok ( out , 1572864 )));
    plant_iReport_print(get_report(), ffi_safe_status ( ));
  return plant_main;
}
tx_t plant_safe_adapter_big(int argc, tx_t* argv) {
  long n = (argc > 0) ? plant_rw_arg_long(argv[0]) : 0;
  return (tx_t)big(n);
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("big", plant_safe_adapter_big);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

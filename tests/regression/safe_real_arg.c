#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t combine(long a, int b, tx_t tag);
tx_t plant_main();


tx_t combine(long a, int b, tx_t tag) {
  if (plant_boundary_block("combine", "SAFE")) return "";
  plant_safe_enter("combine");
  plant_safe_channel_init("combine");
    plant_iReport_print(get_report(), _cat3(_cat4("worker args: ", _from_long(a), " ", _from_long(b)), " ", tag));
    plant_safe_exit();
  return _cat(_cat4(tag, ":", _from_long(a), ":"), _from_long(b));
}
tx_t plant_main() {
  tx_t out = "";
    out = plant_safe_call("combine", 3, _from_long(42), _from_long(7), "k");
    plant_iReport_print(get_report(), _cat("out=", out));
    plant_iReport_print(get_report(), ffi_safe_status ( ));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_combine(int argc, tx_t* argv) {
  long a = (argc > 0) ? plant_rw_arg_long(argv[0]) : 0;
  int b = (argc > 1) ? (int)plant_rw_arg_long(argv[1]) : 0;
  tx_t tag = (argc > 2) ? argv[2] : NULL;
  return (tx_t)combine(a, b, tag);
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("combine", plant_safe_adapter_combine);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}

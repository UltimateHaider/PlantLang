#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t proc(long n);
tx_t plant_main();


tx_t proc(long n) {
  if (plant_boundary_block("proc", "SMART")) return "";
  plant_smart_enter("proc", n);
    plant_iReport_print(get_report(), _cat("q", _from_long ( n )));
  plant_smart_exit("proc");
  return proc;
}
tx_t plant_main() {
    proc(4000);
    plant_iReport_print(get_report(), ffi_smart_status ( ));
    plant_iReport_print(get_report(), _cat("chain=", ffi_audit_chain_verify ( )));
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("SMART_POOL_CAPACITY", "2");
  plant_async_config("SMART_POOL_MAX", "4");
  plant_async_config("SMART_CHUNK_SIZE", "100");
  plant_main();
  plant_async_drain();
  return 0;
}

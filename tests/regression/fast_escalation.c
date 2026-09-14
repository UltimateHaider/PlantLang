#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
  if (plant_boundary_block("main", "FAST")) return "";
  plant_fast_enter("main");
    long i = 0;
    tx_t h = "";
    while (i < 40) {
    h = plant_fast_alloc ( 64 );
    i = i+1;
    }
    plant_iReport_print(get_report(), _cat("escalated=", ffi_fast_escalated ( )));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  plant_fast_reset();
  plant_fast_exit();
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("FAST_HEAP_CAPACITY", "256");
  plant_async_config("FAST_HEAP_LIMIT", "512");
  plant_async_config("FAST_ALIGNMENT", "4");
  plant_main();
  plant_async_drain();
  return 0;
}

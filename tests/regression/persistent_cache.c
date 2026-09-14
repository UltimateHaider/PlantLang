#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t serve();
tx_t plant_main();


tx_t serve() {
  if (plant_boundary_block("serve", "PERSISTENT")) return "";
  plant_persist_enter("serve");
    long o = ffi_arc_alloc ( 64 );
    plant_iReport_print(get_report(), _cat("lease=", ffi_arc_lease ( o , 30 )));
    plant_persist_exit();
  return o;
}
tx_t plant_main() {
  tx_t o = "";
    o = serve();
    plant_iReport_print(get_report(), _cat("retain=", ffi_arc_retain ( o )));
    plant_iReport_print(get_report(), _cat("release1=", ffi_arc_release ( o )));
    plant_iReport_print(get_report(), _cat("release2=", ffi_arc_release ( o )));
    plant_iReport_print(get_report(), _cat("cached=", plant_map_to_string ( ffi_persist_status ( ) )));
    ffi_sleep(50);
    plant_iReport_print(get_report(), _cat("gc=", ffi_arc_gc ( )));
    plant_iReport_print(get_report(), _cat("after=", plant_map_to_string ( ffi_persist_status ( ) )));
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("PERSIST_LEASE_MS", "30");
  plant_main();
  plant_async_drain();
  return 0;
}

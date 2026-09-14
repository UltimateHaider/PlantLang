#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
    long o1 = ffi_arc_alloc ( 64 );
    long o2 = ffi_arc_alloc ( 64 );
    plant_iReport_print(get_report(), _cat("lease1=", ffi_arc_lease ( o1 , 60000 )));
    plant_iReport_print(get_report(), _cat("lease2=", ffi_arc_lease ( o2 , 60000 )));
    plant_iReport_print(get_report(), _cat("rel1=", ffi_arc_release ( o1 )));
    plant_iReport_print(get_report(), _cat("rel2=", ffi_arc_release ( o2 )));
    plant_iReport_print(get_report(), _cat("pressure=", ffi_persist_pressure ( )));
    plant_iReport_print(get_report(), _cat("evict=", ffi_lease_evict ( )));
    plant_iReport_print(get_report(), plant_map_to_string ( ffi_persist_status ( ) ));
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("PERSIST_PRESSURE", "95");
  plant_main();
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
    long a = ffi_arc_alloc ( 32 );
    long b = ffi_arc_alloc ( 32 );
    plant_iReport_print(get_report(), _cat("link1=", ffi_arc_link ( a , b )));
    plant_iReport_print(get_report(), _cat("link2=", ffi_arc_link ( b , a )));
    plant_iReport_print(get_report(), _cat("relA=", ffi_arc_release ( a )));
    plant_iReport_print(get_report(), _cat("relB=", ffi_arc_release ( b )));
    plant_iReport_print(get_report(), _cat("manual=", ffi_arc_gc ( )));
    long k = 0;
    long h = 0;
    while (k < 2500) {
    tx_t h = ffi_arc_alloc ( 16 );
    k = k+1;
    }
    plant_iReport_print(get_report(), _cat("auto=", plant_map_to_string ( ffi_persist_status ( ) )));
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("PERSIST_GC_INTERVAL", "1000");
  plant_main();
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
    long k = 0;
    while (k < 30) {
    long h = ffi_arc_alloc ( 16 );
    k = k+1;
    }
    plant_iReport_print(get_report(), plant_map_to_string ( ffi_persist_status ( ) ));
  return plant_main;
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_async_config("PERSIST_GC_INTERVAL", "3");
  plant_main();
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t a = ffi_arc_alloc ( 16 );
    tx_t b = ffi_arc_alloc ( 16 );
    tx_t f1 = ffi_arc_set_finalizer ( a , "free_data" );
    tx_t f2 = ffi_arc_set_finalizer ( b , "free_data" );
    plant_arc_link((tx_t)a, (tx_t)b);
    plant_arc_unlink((tx_t)a, (tx_t)b);
    tx_t r1 = ffi_arc_release ( a );
    tx_t r2 = ffi_arc_release ( b );
    plant_iReport_print(get_report(), _cat("finalized=", ffi_arc_finalized ( )));
  return main;
}

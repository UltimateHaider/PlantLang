#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t r0 = ffi_dist_init ( 4 );
    tx_t o1 = ffi_dist_alloc ( 32 , "alpha" );
    tx_t o2 = ffi_dist_alloc ( 16 , "beta" );
    tx_t o3 = ffi_dist_alloc ( 8 , "gamma" );
    plant_iReport_print(get_report(), _cat("o1_node=", ffi_dist_node ( o1 )));
    plant_iReport_print(get_report(), _cat("o2_node=", ffi_dist_node ( o2 )));
    plant_iReport_print(get_report(), _cat("o3_node=", ffi_dist_node ( o3 )));
    tx_t r1 = ffi_dist_release ( o1 );
    tx_t r2 = ffi_dist_release ( o2 );
    tx_t r3 = ffi_dist_release ( o3 );
    plant_iReport_print(get_report(), ffi_dist_status ( ));
  return main;
}

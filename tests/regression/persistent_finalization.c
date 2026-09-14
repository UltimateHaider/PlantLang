#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long o = ffi_arc_alloc ( 16 );
    plant_iReport_print(get_report(), _cat("reg=", ffi_arc_set_finalizer ( o , "free_data" )));
    plant_iReport_print(get_report(), _cat("rel=", ffi_arc_release ( o )));
    plant_iReport_print(get_report(), _cat("finalized=", ffi_arc_finalized ( )));
    long p = ffi_arc_alloc ( 16 );
    long q = ffi_arc_alloc ( 16 );
    plant_iReport_print(get_report(), _cat("reg2=", ffi_arc_set_finalizer ( q , "close_ctx" )));
    plant_iReport_print(get_report(), _cat("link=", ffi_arc_link ( p , q )));
    plant_iReport_print(get_report(), _cat("link2=", ffi_arc_link ( q , p )));
    plant_iReport_print(get_report(), _cat("relP=", ffi_arc_release ( p )));
    plant_iReport_print(get_report(), _cat("relQ=", ffi_arc_release ( q )));
    plant_iReport_print(get_report(), _cat("gc=", ffi_arc_gc ( )));
    plant_iReport_print(get_report(), _cat("finalized2=", ffi_arc_finalized ( )));
    plant_iReport_print(get_report(), _cat("chain=", ffi_audit_chain_verify ( )));
  return main;
}

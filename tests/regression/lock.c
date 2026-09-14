#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t x = "shared-resource";
    plant_iReport_print(get_report(), _cat("acquire=", plant_lock ( x )));
    plant_iReport_print(get_report(), _cat("held=", plant_lock_held ( x )));
    plant_iReport_print(get_report(), _cat("double=", plant_lock ( x )));
    plant_iReport_print(get_report(), _cat("held2=", plant_lock_held ( x )));
    plant_iReport_print(get_report(), _cat("release=", plant_lock_release ( x )));
    plant_iReport_print(get_report(), _cat("held3=", plant_lock_held ( x )));
    plant_iReport_print(get_report(), _cat("release_idle=", plant_lock_release ( x )));
    plant_lock((tx_t)x);
    plant_iReport_print(get_report(), _cat("stmt_status=", plant_map_to_string ( plant_lock_status ( ) )));
    plant_iReport_print(get_report(), _cat("rel_stmt=", plant_lock_release ( x )));
    plant_iReport_print(get_report(), _cat("status_final=", plant_map_to_string ( plant_lock_status ( ) )));
    tx_t e = "";
    plant_iReport_print(get_report(), _cat("undef=", plant_lock ( e )));
    plant_iReport_print(get_report(), _cat("undef_release=", plant_lock_release ( e )));
    return 0;
}

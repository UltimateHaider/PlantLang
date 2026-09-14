#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t v = plant_tap( "/tmp/plantlang_vein_test.txt" , "w" );
    plant_iReport_print(get_report(), _cat("infuse=", plant_infuse( v , "hello chloroplast" )));
    plant_iReport_print(get_report(), _cat("seal=", plant_seal( v )));
    tx_t v2 = plant_tap( "/tmp/plantlang_vein_test.txt" , "r" );
    plant_iReport_print(get_report(), _cat("absorb=", plant_absorb( v2 )));
    plant_iReport_print(get_report(), _cat("seal2=", plant_seal( v2 )));
    tx_t v3 = plant_tap( "/tmp/plantlang_vein_test.txt" , "a" );
    plant_iReport_print(get_report(), _cat("append=", plant_infuse( v3 , "!" )));
    plant_iReport_print(get_report(), _cat("seal3=", plant_seal( v3 )));
    tx_t v4 = plant_tap( "/tmp/plantlang_vein_test.txt" , "r" );
    plant_iReport_print(get_report(), _cat("absorb2=", plant_absorb( v4 )));
    plant_iReport_print(get_report(), _cat("seal4=", plant_seal( v4 )));
    return 0;
}

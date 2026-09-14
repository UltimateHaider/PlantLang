#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("abs=", plant_abs(_from_double( - 5 ))));
    plant_iReport_print(get_report(), _cat("round=", plant_round( "3.7" )));
    plant_iReport_print(get_report(), _cat("pow=", plant_pow(_from_double( 2 ), _from_double( 3 ))));
    plant_iReport_print(get_report(), _cat("ceil=", plant_ceil( "3.2" )));
    plant_iReport_print(get_report(), _cat("floor=", plant_floor( "3.9" )));
    plant_iReport_print(get_report(), _cat("sin0=", plant_sin(_from_double( 0 ))));
    plant_iReport_print(get_report(), _cat("cos0=", plant_cos(_from_double( 0 ))));
    plant_iReport_print(get_report(), _cat("sqrt16=", plant_sqrt(_from_double( 16 ))));
    plant_iReport_print(get_report(), _cat("sqrt_neg=", plant_sqrt(_from_double( - 1 ))));
    plant_iReport_print(get_report(), _cat("pow00=", plant_pow(_from_double( 0 ), _from_double( 0 ))));
    tx_t rr = plant_random( );
    if (strcmp(rr,"0") >= 0 && strcmp(rr,"1") < 0) {
    plant_iReport_print(get_report(), "rand_ok=1");
    }
    if (!( strcmp(rr,"0") >= 0 && strcmp(rr,"1") < 0 )) {
    plant_iReport_print(get_report(), "rand_ok=0");
    }
    plant_iReport_print(get_report(), _cat("abs_str=", plant_abs( "-7" )));
    plant_iReport_print(get_report(), _cat("round_neg=", plant_round( "-2.5" )));
    plant_iReport_print(get_report(), _cat("sqrt9=", plant_sqrt(_from_double( 9 ))));
    return 0;
}

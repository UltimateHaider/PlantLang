#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long s = 0;
    for (long i = 1; i <= 10; i += 2) {
    s = s+i;
    }
    plant_iReport_print(get_report(), _cat("odds1-9=", _from_long(s)));
    tx_t s2 = "";
    for (long i = 5; i >= 1; i += - 1) {
    s2 = _cat3(s2, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("down=", s2));
    tx_t s3 = "";
    for (long i = 10; i >= 0; i += - 3) {
    s3 = _cat3(s3, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("down3=", s3));
    long k = 2;
    tx_t s4 = "";
    for (long i = 1; ((k != 0) && (((k > 0) && (i <= 6)) || ((k <= 0) && (i >= 6)))); i += k) {
    s4 = _cat3(s4, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("runtime-step=", s4));
    tx_t hit = "none";
    for (long i = 1; i >= 3; i += - 1) {
    hit = "ran";
    }
    plant_iReport_print(get_report(), _cat("neg-norun=", hit));
    long s5 = 0;
    for (long i = 10; i >= 2; i += - 4) {
    s5 = s5+i;
    }
    plant_iReport_print(get_report(), _cat("sum10-2-4=", _from_long(s5)));
    return 0;
}

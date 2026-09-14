#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t s1 = "";
    for (long i = 1; i <= 5; i += 2) {
    s1 = _cat3(s1, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("std=", s1));
    tx_t s2 = "";
    for (long i = 1; i <= 5; i += 2) {
    s2 = _cat3(s2, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("multisp=", s2));
    tx_t s3 = "";
    for (long i = 1; i <= 5 ; i += 2) {
    s3 = _cat3(s3, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("attached=", s3));
    tx_t s4 = "";
    for (long i = 5; i >= 1; i += - 2) {
    s4 = _cat3(s4, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("neg=", s4));
    tx_t s5 = "";
    for (long i = 5; i >= 1; i += - 2) {
    s5 = _cat3(s5, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("negatt=", s5));
    tx_t s6 = "";
    for (long i = 10; i >= 2; i += - 3) {
    s6 = _cat3(s6, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("neg-spaced=", s6));
    long k = 4;
    tx_t s7 = "";
    for (long i = 1; ((k != 0) && (((k > 0) && (i <= 9)) || ((k <= 0) && (i >= 9)))); i += k) {
    s7 = _cat3(s7, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("runtime=", s7));
    return 0;
}

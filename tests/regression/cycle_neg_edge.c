#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t s1 = "";
    for (long i = 5; i >= 1; i += - 1) {
    s1 = _cat3(s1, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("down5-1=", s1));
    tx_t s2 = "";
    for (long i = 3; i >= 3; i += - 1) {
    s2 = _cat3(s2, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("eq-negstep=", s2));
    tx_t hit1 = "none";
    for (long i = 1; i >= 5; i += - 1) {
    hit1 = "ran";
    }
    plant_iReport_print(get_report(), _cat("mismatch-neg=", hit1));
    tx_t hit2 = "none";
    for (long i = 5; i <= 1; i += 1) {
    hit2 = "ran";
    }
    plant_iReport_print(get_report(), _cat("mismatch-pos=", hit2));
    tx_t s3 = "";
    for (long i = 10; i >= 1; i += 0 - 2) {
    s3 = _cat3(s3, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("fold-neg=", s3));
    tx_t s4 = "";
    for (long i = 1; i <= 4; i += 1 + 1) {
    s4 = _cat3(s4, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("fold-pos=", s4));
    long z = 0;
    tx_t hit3 = "none";
    for (long i = 1; ((z != 0) && (((z > 0) && (i <= 3)) || ((z <= 0) && (i >= 3)))); i += z) {
    hit3 = "ran";
    }
    plant_iReport_print(get_report(), _cat("runtime-zero=", hit3));
    tx_t s5 = "";
    for (long i = 6; i >= 1; i += - 2) {
    if (i == 4) {
                      continue;
    }
    s5 = _cat3(s5, _from_long ( i ), " ");
    }
    plant_iReport_print(get_report(), _cat("neg-cont=", s5));
    tx_t s6 = "";
    for (long i = 6; i >= 1; i += - 1) {
    for (long j = 1; j <= 3; j += 1) {
    if (j == 2) {
                              continue;
    }
    s6 = _cat(_cat4(s6, _from_long ( i ), ",", _from_long ( j )), " ");
    }
    if (i == 4) {
                      break;
    }
    }
    plant_iReport_print(get_report(), _cat("neg-nested=", s6));
    return 0;
}

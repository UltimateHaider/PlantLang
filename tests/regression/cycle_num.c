#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long s = 0;
    for (long i = 1; i <= 5; i += 1) {
    s = s+i;
    }
    plant_iReport_print(get_report(), _cat("sum1-5=", _from_long(s)));
    long b = 3;
    long s2 = 0;
    for (long i = b; i <= b + 3; i += 1) {
    s2 = s2+i;
    }
    plant_iReport_print(get_report(), _cat("sum3-6=", _from_long(s2)));
    for (long i = 1; i <= 1; i += 1) {
    plant_iReport_print(get_report(), _cat("single=", _from_long ( i )));
    }
    tx_t hit = "none";
    for (long i = 5; i <= 1; i += 1) {
    hit = "ran";
    }
    plant_iReport_print(get_report(), _cat("asc-norun=", hit));
    tx_t s3 = "";
    for (long i = 1; i <= 3; i += 1) {
    for (long j = 1; j <= 2; j += 1) {
    s3 = _cat(_cat4(s3, _from_long ( i ), ",", _from_long ( j )), " ");
    }
    }
    plant_iReport_print(get_report(), _cat("nested=", s3));
    for (long i = 1; i <= 5; i += 1) {
    if (i > 2) {
                      break;
    }
    plant_iReport_print(get_report(), _cat("brk=", _from_long ( i )));
    }
    for (long i = 1; i <= 5; i += 1) {
    if (i == 2) {
                      continue;
    }
    plant_iReport_print(get_report(), _cat("con=", _from_long ( i )));
    }
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long a = 10;
    a += 5;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a -= 3;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a += - 2;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a -= - 4;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a += 0;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a += a - 2;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a += a * 2;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    a -= 2 * a;
    plant_iReport_print(get_report(), _cat("a=", _from_long(a)));
    return 0;
}

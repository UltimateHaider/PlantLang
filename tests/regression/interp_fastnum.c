#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 42;
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(0), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(1), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(2), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(3), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(4), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(5), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(6), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(7), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(8), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_digit(9), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_long(5+4), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_long(10), ""));
    plant_iReport_print(get_report(), _cat3("d=", _from_long(n), ""));
    plant_iReport_print(get_report(), _cat("d=", _from_digit(5)));
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 42;
    tx_t s = "world";
    long eb2 = 7;
    plant_iReport_print(get_report(), _cat3("hello ", _from_long(n), ""));
    plant_iReport_print(get_report(), _cat3("n=", _from_long(n), ""));
    plant_iReport_print(get_report(), _cat3("s=", s, ""));
    plant_iReport_print(get_report(), _cat3("eb2=", _from_long(eb2), ""));
    plant_iReport_print(get_report(), _cat3("a", s, "b"));
    plant_iReport_print(get_report(), _cat(_cat4("mixed ", _from_long(n), " and ", s), ""));
    plant_iReport_print(get_report(), _cat3("empty=", "", " done"));
    plant_iReport_print(get_report(), _cat3("neg=", _from_long(-n), ""));
    plant_iReport_print(get_report(), _cat3("zero=", _from_digit(0), ""));
    plant_iReport_print(get_report(), _cat3("pos=", _from_long(n+1), ""));
    plant_iReport_print(get_report(), _cat3("arith=", _from_long(n+eb2 - 1), ""));
    plant_iReport_print(get_report(), _cat3("len=", _from_long(strlen(s)), ""));
    plant_iReport_print(get_report(), _cat3("nested=", _from_long(n+n), ""));
    return 0;
}

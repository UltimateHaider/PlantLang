#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 9;
    tx_t s = "x";
    plant_iReport_print(get_report(), _cat4(_cat4(_cat4("a", "b", "c", "d"), "e", "f", "g"), "h", "i", "j"));
    plant_iReport_print(get_report(), _cat(_cat4("p", s, "q", s), "r"));
    plant_iReport_print(get_report(), _cat4(_cat4(_cat4(_cat4("", _from_long(n), "", _from_long(n)), "", _from_long(n), ""), _from_long(n), "", _from_long(n)), "", _from_long(n), ""));
    plant_iReport_print(get_report(), _cat3(_cat4("1", "2", "3", "4"), "5", "6"));
    plant_iReport_print(get_report(), _cat("n=", _from_long(n)));
    return 0;
}

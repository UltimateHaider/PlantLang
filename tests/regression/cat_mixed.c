#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n2 = 7;
    plant_iReport_print(get_report(), _cat("x", _from_digit(2)));
    plant_iReport_print(get_report(), _cat("n=", _from_long(n2)));
    plant_iReport_print(get_report(), _cat3("n2=", _from_long(n2), "!"));
    tx_t eb2 = "z";
    plant_iReport_print(get_report(), _cat("tag", eb2));
    return 0;
}

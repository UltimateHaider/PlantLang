#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long a = 10;
    long b = 20;
    long c = a+b;
    plant_iReport_print(get_report(), _from_long(c));
    plant_iReport_print(get_report(), _from_long(a+b+5));
    tx_t t = _cat("sum ", _from_long(c));
    plant_iReport_print(get_report(), t);
    return 0;
}

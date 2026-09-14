#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long x = 42;
    plant_iReport_print(get_report(), _from_long(x));
    plant_iReport_print(get_report(), _from_long(x+1));
    plant_iReport_print(get_report(), _from_long(x * 2 - 1));
    plant_iReport_print(get_report(), _from_long(strlen( "hello world" )));
    plant_iReport_print(get_report(), "12");
    plant_iReport_print(get_report(), _from_long(7));
    long f = 2;
    plant_iReport_print(get_report(), _from_long(f+x));
    return 0;
}

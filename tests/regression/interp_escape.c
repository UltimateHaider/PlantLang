#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 42;
    plant_iReport_print(get_report(), "lit \\${x}");
    plant_iReport_print(get_report(), _cat3("mix \\${x} and ", _from_long(1+2), ""));
    plant_iReport_print(get_report(), "only \\${a} \\${b}");
    plant_iReport_print(get_report(), _cat3("tail ", _from_long(n), " \\${y}"));
    plant_iReport_print(get_report(), _cat3("a\\b ", _from_long(n), ""));
    return 0;
}

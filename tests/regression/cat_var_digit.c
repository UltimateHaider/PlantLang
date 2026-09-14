#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t eb2 = "world";
    tx_t v2 = "yay";
    plant_iReport_print(get_report(), _cat("hello ", eb2));
    plant_iReport_print(get_report(), _cat("prefix", v2));
    return 0;
}

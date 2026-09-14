#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t helper();
tx_t main();


tx_t helper() {
    plant_iReport_print(get_report(), "from helper");
    return 0;
}
tx_t main() {
    helper();
    plant_iReport_print(get_report(), "after imports");
    return 0;
}

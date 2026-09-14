#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t eb2 = "foo";
    tx_t eb3 = "bar";
    plant_iReport_print(get_report(), _cat(eb2, eb3));
    tx_t s2 = "num";
    tx_t s3 = "ber";
    plant_iReport_print(get_report(), _cat(s2, s3));
    return 0;
}

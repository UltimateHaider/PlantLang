#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_eval_to_str(" 1 + 2 "));
    plant_iReport_print(get_report(), plant_math_eval_to_str(" 3 * 7 + 1 "));
    return 0;
}

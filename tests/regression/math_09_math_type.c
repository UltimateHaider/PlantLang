#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    void* expr = plant_math_create("2 + 3 * 4");
    plant_iReport_print(get_report(), plant_math_value_str( expr ));
    void* expr2 = plant_math_create("PI * 2");
    plant_iReport_print(get_report(), plant_math_value_str( expr2 ));
    void* expr3 = plant_math_create("plant_sqrt(_from_double(144))");
    plant_iReport_print(get_report(), plant_math_value_str( expr3 ));
    return 0;
}

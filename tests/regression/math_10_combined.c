#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_eval_to_str( "2 ^ 10 - 1" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "plant_sqrt(3^2 + 4^2)" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "PI * 2 ^ 2" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "math_exp(_from_double(1)) * 2" ));
    void* combined = plant_math_create("plant_sin(PI / 2) + plant_cos(_from_double(0))");
    plant_iReport_print(get_report(), plant_math_value_str( combined ));
    return 0;
}

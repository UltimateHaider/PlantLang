#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_eval_to_str( "plant_sin(_from_double(0))" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "plant_cos(_from_double(0))" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "plant_sqrt(_from_double(16))" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "plant_abs(_from_double(-5))" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "math_exp(_from_double(0))" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "math_log(_from_double(1))" ));
    return 0;
}

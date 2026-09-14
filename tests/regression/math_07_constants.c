#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_eval_to_str( "PI" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "E" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "TAU" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "PHI" ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( "SQRT2" ));
    return 0;
}

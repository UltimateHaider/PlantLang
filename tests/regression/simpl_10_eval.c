#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_eval_to_str( plant_math_simplify_str( "2 + 3" ) ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( plant_math_simplify_str( "PI * 2" ) ));
    plant_iReport_print(get_report(), plant_math_eval_to_str( plant_math_simplify_str( "plant_sqrt(_from_double(16))" ) ));
    plant_iReport_print(get_report(), plant_math_simplify_str( "10 + 0" ));
    return 0;
}

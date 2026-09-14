#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_math_subst_str( "x^2 + y^2" , "x" , "3" ));
    plant_iReport_print(get_report(), plant_math_subst_str( "a*x + b" , "a" , "5" ));
    return 0;
}

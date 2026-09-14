#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long x = 41;
    tx_t m = "xx";
    plant_iReport_print(get_report(), _cat("x=", _from_long(x)));
    plant_iReport_print(get_report(), _cat(_from_long(x), "!"));
    plant_iReport_print(get_report(), _cat(_cat4("multi ", m, " ", _from_long(x)), "."));
    plant_iReport_print(get_report(), _cat("expr ", _from_long(( x + 1 ))));
    plant_iReport_print(get_report(), _cat("len ", _from_long(strlen( m ))));
    return 0;
}

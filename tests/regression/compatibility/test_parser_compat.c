#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long x = 1;
    x = x+1;
    plant_print(_cat("x=", _from_long(x)));
    if (x == 2) {
    plant_print("equal");
    }
    while (x < 5) {
    plant_print(_cat("loop=", _from_long(x)));
    x = x+1;
    }
    return "done";
}

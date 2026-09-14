#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t add(long a, long b);
tx_t main();


tx_t add(long a, long b) {
    return a+b;
}
tx_t main() {
  tx_t r = "";
  tx_t s = "";
    r = add(30, 12);
    long n = 0;
    n = r;
    s = _from_long(n);
    plant_iReport_print(get_report(), _cat("num=", s));
    plant_iReport_print(get_report(), _from_long(n));
    plant_iReport_print(get_report(), _from_long(n * 2));
    return 0;
}

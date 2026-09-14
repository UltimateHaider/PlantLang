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
    long n = 2+3;
    plant_iReport_print(get_report(), _from_long(n));
    r = add(30, 12);
    long n1 = 0;
    n1 = r;
    plant_iReport_print(get_report(), _from_long(n1));
    long n2 = 7;
    plant_iReport_print(get_report(), _from_long(n2+1));
    plant_iReport_print(get_report(), _from_long(n2 * 2));
    return 0;
}

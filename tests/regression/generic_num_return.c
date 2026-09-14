#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t plant_max2_NUM(long a, long b);
tx_t plant_add_NUM(long a, long b);
tx_t plant_add_FACT(int a, int b);


tx_t main() {
  tx_t m = "";
  tx_t n = "";
  tx_t s = "";
  tx_t f = "";
    m = plant_max2_NUM(15, 9);
    plant_iReport_print(get_report(), m);
    n = plant_max2_NUM(3, 8);
    plant_iReport_print(get_report(), _cat("max=", n));
    s = plant_add_NUM(30, 12);
    plant_iReport_print(get_report(), _cat("sum=", s));
    f = plant_add_FACT(2, 3);
    plant_iReport_print(get_report(), _cat("f=", f));
    return 0;
}
tx_t plant_max2_NUM(long a, long b) {
    if (a > b) {
    return _from_long(a);
    }
    return _from_long(b);
}
tx_t plant_add_NUM(long a, long b) {
    return _from_long(a+b);
}
tx_t plant_add_FACT(int a, int b) {
    return _from_long(a+b);
}

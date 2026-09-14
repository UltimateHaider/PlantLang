#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t plant_max2_NUM(long a, long b);
tx_t plant_echo_TX(tx_t v);


tx_t main() {
  tx_t m = "";
  tx_t s = "";
    m = plant_max2_NUM(15, 9);
    long m2 = 0;
    m2 = m;
    plant_iReport_print(get_report(), _from_long(m2));
    plant_iReport_print(get_report(), _from_long(m2+1));
    s = plant_echo_TX("cd");
    plant_iReport_print(get_report(), _cat("str=", s));
    return 0;
}
tx_t plant_max2_NUM(long a, long b) {
    if (a > b) {
    return a;
    }
    return b;
}
tx_t plant_echo_TX(tx_t v) {
    return v;
}

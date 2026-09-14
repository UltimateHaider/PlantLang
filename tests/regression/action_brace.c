#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t twice(long n);
tx_t empty();
tx_t comma_legacy();
tx_t thrice(long n);


tx_t main() {
  tx_t _ = "";
    plant_iReport_print(get_report(), "brace main");
    long x = 6;
    if (x > 5) {
    plant_iReport_print(get_report(), "big");
    }
    long i = 0;
    while (i < 3) {
    plant_iReport_print(get_report(), "tick");
    i = i+1;
    }
    plant_iReport_print(get_report(), twice ( 21 ));
    empty();
    comma_legacy();
    plant_iReport_print(get_report(), thrice ( 3 ));
    return 0;
}
tx_t twice(long n) {
    return _from_long(n * 2);
}
tx_t empty() {
  return "";
}
tx_t comma_legacy() {
    plant_iReport_print(get_report(), "comma form still works");
    plant_iReport_print(get_report(), thrice ( 3 ));
    return 0;
}
tx_t thrice(long n) {
    return _from_long(n * 3);
}

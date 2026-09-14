#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t sign(long n);
tx_t classify(long n);
tx_t main();


tx_t sign(long n) {
    if (n > 0) {
    return "positive";
    } else {
    return "negative";
    }
  return sign;
}
tx_t classify(long n) {
    if (n >= 90) {
    return "A";
    } else {
    if (n >= 80) {
    return "B";
    } else {
    return "C";
    }
    }
  return classify;
}
tx_t main() {
    plant_iReport_print(get_report(), sign ( 42 ));
    plant_iReport_print(get_report(), sign ( - 3 ));
    plant_iReport_print(get_report(), classify ( 95 ));
    plant_iReport_print(get_report(), classify ( 85 ));
    plant_iReport_print(get_report(), classify ( 70 ));
    long n = 7;
    if (n > 5) {
    long x = 1;
    x = x+1;
    plant_iReport_print(get_report(), _cat("big", _from_long(x)));
    } else {
    plant_iReport_print(get_report(), "small");
    }
    tx_t s = "hi";
    if (strcmp(s,"hi") == 0) {
    plant_iReport_print(get_report(), "greeting");
    } else {
    plant_iReport_print(get_report(), "unknown");
    }
    if (n > 100) {
    plant_iReport_print(get_report(), "huge");
    } else {
    plant_iReport_print(get_report(), "plain-else");
    }
    return 0;
}

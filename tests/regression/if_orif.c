#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t grade(long n);
tx_t mood(tx_t w);
tx_t main();


tx_t grade(long n) {
    if (n >= 90) {
    return "A";
    } else if (n >= 80) {
    return "B";
    } else if (n >= 70) {
    return "C";
    } else if (n >= 60) {
    return "D";
    } else if (n >= 60) {
    return "D";
    } else {
    return "F";
    }
  return grade;
}
tx_t mood(tx_t w) {
    if (strcmp(w,"sunny") == 0) {
    return "happy";
    } else if (strcmp(w,"rainy") == 0) {
    return "cozy";
    } else if (strcmp(w,"rainy") == 0) {
    return "cozy";
    } else {
    return "meh";
    }
  return mood;
}
tx_t main() {
    plant_iReport_print(get_report(), grade ( 95 ));
    plant_iReport_print(get_report(), grade ( 85 ));
    plant_iReport_print(get_report(), grade ( 75 ));
    plant_iReport_print(get_report(), grade ( 65 ));
    plant_iReport_print(get_report(), grade ( 10 ));
    long n = 5;
    if (n > 10) {
    plant_iReport_print(get_report(), "big");
    } else if (n > 7) {
    plant_iReport_print(get_report(), "mid");
    } else if (n > 3) {
    plant_iReport_print(get_report(), "small");
    }
    if (n > 100) {
    plant_iReport_print(get_report(), "never");
    } else if (n > 200) {
    plant_iReport_print(get_report(), "still-never");
    }
    plant_iReport_print(get_report(), mood ( "sunny" ));
    plant_iReport_print(get_report(), mood ( "rainy" ));
    plant_iReport_print(get_report(), mood ( "windy" ));
    return 0;
}

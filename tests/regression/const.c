#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "const start");
    static const char *city = "atlantis";
    plant_iReport_print(get_report(), city);
    if (1) {
        static const char *city = "inner-city";
        static const char *depth = "deep";
    plant_iReport_print(get_report(), city);
    plant_iReport_print(get_report(), _cat3(city, "-", depth));
    }
    plant_iReport_print(get_report(), city);
    while (0) {
        static const char *never = "nope";
    plant_iReport_print(get_report(), _cat(city, "-loop"));
    }
    for (long i = 1; i <= 2; i += 1) {
    plant_iReport_print(get_report(), city);
    }
    plant_iReport_print(get_report(), "const done");
    return 0;
}

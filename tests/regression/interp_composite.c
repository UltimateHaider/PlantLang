#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 5;
    tx_t s = "B";
    tx_t r = "";
    plant_iReport_print(get_report(), _cat("x=", _cat3("a", _from_long(n), "b")));
    plant_iReport_print(get_report(), _cat3("esc=\"", s, "\""));
    plant_iReport_print(get_report(), "nl=a\nb");
    plant_iReport_print(get_report(), "quo=\"q\"");
    r = _cat3("r=", _from_long(n), "");
    plant_iReport_print(get_report(), r);
    r = _cat3("pre ", s, " post");
    plant_iReport_print(get_report(), r);
    plant_iReport_print(get_report(), _cat(_cat4("multi=", _from_long(n), "x", _from_long(n)), ""));
    plant_iReport_print(get_report(), _cat3("cond=", _from_long(n+2 * n), ""));
    plant_iReport_print(get_report(), _cat3("quoted=", _cat(s, _cat3("", s, "")), ""));
    return 0;
}

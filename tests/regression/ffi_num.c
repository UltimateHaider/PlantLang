#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t s0 = "";
    s0 = ffi_add(2, 3);
    plant_iReport_print(get_report(), _cat("add=", s0));
    long c = 0;
    c = ffi_parse_cfg("ok");
    plant_iReport_print(get_report(), _cat("cfg=", _from_long(c)));
    plant_iReport_print(get_report(), _from_long(c));
    long e = 0;
    e = ffi_last_error();
    plant_iReport_print(get_report(), _cat("err=", _from_long(e)));
    plant_iReport_print(get_report(), _from_long(e));
    return 0;
}

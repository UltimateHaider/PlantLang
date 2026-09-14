#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Color 1
typedef enum {
  RED,
  GREEN,
  BLUE
} Color;
#endif
/*__PLANT_TYPES_END__*/

tx_t pick(long n);
tx_t main();


tx_t pick(long n) {
    if (n == 0) {
    return _from_enum(RED, "RED,GREEN,BLUE");
    }
    return _from_enum(GREEN, "RED,GREEN,BLUE");
}
tx_t main() {
  tx_t r1 = "";
  tx_t c = "";
  tx_t r2 = "";
  tx_t p = "";
  tx_t r3 = "";
  tx_t r4 = "";
    r1 = ffi_is_green(_to_enum(GREEN, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("literal=", r1));
    c = _from_enum(ffi_color(), "RED,GREEN,BLUE");
    r2 = ffi_is_green(_to_enum(c, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("var=", r2));
    p = pick(1);
    r3 = ffi_is_green(_to_enum(p, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("action=", r3));
    r4 = ffi_is_green(_to_enum(0, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("int=", r4));
    return 0;
}

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

tx_t main();
tx_t plant_pick_Color(tx_t n);


tx_t main() {
  tx_t p = "";
  tx_t z = "";
  tx_t c = "";
  tx_t r = "";
  tx_t r2 = "";
    p = plant_pick_Color(1);
    plant_iReport_print(get_report(), _cat("p=", p));
    z = plant_pick_Color(0);
    plant_iReport_print(get_report(), _cat("z=", z));
    c = _from_enum(ffi_color(), "RED,GREEN,BLUE");
    plant_iReport_print(get_report(), _cat("c=", _from_enum(c, "RED,GREEN,BLUE")));
    r = ffi_is_green(_to_enum(c, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("green=", r));
    r2 = ffi_is_green(_to_enum(GREEN, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("g2=", r2));
    return 0;
}
tx_t plant_pick_Color(tx_t n) {
    if (n == 0) {
    return _from_enum(RED, "RED,GREEN,BLUE");
    }
    return _from_enum(GREEN, "RED,GREEN,BLUE");
}

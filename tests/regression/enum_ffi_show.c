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


tx_t main() {
  tx_t c = "";
  tx_t i = "";
    plant_iReport_print(get_report(), _from_enum(ffi_color ( ), "RED,GREEN,BLUE"));
    c = _from_enum(ffi_color(), "RED,GREEN,BLUE");
    plant_iReport_print(get_report(), _from_enum(c, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("c=", _from_enum(c, "RED,GREEN,BLUE")));
    i = ffi_color_idx(_to_enum(GREEN, "RED,GREEN,BLUE"));
    plant_iReport_print(get_report(), _cat("idx=", i));
    return 0;
}

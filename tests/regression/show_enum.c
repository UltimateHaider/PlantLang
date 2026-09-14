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
    tx_t c = GREEN;
    plant_iReport_print(get_report(), _from_enum(c, "RED,GREEN,BLUE"));
    tx_t d = BLUE;
    plant_iReport_print(get_report(), _from_enum(d, "RED,GREEN,BLUE"));
    tx_t e = RED;
    plant_iReport_print(get_report(), _from_enum(e, "RED,GREEN,BLUE"));
    return 0;
}

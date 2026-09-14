#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Level 1
typedef enum {
  LOW,
  MEDIUM,
  HIGH
} Level;
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t plant_report_Level(tx_t v);


tx_t main() {
  tx_t r = "";
    tx_t lv = HIGH;
    r = plant_report_Level(lv);
    return 0;
}
tx_t plant_report_Level(tx_t v) {
    tx_t mine = v;
    plant_iReport_print(get_report(), _from_enum(mine, "LOW,MEDIUM,HIGH"));
    plant_iReport_print(get_report(), _from_enum(v, "LOW,MEDIUM,HIGH"));
    return 0;
}

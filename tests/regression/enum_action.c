#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Player 1
typedef enum {
  ALICE,
  BOB,
  CAD
} Player;
#endif
/*__PLANT_TYPES_END__*/

tx_t describe(tx_t id);
tx_t main();


tx_t describe(tx_t id) {
    plant_iReport_print(get_report(), _cat("player ", _from_enum(id, "ALICE,BOB,CAD")));
    return 0;
}
tx_t main() {
  tx_t rr = "";
  tx_t rr2 = "";
    tx_t who = BOB;
    plant_iReport_print(get_report(), _from_enum(who, "ALICE,BOB,CAD"));
    rr = describe(_to_enum(who, "ALICE,BOB,CAD"));
    rr2 = describe(_to_enum(CAD, "ALICE,BOB,CAD"));
    return 0;
}

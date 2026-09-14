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
#define PLANT_ENUM_Suit 1
typedef enum {
  HEARTS,
  SPADES,
  DIAMONDS,
  CLUBS
} Suit;
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t plant_ped_Color(tx_t v);
tx_t plant_ped_Suit(tx_t v);


tx_t main() {
  tx_t r1 = "";
  tx_t r2 = "";
    tx_t c = GREEN;
    tx_t s = SPADES;
    r1 = plant_ped_Color(c);
    r2 = plant_ped_Suit(s);
    return 0;
}
tx_t plant_ped_Color(tx_t v) {
    plant_iReport_print(get_report(), _from_enum(v, "RED,GREEN,BLUE"));
    return _from_enum(v, "RED,GREEN,BLUE");
}
tx_t plant_ped_Suit(tx_t v) {
    plant_iReport_print(get_report(), _from_enum(v, "HEARTS,SPADES,DIAMONDS,CLUBS"));
    return _from_enum(v, "HEARTS,SPADES,DIAMONDS,CLUBS");
}

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
tx_t plant_tag_Color(tx_t v);
tx_t plant_pair_Color_Suit(tx_t a, tx_t b);


tx_t main() {
  tx_t r1 = "";
  tx_t r2 = "";
    tx_t c = BLUE;
    tx_t s = DIAMONDS;
    r1 = plant_tag_Color(c);
    r2 = plant_pair_Color_Suit(c, s);
    return 0;
}
tx_t plant_tag_Color(tx_t v) {
    plant_iReport_print(get_report(), _cat3("tag:", _from_enum(v, "RED,GREEN,BLUE"), "!"));
    return 0;
}
tx_t plant_pair_Color_Suit(tx_t a, tx_t b) {
    plant_iReport_print(get_report(), _cat3(_from_enum(a, "RED,GREEN,BLUE"), "/", _from_enum(b, "HEARTS,SPADES,DIAMONDS,CLUBS")));
    return 0;
}

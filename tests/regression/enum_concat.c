#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
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


tx_t main() {
    tx_t s = SPADES;
    plant_iReport_print(get_report(), _cat("suit=", _from_enum(s, "HEARTS,SPADES,DIAMONDS,CLUBS")));
    plant_iReport_print(get_report(), _cat3("result: ", _from_enum(s, "HEARTS,SPADES,DIAMONDS,CLUBS"), "!"));
    tx_t d = DIAMONDS;
    plant_iReport_print(get_report(), _cat(_from_enum(d, "HEARTS,SPADES,DIAMONDS,CLUBS"), " is valid"));
    return 0;
}

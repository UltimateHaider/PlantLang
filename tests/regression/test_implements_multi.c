#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t make_sound(tx_t self);
tx_t main();


tx_t make_sound(tx_t self) {
    return _cat(_map_get(self, "name"), " makes sound!");
}
tx_t main() {
  tx_t chk1 = "";
  tx_t chk2 = "";
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
    chk1 = plant_is_a(d, "A");
    chk2 = plant_is_a(d, "B");
    plant_iReport_print(get_report(), _cat("is A: ", chk1));
    plant_iReport_print(get_report(), _cat("is B: ", chk2));
    return 0;
}

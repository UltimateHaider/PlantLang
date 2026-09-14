#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t make_sound(tx_t self);
tx_t main();


tx_t make_sound(tx_t self) {
    return _cat4(_map_get(self, "name"), " the ", _map_get(self, "breed"), " barks!");
}
tx_t main() {
  tx_t s1 = "";
  tx_t s2 = "";
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
  plant_map_set(d, "breed" , "Lab");
    s1 = plant_is_a(d, "Animal");
    s2 = plant_is_a(d, "Dog");
    plant_iReport_print(get_report(), _cat("is Animal: ", s1));
    plant_iReport_print(get_report(), _cat("is Dog: ", s2));
    return 0;
}

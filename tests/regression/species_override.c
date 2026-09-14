#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t speak(tx_t self);
tx_t describe(tx_t self);
tx_t fetch(tx_t self);
tx_t main();


tx_t speak(tx_t self) {
    return _cat(_map_get(self, "name"), " makes a sound");
}
tx_t describe(tx_t self) {
    return _cat("I am ", _map_get(self, "name"));
}
tx_t fetch(tx_t self) {
    return _cat4(_map_get(self, "name"), " the ", _map_get(self, "breed"), " fetches!");
}
tx_t main() {
    tx_t a = plant_species_create_by_name("Animal");
  plant_map_set(a, "name" , "Generic");
    plant_iReport_print(get_report(), speak(a));
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
  plant_map_set(d, "breed" , "Lab");
    plant_iReport_print(get_report(), _from_long(plant_array_length(d)));
    plant_iReport_print(get_report(), speak(d));
    plant_iReport_print(get_report(), fetch(d));
    plant_iReport_print(get_report(), describe(d));
    return 0;
}

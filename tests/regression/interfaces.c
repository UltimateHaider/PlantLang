#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t speak(tx_t self);
tx_t meow(tx_t self);
tx_t main();


tx_t speak(tx_t self) {
    return _cat(_map_get(self, "name"), " barks!");
}
tx_t meow(tx_t self) {
    return _cat(_map_get(self, "name"), " meows!");
}
tx_t main() {
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
    plant_iReport_print(get_report(), speak(d));
    plant_iReport_print(get_report(), _from_long(plant_array_length(d)));
    tx_t c = plant_species_create_by_name("Cat");
  plant_map_set(c, "name" , "Whiskers");
    plant_iReport_print(get_report(), meow(c));
    plant_iReport_print(get_report(), _from_long(plant_array_length(c)));
    plant_iReport_print(get_report(), "interfaces ok");
    return 0;
}

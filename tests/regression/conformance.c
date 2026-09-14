#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t speak(tx_t self);
tx_t move(tx_t self);
tx_t main();


tx_t speak(tx_t self) {
    return _cat(_map_get(self, "name"), " beeps!");
}
tx_t move(tx_t self) {
    return _cat(_map_get(self, "name"), " rolls!");
}
tx_t main() {
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
    plant_iReport_print(get_report(), speak(d));
    tx_t r = plant_species_create_by_name("Robot");
  plant_map_set(r, "name" , "Bot");
    plant_iReport_print(get_report(), speak(r));
    plant_iReport_print(get_report(), move(r));
    return 0;
}

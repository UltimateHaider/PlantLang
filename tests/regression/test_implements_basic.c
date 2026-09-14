#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t speak(tx_t self);
tx_t main();


tx_t speak(tx_t self) {
    return _cat(_map_get(self, "name"), " barks!");
}
tx_t main() {
  tx_t chk = "";
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
    plant_iReport_print(get_report(), speak(d));
    chk = plant_is_a(d, "Speakable");
    plant_iReport_print(get_report(), _cat("is Speakable: ", chk));
    return 0;
}

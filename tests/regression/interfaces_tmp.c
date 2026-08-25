#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t r1 = "";
  tx_t r2 = "";
    tx_t d = plant_species_create_by_name("Dog");
  plant_map_set(d, "name" , "Rex");
    plant_print(_from_long(plant_array_length(d)));
    r1 = plant_is_a((tx_t)d, "Speakable");
    plant_print(r1);
    PlantArray* c = plant_map_set(plant_map_create(), "name", "Whiskers");
    r2 = plant_is_a(c, "Speakable");
    plant_print(r2);
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t p = plant_species_create_by_name("Person");
    plant_iReport_print(get_report(), _cat3("[", _map_get(p, "name"), "]"));
    plant_iReport_print(get_report(), _cat3("[", _map_get(p, "age"), "]"));
  plant_map_set(p, "name" , "Ada");
  plant_map_set(p, "age" , "36");
    plant_iReport_print(get_report(), _map_get(p, "name"));
    plant_iReport_print(get_report(), _map_get(p, "age"));
    tx_t e = plant_species_create_by_name("Empty");
    plant_iReport_print(get_report(), _from_long(plant_array_length(e)));
    PlantArray* other = plant_map_set(plant_map_create(), "age", _from_long(50));
  plant_map_set(p, "age" , "40");
    plant_iReport_print(get_report(), _map_get(p, "age"));
    return 0;
}

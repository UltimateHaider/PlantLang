#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t get_item(tx_t self);
tx_t set_item(tx_t self, tx_t v);
tx_t main();


tx_t get_item(tx_t self) {
    return _map_get(self, "value");
}
tx_t set_item(tx_t self, tx_t v) {
  plant_map_set(self, "value" , v);
  return set_item;
}
tx_t main() {
    tx_t b = plant_species_create_by_name("Box");
  plant_map_set(b, "value" , "hello");
    plant_iReport_print(get_report(), get_item(b));
    return 0;
}

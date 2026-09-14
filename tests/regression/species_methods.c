#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t greet(tx_t self);
tx_t haveBirthday(tx_t self);
tx_t isAdult(tx_t self);
tx_t describe(tx_t self);
tx_t main();


tx_t greet(tx_t self) {
    return _cat3("Hello, ", _map_get(self, "name"), "!");
}
tx_t haveBirthday(tx_t self) {
  plant_map_set(self, "age" , "37");
  return haveBirthday;
}
tx_t isAdult(tx_t self) {
    if (strcmp(_map_get(self, "age"),"18") >= 0) {
    return "yes";
    }
    return "no";
}
tx_t describe(tx_t self) {
    return _cat3(_map_get(self, "name"), "/", _map_get(self, "age"));
}
tx_t main() {
    tx_t p = plant_species_create_by_name("Person");
  plant_map_set(p, "name" , "Ada");
  plant_map_set(p, "age" , "30");
    plant_iReport_print(get_report(), greet(p));
    plant_iReport_print(get_report(), isAdult(p));
    plant_iReport_print(get_report(), describe(p));
  haveBirthday(p);
    plant_iReport_print(get_report(), describe(p));
    plant_iReport_print(get_report(), isAdult(p));
    return 0;
}

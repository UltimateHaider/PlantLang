#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t nm = "";
  tx_t yr = "";
  tx_t a1 = "";
  tx_t ax = "";
  tx_t b1 = "";
  tx_t bx = "";
    PlantArray* m1 = plant_map_set(plant_map_set(plant_map_set(plant_map_create(), "name", "plant"), "kind", "tree"), "year", _from_long(2026));
    nm = _map_get(m1, "name");
    yr = _map_get(m1, "year");
    plant_iReport_print(get_report(), _cat("name=", nm));
    plant_iReport_print(get_report(), _cat("year=", yr));
    plant_iReport_print(get_report(), _cat("m1=", plant_map_to_string ( m1 )));
    plant_iReport_print(get_report(), _cat("j1=", json_stringify ( m1 )));
    long n = 10;
    PlantArray* m2 = plant_map_set(plant_map_set(plant_map_set(plant_map_set(plant_map_create(), "a", plant_list_make(2, _from_long(1), _from_long(2))), "b", plant_map_set(plant_map_create(), "x", _from_long(n+1))), "lit", _from_long(1+1)), "c", "z");
    a1 = _map_get(m2, "a");
    ax = plant_list_get(a1, 0);
    b1 = _map_get(m2, "b");
    bx = _map_get(b1, "x");
    plant_iReport_print(get_report(), _cat("ax=", ax));
    plant_iReport_print(get_report(), _cat("bx=", bx));
    plant_iReport_print(get_report(), _cat("m2=", plant_map_to_string ( m2 )));
    PlantArray* e = plant_map_create();
    plant_iReport_print(get_report(), _cat("e=", plant_map_to_string ( e )));
    return 0;
}

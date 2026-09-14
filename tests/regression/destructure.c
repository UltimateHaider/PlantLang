#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* m = plant_map_set(plant_map_set(plant_map_create(), "x", _from_long(10)), "y", _from_long(20));
    tx_t x = _map_get((m), "x");
    tx_t y = _map_get((m), "y");
    plant_iReport_print(get_report(), x);
    plant_iReport_print(get_report(), y);
    plant_iReport_print(get_report(), _cat(x, y));
    tx_t rx = _map_get((m), "x");
    tx_t ry = _map_get((m), "y");
    plant_iReport_print(get_report(), _cat3(rx, "/", ry));
    PlantArray* l = plant_list_make(3, _from_long(7), _from_long(8), _from_long(9));
    tx_t a = plant_list_get((l), 0);
    tx_t b = plant_list_get((l), 1);
    tx_t c = plant_list_get((l), 2);
    plant_iReport_print(get_report(), plant_join( plant_list_make(3, a, b, c) , "," ));
    tx_t second = plant_list_get((l), 1);
    plant_iReport_print(get_report(), second);
    PlantArray* nested = plant_map_set(plant_map_set(plant_map_create(), "pt", plant_map_set(plant_map_set(plant_map_create(), "px", _from_long(1)), "py", _from_long(2))), "name", "P");
    tx_t _ds_5_6_0 = _map_get((nested), "pt");
    tx_t px = _map_get(_ds_5_6_0, "px");
    tx_t py = _map_get(_ds_5_6_0, "py");
    tx_t nm = _map_get((nested), "name");
    plant_iReport_print(get_report(), _cat(_cat4(px, "|", py, "|"), nm));
    PlantArray* deep = plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4)));
    tx_t _ds_7_8_0 = plant_list_get((deep), 0);
    tx_t d1 = plant_list_get(_ds_7_8_0, 0);
    tx_t d2 = plant_list_get(_ds_7_8_0, 1);
    tx_t _ds_7_9_1 = plant_list_get((deep), 1);
    tx_t d3 = plant_list_get(_ds_7_9_1, 0);
    tx_t d4 = plant_list_get(_ds_7_9_1, 1);
    plant_iReport_print(get_report(), plant_join( plant_list_make(4, d1, d2, d3, d4) , "-" ));
    tx_t _ds_10_11_0 = _map_get((nested), "pt");
    tx_t onlyY = _map_get(_ds_10_11_0, "py");
    plant_iReport_print(get_report(), onlyY);
    return 0;
}

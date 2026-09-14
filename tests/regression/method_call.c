#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* m = plant_list_make ( 4 , "name" , "root" , "count" , "7" );
    plant_iReport_print(get_report(), plant_map_get(m, "name"));
    plant_iReport_print(get_report(), _from_long(plant_map_has(m, "name")));
    plant_iReport_print(get_report(), _from_long(plant_map_has(m, "missing")));
  plant_map_set(m, "extra" , "9");
    plant_iReport_print(get_report(), plant_map_get(m, "extra"));
    plant_iReport_print(get_report(), _cat("x=", plant_map_get(m, "name")));
    plant_iReport_print(get_report(), _from_long(_to_long(plant_map_get(m, "count"))+1));
    PlantArray* l = plant_list_make ( 3 , "a" , "b" , "c" );
  plant_list_push(l, "d");
    plant_iReport_print(get_report(), plant_list_get(l ,  3 ));
    plant_iReport_print(get_report(), plant_list_pop(l));
    plant_iReport_print(get_report(), plant_list_pop(plant_list_push(l, "x")));
    PlantArray* nested = plant_list_make ( 2 , "pt" , plant_list_make ( 4 , "x" , "7" , "y" , "9" ) );
    plant_iReport_print(get_report(), plant_map_get(plant_map_get(nested, "pt"), "y"));
    plant_iReport_print(get_report(), _map_get(plant_map_get(nested, "pt"), "y"));
    plant_iReport_print(get_report(), _from_long(_to_long(plant_map_get(plant_map_get(nested, "pt"), "y"))+1));
    plant_iReport_print(get_report(), "end");
    return 0;
}

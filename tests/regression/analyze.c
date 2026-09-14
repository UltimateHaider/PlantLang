#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* l = plant_list_make ( 3 , "red" , "green" , "blue" );
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(l)));
    PlantArray* m = plant_list_make ( 6 , "name" , "plant" , "kind" , "tree" , "year" , "2026" );
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(m)));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(NULL)));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(_from_long(7))));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze("abc")));
    tx_t s = "hello world";
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(s)));
    plant_iReport_print(get_report(), _cat("s", plant_map_to_string(plant_analyze(plant_now("YEAR")))));
    return 0;
}

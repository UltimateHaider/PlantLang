#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t sx = "";
    PlantArray* inner = plant_list_make ( 4 , "name" , "root" , "count" , "7" );
    plant_iReport_print(get_report(), _map_get(inner, "name"));
    plant_iReport_print(get_report(), _cat(_map_get(inner, "name"), "!"));
    plant_iReport_print(get_report(), _from_long(_to_long(_map_get(inner, "count"))+1));
    plant_iReport_print(get_report(), _cat("x=", _map_get(inner, "name")));
    sx = _map_get(inner, "name");
    plant_iReport_print(get_report(), sx);
    long n = _to_long(_map_get(inner, "count"));
    plant_iReport_print(get_report(), _from_long(n+1));
    PlantArray* outer = plant_list_make ( 4 , "pt" , plant_list_make ( 4 , "name" , "root" , "nest" , plant_list_make ( 4 , "val" , "9" , "name" , "deep" ) ) , "list" , plant_list_make ( 3 , "a" , "b" , "c" ) );
    plant_iReport_print(get_report(), _map_get(_map_get(outer, "pt"), "name"));
    plant_iReport_print(get_report(), _map_get(_map_get(_map_get(outer, "pt"), "nest"), "val"));
    plant_iReport_print(get_report(), _from_long(_to_long(_map_get(_map_get(_map_get(outer, "pt"), "nest"), "val"))+1));
    plant_iReport_print(get_report(), plant_list_get(_map_get(outer, "list") ,  0 ));
    plant_iReport_print(get_report(), plant_list_get(_map_get(outer, "list") ,  2 ));
    plant_iReport_print(get_report(), "end");
    return 0;
}

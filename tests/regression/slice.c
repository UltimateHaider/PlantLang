#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("s05=", plant_slice( "hello world" , 0 , 5 )));
    plant_iReport_print(get_report(), _cat("s611=", plant_slice( "hello world" , 6 , 11 )));
    plant_iReport_print(get_report(), _cat("s0m1=", plant_slice( "hello world" , 0 , - 1 )));
    plant_iReport_print(get_report(), _cat("sm5m1=", plant_slice( "hello world" , - 5 , - 1 )));
    plant_iReport_print(get_report(), _cat("sm3m1=", plant_slice( "hello" , - 3 , - 1 )));
    plant_iReport_print(get_report(), plant_map_to_string(plant_slice( plant_list_make(5, _from_long(1), _from_long(2), _from_long(3), _from_long(4), _from_long(5)) , 0 , 3 )));
    plant_iReport_print(get_report(), plant_map_to_string(plant_slice( plant_list_make(5, _from_long(1), _from_long(2), _from_long(3), _from_long(4), _from_long(5)) , 0 , - 1 )));
    plant_iReport_print(get_report(), plant_map_to_string(plant_slice( plant_list_make(5, _from_long(1), _from_long(2), _from_long(3), _from_long(4), _from_long(5)) , - 2 , - 1 )));
    plant_iReport_print(get_report(), plant_map_to_string(plant_slice( plant_list_make(4, "a", "b", "c", "d") , 1 , 3 )));
    plant_iReport_print(get_report(), _cat("empty=", plant_slice( "hello" , 2 , 2 )));
    plant_iReport_print(get_report(), _cat("clamp=", plant_slice( "hello" , 0 , 99 )));
    return 0;
}

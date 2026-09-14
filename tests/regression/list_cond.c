#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("has_yes=", plant_has( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 2 )));
    plant_iReport_print(get_report(), _cat("has_no=", plant_has( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 5 )));
    plant_iReport_print(get_report(), _cat("has_empty=", plant_has( plant_list_make(0) , 1 )));
    plant_iReport_print(get_report(), _cat("has_str=", plant_has( plant_list_make(2, "a", "b") , "b" )));
    plant_iReport_print(get_report(), _cat("any_gt=", plant_any( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"> 2")));
    plant_iReport_print(get_report(), _cat("any_gt5=", plant_any( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"> 5")));
    plant_iReport_print(get_report(), _cat("any_empty=", plant_any( plant_list_make(0) ,"> 0")));
    plant_iReport_print(get_report(), _cat("all_gt0=", plant_all( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"> 0")));
    plant_iReport_print(get_report(), _cat("all_gt2=", plant_all( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"> 2")));
    plant_iReport_print(get_report(), _cat("all_empty=", plant_all( plant_list_make(0) ,"> 0")));
    plant_iReport_print(get_report(), _cat("any_eq=", plant_any( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"== 2")));
    plant_iReport_print(get_report(), _cat("all_ge=", plant_all( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,">= 1")));
    plant_iReport_print(get_report(), _cat("all_le=", plant_all( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,"<= 3")));
    return 0;
}

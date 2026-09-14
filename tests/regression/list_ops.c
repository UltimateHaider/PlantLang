#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("first1=", plant_first( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) )));
    plant_iReport_print(get_report(), _cat("last1=", plant_last( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) )));
    plant_iReport_print(get_report(), _cat("sum1=", plant_sum( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) )));
    plant_iReport_print(get_report(), _cat("first_empty=", plant_first( plant_list_make(0) )));
    plant_iReport_print(get_report(), _cat("last_empty=", plant_last( plant_list_make(0) )));
    plant_iReport_print(get_report(), _cat("sum_empty=", plant_sum( plant_list_make(0) )));
    plant_iReport_print(get_report(), _cat("sum_mixed=", plant_sum( plant_list_make(3, _from_long(1), "2", _from_long(3)) )));
    plant_iReport_print(get_report(), _cat("sum_strs=", plant_sum( plant_list_make(2, "a", "b") )));
    plant_iReport_print(get_report(), _cat("sum_bool=", plant_sum( plant_list_make(2, "TRUE", _from_long(2)) )));
    plant_iReport_print(get_report(), _cat("sum_nested=", plant_sum( plant_list_make(3, _from_long(1), plant_list_make(2, _from_long(2), _from_long(3)), _from_long(4)) )));
    plant_iReport_print(get_report(), _cat("first_str=", plant_first( plant_list_make(2, "x", "y") )));
    plant_iReport_print(get_report(), _cat("last_str=", plant_last( plant_list_make(2, "x", "y") )));
    plant_iReport_print(get_report(), _cat("sum_null=", plant_sum( NULL )));
    return 0;
}

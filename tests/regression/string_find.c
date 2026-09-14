#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("find_world=", plant_find( "hello world" , "world" )));
    plant_iReport_print(get_report(), _cat("find_miss=", plant_find( "hello world" , "x" )));
    plant_iReport_print(get_report(), _cat("find_empty_sub=", plant_find( "hello" , "" )));
    plant_iReport_print(get_report(), _cat("find_empty_text=", plant_find( "" , "x" )));
    plant_iReport_print(get_report(), _cat("count_two=", plant_count_of( "hello hello" , "hello" )));
    plant_iReport_print(get_report(), _cat("count_miss=", plant_count_of( "hello" , "x" )));
    plant_iReport_print(get_report(), _cat("count_empty=", plant_count_of( "" , "x" )));
    plant_iReport_print(get_report(), _cat("count_no_overlap=", plant_count_of( "aaaa" , "aa" )));
    plant_iReport_print(get_report(), _cat("find_aaa=", plant_find( "aaa" , "aa" )));
    plant_iReport_print(get_report(), _cat("count_abc=", plant_count_of( "abcabcabc" , "abc" )));
    return 0;
}

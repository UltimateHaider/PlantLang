#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("lit1=", plant_pick( 1 , "yes" , "no" )));
    plant_iReport_print(get_report(), _cat("lit0=", plant_pick( 0 , "yes" , "no" )));
    plant_iReport_print(get_report(), _cat("true=", plant_pick( 1 , 1 , 0 )));
    plant_iReport_print(get_report(), _cat("false=", plant_pick( 0 , 1 , 0 )));
    plant_iReport_print(get_report(), _cat("adult=", plant_pick( 20 >= 18 , "adult" , "child" )));
    plant_iReport_print(get_report(), _cat("child=", plant_pick( 10 >= 18 , "adult" , "child" )));
    plant_iReport_print(get_report(), _cat("empty=", plant_pick( "" , "yes" , "no" )));
    plant_iReport_print(get_report(), _cat("str=", plant_pick( "TRUE" , "a" , "b" )));
    plant_iReport_print(get_report(), _cat("zero=", plant_pick( "0" , "a" , "b" )));
    plant_iReport_print(get_report(), _cat("neg=", plant_pick( - 1 , "a" , "b" )));
    plant_iReport_print(get_report(), _cat("txt=", plant_pick( "anything" , "a" , "b" )));
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("u1=", plant_upper( "hello" )));
    plant_iReport_print(get_report(), _cat("l1=", plant_lower( "HELLO" )));
    plant_iReport_print(get_report(), _cat("u2=", plant_upper( "Hello World" )));
    plant_iReport_print(get_report(), _cat("l2=", plant_lower( "Hello World" )));
    plant_iReport_print(get_report(), _cat("u3=", plant_upper( "MIXED 123 !@#" )));
    plant_iReport_print(get_report(), _cat("l3=", plant_lower( "MIXED 123 !@#" )));
    plant_iReport_print(get_report(), _cat("u4=", plant_upper( "" )));
    plant_iReport_print(get_report(), _cat("l4=", plant_lower( "" )));
    plant_iReport_print(get_report(), _cat("u5=", plant_upper( NULL )));
    plant_iReport_print(get_report(), _cat("l5=", plant_lower( NULL )));
    plant_iReport_print(get_report(), _cat("u6=", plant_upper( "hello world" )));
    plant_iReport_print(get_report(), _cat("l6=", plant_lower( "HELLO WORLD" )));
    plant_iReport_print(get_report(), _cat("t1=", plant_trim( " hello " )));
    plant_iReport_print(get_report(), _cat("t2=", plant_trim( "\thello\n" )));
    plant_iReport_print(get_report(), _cat("t3=", plant_trim( "" )));
    plant_iReport_print(get_report(), _cat("t4=", plant_trim( NULL )));
    plant_iReport_print(get_report(), _cat("t5=", plant_trim( "   all whitespace " )));
    plant_iReport_print(get_report(), _cat("t6=", plant_trim( "no edges" )));
    plant_iReport_print(get_report(), _cat("r1=", plant_list_reverse( "hello" )));
    plant_iReport_print(get_report(), _cat("r2=", plant_list_reverse( "world" )));
    plant_iReport_print(get_report(), _cat("r3=", plant_list_reverse( "" )));
    plant_iReport_print(get_report(), _cat("r4=", plant_list_reverse( NULL )));
    plant_iReport_print(get_report(), _cat("r5=", plant_list_reverse( "racecar" )));
    return 0;
}

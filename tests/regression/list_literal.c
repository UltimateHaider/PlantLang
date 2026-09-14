#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* a = plant_list_make(3, _from_long(1), _from_long(2), _from_long(3));
    plant_iReport_print(get_report(), _cat("simple=", plant_join( a , "," )));
    PlantArray* b = plant_list_make(3, "x", plant_list_make(2, "y", "z"), "w");
    plant_iReport_print(get_report(), _cat("nested=", plant_join( b , "|" )));
    PlantArray* c = plant_list_make(0);
    plant_iReport_print(get_report(), _cat("empty=", plant_join( c , "," )));
    long n = 10;
    tx_t y = "var";
    PlantArray* d = plant_list_make(2, _from_long(n+1), y);
    plant_iReport_print(get_report(), _cat("mixed=", plant_join( d , "-" )));
    PlantArray* e = plant_list_make(2, _from_long(1+1), _from_long(5));
    plant_iReport_print(get_report(), _cat("arith=", plant_join( e , "," )));
    plant_iReport_print(get_report(), _cat("lit=", plant_join( plant_list_make(2, _from_long(n+1), _from_long(2)) , "," )));
    plant_iReport_print(get_report(), _cat("i0=", plant_list_get(a ,  0 )));
    plant_iReport_print(get_report(), _cat("i2=", plant_list_get(a ,  2 )));
    plant_iReport_print(get_report(), _cat("inner=", plant_join( plant_list_get(b ,  1 ) , "-" )));
    return 0;
}

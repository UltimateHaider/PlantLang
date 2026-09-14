#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* l1 = plant_list_make(3, "a", "b", "c");
    plant_iReport_print(get_report(), _cat("abc=", plant_join( l1 , ", " )));
    PlantArray* l2 = plant_list_make(3, "1", "2", "3");
    plant_iReport_print(get_report(), _cat("nums=", plant_join( l2 , "-" )));
    PlantArray* l3 = plant_list_make(0);
    plant_iReport_print(get_report(), _cat("empty=", plant_join( l3 , "," )));
    PlantArray* l4 = plant_list_make(1, "a");
    plant_iReport_print(get_report(), _cat("single=", plant_join( l4 , "," )));
    plant_iReport_print(get_report(), _cat("mixed=", plant_join( plant_list_make(3, "hello", "42", "TRUE") , " | " )));
    PlantArray* m1 = plant_list_make(2, "x", "1");
    plant_iReport_print(get_report(), _cat("complex=", plant_join( plant_list_make(2, m1, plant_list_make(3, _from_long(2), _from_long(3), _from_long(4))) , "," )));
    plant_iReport_print(get_report(), _cat("null_delim=", plant_join( plant_list_make(3, "a", "b", "c") , NULL )));
    plant_iReport_print(get_report(), _cat("null_list=", plant_join( NULL , "," )));
    plant_iReport_print(get_report(), _cat("nested=", plant_join( plant_list_make(3, _from_long(1), plant_list_make(2, _from_long(2), _from_long(3)), _from_long(4)) , "-" )));
    plant_iReport_print(get_report(), _cat("ab=", plant_join( plant_list_make(2, "a", "b") , "," )));
    plant_iReport_print(get_report(), _cat("pristine=", "hello | TRUE"));
    return 0;
}

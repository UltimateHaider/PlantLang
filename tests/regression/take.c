#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* fruits = plant_list_make ( 0 );
    fruits = plant_list_add(fruits, "banana");
    fruits = plant_list_add(fruits, "apple");
    fruits = plant_list_add(fruits, "mango");
    fruits = plant_list_remove(fruits, "apple");
    tx_t out1 = "";
    for (long i = 0; i < plant_array_length(fruits); i++) {
      tx_t f = plant_list_get(fruits, i);
    out1 = _cat3(out1, f, " ");
    }
    plant_iReport_print(get_report(), _cat("after take = ", out1));
    plant_iReport_print(get_report(), _cat("count after take = ", _from_long ( plant_array_length(fruits) )));
    fruits = plant_list_remove(fruits, "apple");
    tx_t out2 = "";
    for (long i = 0; i < plant_array_length(fruits); i++) {
      tx_t f = plant_list_get(fruits, i);
    out2 = _cat3(out2, f, " ");
    }
    plant_iReport_print(get_report(), _cat("take missing value = ", out2));
    PlantArray* empty = plant_list_make ( 0 );
    empty = plant_list_remove(empty, "x");
    plant_iReport_print(get_report(), _cat("take from empty = ok ", _from_long ( plant_array_length(empty) )));
    PlantArray* nils = NULL;
    nils = plant_list_remove(nils, "a");
    plant_iReport_print(get_report(), "take from null = ok");
    PlantArray* dup = plant_list_make ( 0 );
    dup = plant_list_add(dup, "1");
    dup = plant_list_add(dup, "2");
    dup = plant_list_add(dup, "1");
    dup = plant_list_add(dup, "3");
    dup = plant_list_remove(dup, "1");
    tx_t out3 = "";
    for (long i = 0; i < plant_array_length(dup); i++) {
      tx_t n = plant_list_get(dup, i);
    out3 = _cat3(out3, n, " ");
    }
    plant_iReport_print(get_report(), _cat("take first duplicate only = ", out3));
    return 0;
}

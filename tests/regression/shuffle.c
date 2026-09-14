#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* nums = plant_list_make ( 0 );
    nums = plant_list_add(nums, "1");
    nums = plant_list_add(nums, "2");
    nums = plant_list_add(nums, "3");
    nums = plant_list_add(nums, "4");
    nums = plant_list_add(nums, "5");
    nums = plant_list_add(nums, "6");
    nums = plant_list_add(nums, "7");
    nums = plant_list_add(nums, "8");
    long before = 0;
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    before = before+_to_long ( n );
    }
    nums = plant_shuffle(nums);
    long after = 0;
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    after = after+_to_long ( n );
    }
    nums = plant_sort(nums, "");
    tx_t sorted = "";
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    sorted = _cat3(sorted, n, " ");
    }
    plant_iReport_print(get_report(), _cat("sum before shake = ", _from_long ( before )));
    plant_iReport_print(get_report(), _cat("sum after shake = ", _from_long ( after )));
    plant_iReport_print(get_report(), _cat("sorted after shake = ", sorted));
    nums = plant_shuffle(nums);
    tx_t o1 = "";
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    o1 = _cat3(o1, n, ",");
    }
    nums = plant_shuffle(nums);
    tx_t o2 = "";
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    o2 = _cat3(o2, n, ",");
    }
    if (o1 == o2) {
    plant_iReport_print(get_report(), "orders differ = 0");
    }
    if (o1 != o2) {
    plant_iReport_print(get_report(), "orders differ = 1");
    }
    PlantArray* one = plant_list_make ( 0 );
    one = plant_list_add(one, "solo");
    one = plant_shuffle(one);
    tx_t out1 = "";
    for (long i = 0; i < plant_array_length(one); i++) {
      tx_t n = plant_list_get(one, i);
    out1 = _cat(out1, n);
    }
    plant_iReport_print(get_report(), _cat("single element after shake = ", out1));
    PlantArray* empty = plant_list_make ( 0 );
    empty = plant_shuffle(empty);
    plant_iReport_print(get_report(), "empty list shaken = ok");
    return 0;
}

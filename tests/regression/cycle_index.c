#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* items = plant_list_make ( 0 );
    items = plant_list_add(items, "a");
    items = plant_list_add(items, "b");
    items = plant_list_add(items, "c");
    items = plant_list_add(items, "d");
    tx_t s = "";
    for (long idx = 0; idx < plant_array_length(items); idx++) {
      tx_t item = plant_list_get(items, idx);
    s = _cat(_cat4(s, _from_long ( idx ), ":", item), " ");
    }
    plant_iReport_print(get_report(), _cat("indexed=", s));
    PlantArray* one = plant_list_make ( 0 );
    one = plant_list_add(one, "solo");
    for (long idx = 0; idx < plant_array_length(one); idx++) {
      tx_t item = plant_list_get(one, idx);
    plant_iReport_print(get_report(), _cat4("single=", _from_long ( idx ), ":", item));
    }
    PlantArray* empty = plant_list_make ( 0 );
    tx_t hit = "none";
    for (long idx = 0; idx < plant_array_length(empty); idx++) {
      tx_t item = plant_list_get(empty, idx);
    hit = "ran";
    }
    plant_iReport_print(get_report(), _cat("empty=", hit));
    long t = 0;
    for (long idx = 0; idx < plant_array_length(items); idx++) {
      tx_t item = plant_list_get(items, idx);
    t = t+idx;
    }
    plant_iReport_print(get_report(), _cat("idxsum=", _from_long(t)));
    tx_t evens = "";
    for (long idx = 0; idx < plant_array_length(items); idx++) {
      tx_t item = plant_list_get(items, idx);
    if (idx % 2 == 0) {
    evens = _cat3(evens, item, " ");
    }
    }
    plant_iReport_print(get_report(), _cat("evens=", evens));
    PlantArray* nums = plant_list_make ( 0 );
    nums = plant_list_add(nums, "5");
    nums = plant_list_add(nums, "15");
    nums = plant_list_add(nums, "25");
    long total = 0;
    for (long idx = 0; idx < plant_array_length(nums); idx++) {
      tx_t item = plant_list_get(nums, idx);
    total = total+_to_long ( item )+idx;
    }
    plant_iReport_print(get_report(), _cat("sum-plus-idx=", _from_long(total)));
    return 0;
}

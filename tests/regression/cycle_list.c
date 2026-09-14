#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    PlantArray* items = plant_list_make ( 0 );
    items = plant_list_add(items, "one");
    items = plant_list_add(items, "two");
    items = plant_list_add(items, "three");
    tx_t s = "";
    for (long __cycle_i = 0; __cycle_i < plant_array_length(items); __cycle_i++) {
      tx_t item = plant_list_get(items, __cycle_i);
    s = _cat3(s, item, " ");
    }
    plant_iReport_print(get_report(), _cat("elems=", s));
    PlantArray* empty = plant_list_make ( 0 );
    tx_t hit = "none";
    for (long __cycle_i = 0; __cycle_i < plant_array_length(empty); __cycle_i++) {
      tx_t item = plant_list_get(empty, __cycle_i);
    hit = "ran";
    }
    plant_iReport_print(get_report(), _cat("empty=", hit));
    PlantArray* one = plant_list_make ( 0 );
    one = plant_list_add(one, "solo");
    for (long __cycle_i = 0; __cycle_i < plant_array_length(one); __cycle_i++) {
      tx_t item = plant_list_get(one, __cycle_i);
    plant_iReport_print(get_report(), _cat("single=", item));
    }
    PlantArray* nums = plant_list_make ( 0 );
    nums = plant_list_add(nums, "10");
    nums = plant_list_add(nums, "20");
    nums = plant_list_add(nums, "30");
    long total = 0;
    for (long __cycle_i = 0; __cycle_i < plant_array_length(nums); __cycle_i++) {
      tx_t item = plant_list_get(nums, __cycle_i);
    total = total+_to_long ( item );
    }
    plant_iReport_print(get_report(), _cat("sum=", _from_long(total)));
    PlantArray* grid = plant_list_make ( 0 );
    PlantArray* row1 = plant_list_make ( 0 );
    PlantArray* row2 = plant_list_make ( 0 );
    row1 = plant_list_add(row1, "a");
    row1 = plant_list_add(row1, "b");
    row2 = plant_list_add(row2, "c");
    grid = plant_list_add(grid, row1);
    grid = plant_list_add(grid, row2);
    tx_t flat = "";
    for (long __cycle_i = 0; __cycle_i < plant_array_length(grid); __cycle_i++) {
      tx_t row = plant_list_get(grid, __cycle_i);
    for (long __cycle_i = 0; __cycle_i < plant_array_length(row); __cycle_i++) {
      tx_t cell = plant_list_get(row, __cycle_i);
    flat = _cat3(flat, cell, " ");
    }
    }
    plant_iReport_print(get_report(), _cat("nested=", flat));
    tx_t s2 = "";
    for (long __cycle_i = 0; __cycle_i < plant_array_length(items); __cycle_i++) {
      tx_t item = plant_list_get(items, __cycle_i);
    if (strcmp(item,"two") == 0) {
                      continue;
    }
    if (strcmp(item,"three") == 0) {
                      break;
    }
    s2 = _cat3(s2, item, " ");
    }
    plant_iReport_print(get_report(), _cat("ctl=", s2));
    return 0;
}

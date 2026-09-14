#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t p1 = "";
  tx_t p2 = "";
  tx_t p3 = "";
  tx_t p4 = "";
  tx_t nm = "";
  tx_t ag = "";
    PlantArray* fruits = plant_list_make ( 0 );
    fruits = plant_list_add(fruits, "banana");
    fruits = plant_list_add(fruits, "apple");
    fruits = plant_list_add(fruits, "mango");
    fruits = plant_list_add(fruits, "cherry");
    fruits = plant_sort(fruits, "");
    tx_t out1 = "";
    for (long i = 0; i < plant_array_length(fruits); i++) {
      tx_t f = plant_list_get(fruits, i);
    out1 = _cat3(out1, f, " ");
    }
    plant_iReport_print(get_report(), _cat("sort default asc = ", out1));
    fruits = plant_sort(fruits, "DESC");
    tx_t out2 = "";
    for (long i = 0; i < plant_array_length(fruits); i++) {
      tx_t f = plant_list_get(fruits, i);
    out2 = _cat3(out2, f, " ");
    }
    plant_iReport_print(get_report(), _cat("sort explicit desc = ", out2));
    fruits = plant_sort(fruits, "");
    tx_t out3 = "";
    for (long i = 0; i < plant_array_length(fruits); i++) {
      tx_t f = plant_list_get(fruits, i);
    out3 = _cat3(out3, f, " ");
    }
    plant_iReport_print(get_report(), _cat("sort explicit asc = ", out3));
    PlantArray* nums = plant_list_make ( 0 );
    nums = plant_list_add(nums, "5");
    nums = plant_list_add(nums, "3");
    nums = plant_list_add(nums, "8");
    nums = plant_list_add(nums, "1");
    nums = plant_list_add(nums, "9");
    nums = plant_list_add(nums, "2");
    nums = plant_sort(nums, "");
    tx_t out4 = "";
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    out4 = _cat3(out4, n, " ");
    }
    plant_iReport_print(get_report(), _cat("numeric sort asc = ", out4));
    nums = plant_sort(nums, "DESC");
    tx_t out5 = "";
    for (long i = 0; i < plant_array_length(nums); i++) {
      tx_t n = plant_list_get(nums, i);
    out5 = _cat3(out5, n, " ");
    }
    plant_iReport_print(get_report(), _cat("numeric sort desc = ", out5));
    PlantArray* dup = plant_list_make ( 0 );
    dup = plant_list_add(dup, "3");
    dup = plant_list_add(dup, "3");
    dup = plant_list_add(dup, "1");
    dup = plant_list_add(dup, "3");
    dup = plant_list_add(dup, "2");
    dup = plant_sort(dup, "");
    tx_t out6 = "";
    for (long i = 0; i < plant_array_length(dup); i++) {
      tx_t n = plant_list_get(dup, i);
    out6 = _cat3(out6, n, " ");
    }
    plant_iReport_print(get_report(), _cat("duplicates = ", out6));
    PlantArray* one = plant_list_make ( 0 );
    one = plant_list_add(one, "solo");
    one = plant_sort(one, "DESC");
    tx_t out7 = "";
    for (long i = 0; i < plant_array_length(one); i++) {
      tx_t n = plant_list_get(one, i);
    out7 = _cat3(out7, n, " ");
    }
    plant_iReport_print(get_report(), _cat("single element = ", out7));
    PlantArray* empty = plant_list_make ( 0 );
    empty = plant_sort(empty, "");
    empty = plant_sort(empty, "DESC");
    plant_iReport_print(get_report(), "empty list sorted = ok");
    p1 = plant_list_make(6, "name", "alpha", "dept", "eng", "age", "10");
    p2 = plant_list_make(6, "name", "beta", "dept", "eng", "age", "30");
    p3 = plant_list_make(6, "name", "gamma", "dept", "ops", "age", "20");
    p4 = plant_list_make(6, "name", "delta", "dept", "ops", "age", "5");
    PlantArray* people = plant_list_make ( 0 );
    people = plant_list_add(people, p1);
    people = plant_list_add(people, p2);
    people = plant_list_add(people, p3);
    people = plant_list_add(people, p4);
    people = plant_sort(people, "name:ASC");
    tx_t out8 = "";
    for (long i = 0; i < plant_array_length(people); i++) {
      tx_t p = plant_list_get(people, i);
    nm = _map_get(p, "name");
    out8 = _cat3(out8, nm, " ");
    }
    plant_iReport_print(get_report(), _cat("by name asc = ", out8));
    people = plant_sort(people, "age:DESC");
    tx_t out9 = "";
    for (long i = 0; i < plant_array_length(people); i++) {
      tx_t p = plant_list_get(people, i);
    nm = _map_get(p, "name");
    out9 = _cat3(out9, nm, " ");
    }
    plant_iReport_print(get_report(), _cat("by age desc = ", out9));
    people = plant_sort(people, "dept:ASC,age:DESC");
    tx_t out10 = "";
    for (long i = 0; i < plant_array_length(people); i++) {
      tx_t p = plant_list_get(people, i);
    nm = _map_get(p, "name");
    ag = _map_get(p, "age");
    out10 = _cat(_cat4(out10, nm, ":", ag), " ");
    }
    plant_iReport_print(get_report(), _cat("by dept asc, age desc = ", out10));
    people = plant_sort(people, "dept:ASC,age:ASC");
    tx_t out11 = "";
    for (long i = 0; i < plant_array_length(people); i++) {
      tx_t p = plant_list_get(people, i);
    nm = _map_get(p, "name");
    ag = _map_get(p, "age");
    out11 = _cat(_cat4(out11, nm, ":", ag), " ");
    }
    plant_iReport_print(get_report(), _cat("by dept, age default asc = ", out11));
    return 0;
}

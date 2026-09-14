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
    tx_t out1 = "";
    for (long i = 0; i < plant_array_length(items); i++) {
      tx_t it = plant_list_get(items, i);
    out1 = _cat3(out1, it, " ");
    }
    plant_iReport_print(get_report(), _cat("append to existing = ", out1));
    PlantArray* fresh = NULL;
    fresh = plant_list_add(fresh, "first");
    fresh = plant_list_add(fresh, "second");
    tx_t out2 = "";
    for (long i = 0; i < plant_array_length(fresh); i++) {
      tx_t it = plant_list_get(fresh, i);
    out2 = _cat3(out2, it, " ");
    }
    plant_iReport_print(get_report(), _cat("instantiate null target = ", out2));
    fresh = plant_list_add(fresh, "first");
    tx_t out3 = "";
    for (long i = 0; i < plant_array_length(fresh); i++) {
      tx_t it = plant_list_get(fresh, i);
    out3 = _cat3(out3, it, " ");
    }
    plant_iReport_print(get_report(), _cat("duplicate appended = ", out3));
    fresh = plant_list_remove(fresh, "first");
    tx_t out4 = "";
    for (long i = 0; i < plant_array_length(fresh); i++) {
      tx_t it = plant_list_get(fresh, i);
    out4 = _cat3(out4, it, " ");
    }
    plant_iReport_print(get_report(), _cat("after take one duplicate = ", out4));
    return 0;
}

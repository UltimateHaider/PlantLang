#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long total = 0;
    for (long i = 1; i <= 5; i += 1) {
        total += i;
    if (i == 3) {
                      continue;
    }
        total -= 1;
    }
    plant_iReport_print(get_report(), _cat("total=", _from_long(total)));
    long down = 0;
    for (long i = 3; i >= 1; i += - 1) {
        down += i;
    }
    plant_iReport_print(get_report(), _cat("down=", _from_long(down)));
    long j = 0;
    PlantArray* items = plant_list_make ( 0 );
    items = plant_list_add(items, "a");
    items = plant_list_add(items, "b");
    items = plant_list_add(items, "c");
    for (long idx = 0; idx < plant_array_length(items); idx++) {
      tx_t item = plant_list_get(items, idx);
        j += idx+1;
    }
    plant_iReport_print(get_report(), _cat("j=", _from_long(j)));
    return 0;
}

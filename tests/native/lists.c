#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t r = "";
    PlantArray* parts = plant_list_make ( 0 );
    parts = plant_list_add(parts, "first");
    parts = plant_list_add(parts, "second");
    r = plant_list_get(parts, 1);
    plant_iReport_print(get_report(), _cat("second=", r));
    if (strcmp(_at ( parts , 0 ),"first") == 0) {
    plant_iReport_print(get_report(), "first-ok");
    }
    return 0;
}

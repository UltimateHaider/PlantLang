#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t e0 = "";
    PlantArray* xs = plant_list_make ( 0 );
    xs = plant_list_add(xs, _from_long ( 3 ));
    xs = plant_list_add(xs, _from_long ( 8 ));
    plant_iReport_print(get_report(), _from_long(plant_array_length(xs)));
    e0 = plant_list_get(xs, 0);
    plant_iReport_print(get_report(), _cat("first ", e0));
    long acc = 0;
    acc = acc+10;
    plant_iReport_print(get_report(), _from_long(acc));
    return 0;
}

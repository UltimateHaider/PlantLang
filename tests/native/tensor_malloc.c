#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_tensor_to_string((PlantTensor*)plant_tensor_from_list( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4))) )));
    return 0;
}

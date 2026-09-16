#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), plant_tensor_to_string((PlantTensor*)plant_tensor_from_list( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) )));
    tx_t t = plant_tensor_from_list( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4))) );
    plant_iReport_print(get_report(), t);
    plant_iReport_print(get_report(), t);
    tx_t a = plant_tensor_from_list( plant_list_make(3, _from_long(10), _from_long(20), _from_long(30)) );
    tx_t b = plant_tensor_from_list( plant_list_make(3, _from_long(10), _from_long(20), _from_long(30)) );
    plant_iReport_print(get_report(), a);
    plant_iReport_print(get_report(), b);
    tx_t c1 = plant_tensor_from_list( plant_list_make(1, _from_long(1)) );
    tx_t c2 = plant_tensor_from_list( plant_list_make(1, _from_long(2)) );
    tx_t c3 = plant_tensor_from_list( plant_list_make(1, _from_long(3)) );
    plant_iReport_print(get_report(), c1);
    plant_iReport_print(get_report(), c2);
    plant_iReport_print(get_report(), c3);
    return 0;
}

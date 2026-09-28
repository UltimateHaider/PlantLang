#include <plant_compat.h>
#include <plant_memory.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t L1 = plant_list_make(5, _from_long(1), _from_long(2), _from_long(3), _from_long(4), _from_long(5));
    plant_iReport_print(get_report(), L1);
    plant_list_free(L1);
    plant_iReport_print(get_report(), "old_style_list_free: PASS");
    tx_t T1 = plant_tensor_from_list( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4))) );
    plant_iReport_print(get_report(), T1);
    plant_tensor_free(T1);
    plant_iReport_print(get_report(), "old_style_tensor_free: PASS");
    tx_t L2 = plant_list_make(2, _from_long(10), _from_long(20));
    tx_t T2 = plant_tensor_from_list( plant_list_make(2, _from_long(30), _from_long(40)) );
    plant_iReport_print(get_report(), L2);
    plant_iReport_print(get_report(), T2);
    plant_list_free(L2);
    T2 = plant_mem_free((tx_t)T2);
    plant_iReport_print(get_report(), "mixed_old_and_new: PASS");
    tx_t L3 = plant_list_make(1, _from_long(100));
    plant_list_free(L3);
    L3 = plant_mem_free((tx_t)L3);
    plant_iReport_print(get_report(), "multiple_free_calls: PASS");
    tx_t T3 = plant_tensor_from_list( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) );
    plant_iReport_print(get_report(), T3);
    T3 = plant_mem_free((tx_t)T3);
    plant_iReport_print(get_report(), "tensor_show_then_free: PASS");
  return main;
}

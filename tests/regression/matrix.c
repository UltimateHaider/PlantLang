#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "== vectors ==");
    plant_iReport_print(get_report(), plant_dot( plant_list_make(2, _from_long(2), _from_long(3)) ,  plant_list_make(2, _from_long(4), _from_long(1)) ));
    plant_iReport_print(get_report(), plant_norm( plant_list_make(2, _from_long(3), _from_long(4)) ));
    plant_iReport_print(get_report(), plant_join( plant_cross( plant_list_make(3, _from_long(1), _from_long(0), _from_long(0)) ,  plant_list_make(3, _from_long(0), _from_long(1), _from_long(0)) ) , "," ));
    plant_iReport_print(get_report(), plant_list_get(plant_cross( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ,  plant_list_make(3, _from_long(4), _from_long(5), _from_long(6)) ) ,  0 ));
    plant_iReport_print(get_report(), "== transpose ==");
    PlantArray* M = plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4)));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_transpose( M ) ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_transpose( plant_list_make(1, plant_list_make(3, _from_long(1), _from_long(2), _from_long(3))) ) ) , "," ));
    plant_iReport_print(get_report(), "== det ==");
    plant_iReport_print(get_report(), plant_det( M ));
    plant_iReport_print(get_report(), plant_det( plant_list_make(3, plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)), plant_list_make(3, _from_long(4), _from_long(5), _from_long(6)), plant_list_make(3, _from_long(7), _from_long(8), _from_long(10))) ));
    plant_iReport_print(get_report(), plant_det( plant_list_make(2, plant_list_make(2, _from_long(2), _from_long(1)), plant_list_make(2, _from_long(1), _from_long(3))) ));
    plant_iReport_print(get_report(), "== inverse ==");
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_inverse( M ) ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_inverse( plant_list_make(2, plant_list_make(2, _from_long(2), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(2))) ) ) , "," ));
    plant_iReport_print(get_report(), "== matrix_mult ==");
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_matrix_mult( M ,  M ) ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_matrix_mult( plant_list_make(1, plant_list_make(3, _from_long(1), _from_long(2), _from_long(3))) ,  plant_list_make(3, plant_list_make(1, _from_long(4)), plant_list_make(1, _from_long(5)), plant_list_make(1, _from_long(6))) ) ) , "," ));
    plant_iReport_print(get_report(), "== errors ==");
    plant_iReport_print(get_report(), plant_dot( plant_list_make(2, _from_long(1), _from_long(2)) ,  plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ));
    plant_iReport_print(get_report(), plant_cross( plant_list_make(2, _from_long(1), _from_long(2)) ,  plant_list_make(2, _from_long(3), _from_long(4)) ));
    plant_iReport_print(get_report(), plant_matrix_mult( plant_list_make(1, plant_list_make(2, _from_long(1), _from_long(2))) ,  plant_list_make(1, plant_list_make(2, _from_long(1), _from_long(2))) ));
    plant_iReport_print(get_report(), plant_det( plant_list_make(2, plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)), plant_list_make(3, _from_long(4), _from_long(5), _from_long(6))) ));
    plant_iReport_print(get_report(), plant_inverse( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(2), _from_long(4))) ));
    plant_iReport_print(get_report(), plant_inverse( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(1, _from_long(3))) ));
    plant_iReport_print(get_report(), plant_norm( "nope" ));
    plant_iReport_print(get_report(), plant_dot( "a" ,  "b" ));
    plant_iReport_print(get_report(), "== scalar roundtrip ==");
    plant_iReport_print(get_report(), plant_round( plant_det( M ) ));
    return 0;
}

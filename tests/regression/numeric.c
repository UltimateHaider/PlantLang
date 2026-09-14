#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "== lu ==");
    PlantArray* A = plant_list_make(2, plant_list_make(2, _from_long(4), _from_long(3)), plant_list_make(2, _from_long(6), _from_long(3)));
    PlantArray* f = plant_lu( A );
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_list_get(f ,  0 ) ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_list_get(f ,  1 ) ) , "," ));
    plant_iReport_print(get_report(), "== eigen ==");
    PlantArray* S = plant_list_make(2, plant_list_make(2, _from_long(2), _from_long(1)), plant_list_make(2, _from_long(1), _from_long(2)));
    PlantArray* eg = plant_eigen( S );
    plant_iReport_print(get_report(), plant_join( plant_list_get(eg ,  0 ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_list_flatten( plant_list_get(eg ,  1 ) ) , "," ));
    PlantArray* e2 = plant_eigen( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(5)), plant_list_make(2, _from_long(5), _from_long(1))) );
    plant_iReport_print(get_report(), plant_join( plant_list_get(e2 ,  0 ) , "," ));
    plant_iReport_print(get_report(), plant_eigen( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(3), _from_long(4))) ));
    plant_iReport_print(get_report(), "== svd ==");
    PlantArray* s1 = plant_svd( plant_list_make(2, plant_list_make(2, _from_long(3), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(2))) );
    plant_iReport_print(get_report(), plant_join( plant_list_get(s1 ,  1 ) , "," ));
    PlantArray* s2 = plant_svd( plant_list_make(3, plant_list_make(2, _from_long(4), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(1)), plant_list_make(2, _from_long(0), _from_long(0))) );
    plant_iReport_print(get_report(), plant_join( plant_list_get(s2 ,  1 ) , "," ));
    plant_iReport_print(get_report(), "== solve ==");
    plant_iReport_print(get_report(), plant_join( plant_solve( plant_list_make(2, plant_list_make(2, _from_long(2), _from_long(1)), plant_list_make(2, _from_long(1), _from_long(3))) ,  plant_list_make(2, _from_long(5), _from_long(10)) ) , "," ));
    plant_iReport_print(get_report(), plant_join( plant_solve( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(1))) ,  plant_list_make(2, _from_long(7), _from_long(9)) ) , "," ));
    plant_iReport_print(get_report(), plant_solve( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(2), _from_long(4))) ,  plant_list_make(2, _from_long(3), _from_long(6)) ));
    plant_iReport_print(get_report(), "== cond ==");
    plant_iReport_print(get_report(), plant_cond( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(1))) ));
    plant_iReport_print(get_report(), plant_cond( plant_list_make(2, plant_list_make(2, _from_long(2), _from_long(0)), plant_list_make(2, _from_long(0), _from_long(2))) ));
    plant_iReport_print(get_report(), plant_cond( plant_list_make(2, plant_list_make(2, _from_long(1), _from_long(2)), plant_list_make(2, _from_long(2), _from_long(4))) ));
    plant_iReport_print(get_report(), "== errors ==");
    plant_iReport_print(get_report(), plant_lu( plant_list_make(1, plant_list_make(3, _from_long(1), _from_long(2), _from_long(3))) ));
    plant_iReport_print(get_report(), plant_eigen( "nope" ));
    plant_iReport_print(get_report(), plant_solve( plant_list_make(1, plant_list_make(2, _from_long(2), _from_long(1))) ,  plant_list_make(1, _from_long(1)) ));
    return 0;
}

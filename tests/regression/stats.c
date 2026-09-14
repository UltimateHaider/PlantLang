#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t v = "";
  tx_t p = "";
  tx_t lo = "";
  tx_t hi = "";
  tx_t sp = "";
  tx_t md = "";
    PlantArray* nums = plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4));
    plant_iReport_print(get_report(), "== variance/stddev/product ==");
    plant_iReport_print(get_report(), plant_list_variance( nums ));
    plant_iReport_print(get_report(), plant_list_stddev( nums ));
    plant_iReport_print(get_report(), plant_list_product( nums ));
    plant_iReport_print(get_report(), "== extrema/spread/mode ==");
    plant_iReport_print(get_report(), plant_list_min( nums ));
    plant_iReport_print(get_report(), plant_list_max( nums ));
    plant_iReport_print(get_report(), plant_list_range( nums ));
    plant_iReport_print(get_report(), plant_list_mode( plant_list_make(5, _from_long(7), _from_long(2), _from_long(7), _from_long(9), _from_long(7)) ));
    plant_iReport_print(get_report(), "== scalar forms intact ==");
    plant_iReport_print(get_report(), math_min(_from_double( 3 ), _from_double( 5 )));
    plant_iReport_print(get_report(), math_max(_from_double( - 5 ), _from_double( 3 )));
    plant_iReport_print(get_report(), plant_join( plant_range_list(_from_double( 2 ), _from_double( 6 )) , "," ));
    plant_iReport_print(get_report(), "== reap ingestion ==");
    v = plant_list_variance( nums );
    plant_iReport_print(get_report(), v);
    p = plant_list_product( nums );
    plant_iReport_print(get_report(), p);
    lo = plant_list_min( nums );
    plant_iReport_print(get_report(), lo);
    hi = plant_list_max( nums );
    plant_iReport_print(get_report(), hi);
    sp = plant_list_range( nums );
    plant_iReport_print(get_report(), sp);
    md = plant_list_mode( nums );
    plant_iReport_print(get_report(), md);
    plant_iReport_print(get_report(), "== edges ==");
    PlantArray* empty = plant_list_make(0);
    plant_iReport_print(get_report(), plant_list_variance( empty ));
    plant_iReport_print(get_report(), plant_list_stddev( empty ));
    plant_iReport_print(get_report(), plant_list_product( empty ));
    plant_iReport_print(get_report(), plant_list_min( empty ));
    plant_iReport_print(get_report(), plant_list_max( empty ));
    plant_iReport_print(get_report(), plant_list_range( empty ));
    plant_iReport_print(get_report(), _cat3("[", plant_list_mode( empty ), "]"));
    plant_iReport_print(get_report(), _cat3("[", plant_join( plant_list_mode( plant_list_make(2, _from_long(1), _from_long(1)) ) , "" ), "]"));
    plant_iReport_print(get_report(), "== type coercion ==");
    PlantArray* mixed = plant_list_make(4, "2", "x", "4", "b");
    plant_iReport_print(get_report(), plant_list_product( mixed ));
    plant_iReport_print(get_report(), plant_list_min( mixed ));
    plant_iReport_print(get_report(), plant_list_max( mixed ));
    plant_iReport_print(get_report(), plant_list_variance( plant_list_make(2, "x", "y") ));
    plant_iReport_print(get_report(), plant_list_mode( plant_list_make(3, "3", "3", "5") ));
    return 0;
}

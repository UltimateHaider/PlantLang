#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t f1 = "";
  tx_t f2 = "";
  tx_t f3 = "";
  tx_t f4 = "";
  tx_t f5 = "";
  tx_t c1 = "";
  tx_t c2 = "";
  tx_t ch3 = "";
  tx_t c3 = "";
  tx_t ch4 = "";
  tx_t c4 = "";
  tx_t ch5 = "";
  tx_t c5 = "";
  tx_t ch6 = "";
  tx_t c6 = "";
  tx_t z1 = "";
  tx_t z2 = "";
  tx_t zc3 = "";
  tx_t z3 = "";
  tx_t zc4 = "";
  tx_t z4 = "";
  tx_t g1 = "";
  tx_t g2 = "";
  tx_t g3 = "";
  tx_t g4 = "";
  tx_t l1 = "";
  tx_t l2 = "";
  tx_t l3 = "";
  tx_t n1 = "";
  tx_t n2 = "";
  tx_t n3 = "";
    plant_iReport_print(get_report(), "== flatten ==");
    f1 = plant_join( plant_list_flatten( plant_list_make(2, plant_list_make(2, "a", "b"), plant_list_make(1, "c")) ) , " " );
    plant_iReport_print(get_report(), f1);
    f2 = plant_join( plant_list_flatten( plant_list_make(3, _from_long(1), plant_list_make(2, _from_long(2), _from_long(3)), _from_long(4)) ) , " " );
    plant_iReport_print(get_report(), f2);
    f3 = plant_join( plant_list_flatten( plant_list_make(0) ) , " " );
    plant_iReport_print(get_report(), _cat3("[", f3, "]"));
    f4 = plant_list_flatten( "abc" );
    plant_iReport_print(get_report(), f4);
    f5 = plant_join( plant_list_flatten( plant_list_make(1, plant_list_make(2, _from_long(1), plant_list_make(1, _from_long(2)))) ) , " " );
    plant_iReport_print(get_report(), f5);
    plant_iReport_print(get_report(), "== chunk ==");
    c1 = plant_join( plant_list_chunk( plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4)) , 2 ) , " " );
    plant_iReport_print(get_report(), c1);
    c2 = plant_join( plant_list_chunk( plant_list_make(5, _from_long(1), _from_long(2), _from_long(3), _from_long(4), _from_long(5)) , 2 ) , " " );
    plant_iReport_print(get_report(), c2);
    ch3 = plant_list_chunk( plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4)) , 2 );
    c3 = _from_long(plant_array_length(ch3));
    plant_iReport_print(get_report(), c3);
    ch4 = plant_list_chunk( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 5 );
    c4 = _from_long(plant_array_length(ch4));
    plant_iReport_print(get_report(), c4);
    ch5 = plant_list_chunk( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 0 );
    c5 = _from_long(plant_array_length(ch5));
    plant_iReport_print(get_report(), c5);
    ch6 = plant_list_chunk( plant_list_make(0) , 2 );
    c6 = _from_long(plant_array_length(ch6));
    plant_iReport_print(get_report(), c6);
    plant_iReport_print(get_report(), "== zip ==");
    z1 = plant_join( plant_list_zip( plant_list_make(2, "a", "b") , plant_list_make(2, _from_long(1), _from_long(2)) ) , " " );
    plant_iReport_print(get_report(), z1);
    z2 = plant_join( plant_list_zip( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , plant_list_make(1, "x") ) , " " );
    plant_iReport_print(get_report(), z2);
    zc3 = plant_list_zip( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , plant_list_make(2, "x", "y") );
    z3 = _from_long(plant_array_length(zc3));
    plant_iReport_print(get_report(), z3);
    zc4 = plant_list_zip( plant_list_make(0) , plant_list_make(2, _from_long(1), _from_long(2)) );
    z4 = _from_long(plant_array_length(zc4));
    plant_iReport_print(get_report(), z4);
    plant_iReport_print(get_report(), "== filter_gt ==");
    g1 = plant_join( plant_list_filter_gt( plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4)) , 2 ) , " " );
    plant_iReport_print(get_report(), g1);
    g2 = plant_join( plant_list_filter_gt( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 3 ) , " " );
    plant_iReport_print(get_report(), _cat3("[", g2, "]"));
    g3 = plant_join( plant_list_filter_gt( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 0 ) , " " );
    plant_iReport_print(get_report(), g3);
    g4 = plant_join( plant_list_filter_gt( plant_list_make(4, "a", _from_long(1), "b", _from_long(2)) , 1 ) , " " );
    plant_iReport_print(get_report(), g4);
    plant_iReport_print(get_report(), "== filter_lt ==");
    l1 = plant_join( plant_list_filter_lt( plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4)) , 3 ) , " " );
    plant_iReport_print(get_report(), l1);
    l2 = plant_join( plant_list_filter_lt( plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) , 1 ) , " " );
    plant_iReport_print(get_report(), _cat3("[", l2, "]"));
    l3 = plant_join( plant_list_filter_lt( plant_list_make(3, _from_long(5), _from_long(10), _from_long(15)) , 10 ) , " " );
    plant_iReport_print(get_report(), l3);
    plant_iReport_print(get_report(), "== nested ==");
    n1 = plant_join( plant_list_flatten( plant_list_chunk( plant_list_make(4, _from_long(1), _from_long(2), _from_long(3), _from_long(4)) , 2 ) ) , " " );
    plant_iReport_print(get_report(), n1);
    n2 = plant_join( plant_list_sort( plant_list_filter_gt( plant_list_make(4, _from_long(5), _from_long(1), _from_long(4), _from_long(2)) , 2 ) ) , " " );
    plant_iReport_print(get_report(), n2);
    n3 = plant_join( plant_list_flatten( plant_list_zip( plant_list_make(2, "a", "b") , plant_list_make(3, _from_long(1), _from_long(2), _from_long(3)) ) ) , " " );
    plant_iReport_print(get_report(), n3);
    plant_iReport_print(get_report(), "done");
    return 0;
}

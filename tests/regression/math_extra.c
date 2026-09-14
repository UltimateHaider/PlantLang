#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t p = "";
  tx_t l = "";
    plant_iReport_print(get_report(), "== min/max ==");
    plant_iReport_print(get_report(), math_min(_from_double( 3 ), _from_double( 5 )));
    plant_iReport_print(get_report(), math_max(_from_double( 3 ), _from_double( 5 )));
    plant_iReport_print(get_report(), math_min(_from_double( - 1 ), _from_double( 4 )));
    plant_iReport_print(get_report(), math_min(_from_double( 3 ), _from_double( 3 )));
    plant_iReport_print(get_report(), math_min(_from_double( 2.5 ), _from_double( 1.5 )));
    plant_iReport_print(get_report(), math_max(_from_double( 2.5 ), _from_double( 1.5 )));
    plant_iReport_print(get_report(), math_min(_from_double( 0 ), _from_double( 5 )));
    plant_iReport_print(get_report(), math_max(_from_double( 0 ), _from_double( 5 )));
    plant_iReport_print(get_report(), math_min(_from_double( 5 ), _from_double( 0 )));
    plant_iReport_print(get_report(), math_max(_from_double( 5 ), _from_double( 0 )));
    plant_iReport_print(get_report(), "== tan/atan ==");
    plant_iReport_print(get_report(), math_tan(_from_double( 0 )));
    plant_iReport_print(get_report(), math_tan(_from_double( 1 )));
    plant_iReport_print(get_report(), math_tan(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_atan(_from_double( 0 )));
    plant_iReport_print(get_report(), math_atan(_from_double( 1 )));
    plant_iReport_print(get_report(), math_atan(_from_double( 0.5 )));
    plant_iReport_print(get_report(), "== cot ==");
    plant_iReport_print(get_report(), math_cot(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_cot(_from_double( 1 )));
    plant_iReport_print(get_report(), "== asin/acos ==");
    plant_iReport_print(get_report(), math_asin(_from_double( 0 )));
    plant_iReport_print(get_report(), math_asin(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_asin(_from_double( 2 )));
    plant_iReport_print(get_report(), math_acos(_from_double( 0 )));
    plant_iReport_print(get_report(), math_acos(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_acos(_from_double( 2 )));
    plant_iReport_print(get_report(), "== atan2 ==");
    plant_iReport_print(get_report(), math_atan2(_from_double( 1 ), _from_double( 1 )));
    plant_iReport_print(get_report(), math_atan2(_from_double( 1 ), _from_double( 0 )));
    plant_iReport_print(get_report(), math_atan2(_from_double( 0 ), _from_double( 0 )));
    plant_iReport_print(get_report(), math_atan2(_from_double( - 1 ), _from_double( - 1 )));
    plant_iReport_print(get_report(), "== hyperbolics ==");
    plant_iReport_print(get_report(), math_sinh(_from_double( 0 )));
    plant_iReport_print(get_report(), math_sinh(_from_double( 1 )));
    plant_iReport_print(get_report(), math_cosh(_from_double( 0 )));
    plant_iReport_print(get_report(), math_cosh(_from_double( 1 )));
    plant_iReport_print(get_report(), math_tanh(_from_double( 0 )));
    plant_iReport_print(get_report(), math_tanh(_from_double( 1 )));
    plant_iReport_print(get_report(), "== exp/log ==");
    plant_iReport_print(get_report(), math_exp(_from_double( 0 )));
    plant_iReport_print(get_report(), math_exp(_from_double( 1 )));
    plant_iReport_print(get_report(), math_expm1(_from_double( 0 )));
    plant_iReport_print(get_report(), math_expm1(_from_double( 1 )));
    plant_iReport_print(get_report(), math_log10(_from_double( 10 )));
    plant_iReport_print(get_report(), math_log10(_from_double( 100 )));
    plant_iReport_print(get_report(), math_log10(_from_double( 0 )));
    plant_iReport_print(get_report(), math_log2(_from_double( 2 )));
    plant_iReport_print(get_report(), math_log2(_from_double( 8 )));
    plant_iReport_print(get_report(), math_log2(_from_double( 0 )));
    plant_iReport_print(get_report(), math_log1p(_from_double( 0 )));
    plant_iReport_print(get_report(), math_log1p(_from_double( 1 )));
    plant_iReport_print(get_report(), "== hypot ==");
    plant_iReport_print(get_report(), math_hypot(_from_double( 3 ), _from_double( 4 )));
    plant_iReport_print(get_report(), math_hypot(_from_double( 0 ), _from_double( 0 )));
    plant_iReport_print(get_report(), math_hypot(_from_double( 1 ), _from_double( 1 )));
    plant_iReport_print(get_report(), math_hypot(_from_double( 1.5 ), _from_double( 2 )));
    plant_iReport_print(get_report(), "== nested ==");
    plant_iReport_print(get_report(), math_min( math_max(_from_double( 2 ), _from_double( 9 )) , _from_double( 4 )));
    plant_iReport_print(get_report(), math_max( math_min(_from_double( 2 ), _from_double( 9 )) , _from_double( 4 )));
    plant_iReport_print(get_report(), math_tan(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_atan(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_hypot( math_tan(_from_double( 0.5 )) , _from_double( 1 )));
    plant_iReport_print(get_report(), math_log10( math_expm1(_from_double( 9 )) ));
    plant_iReport_print(get_report(), "== math: FFI module still works ==");
    p = math_PI;
    plant_iReport_print(get_report(), p);
    l = math_LOG("10");
    plant_iReport_print(get_report(), l);
    return 0;
}

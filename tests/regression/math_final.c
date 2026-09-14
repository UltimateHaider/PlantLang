#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "== sec/csc ==");
    plant_iReport_print(get_report(), math_sec(_from_double( 0 )));
    plant_iReport_print(get_report(), math_sec(_from_double( 1 )));
    plant_iReport_print(get_report(), math_csc(_from_double( 1 )));
    plant_iReport_print(get_report(), math_csc(_from_double( 0.5 )));
    plant_iReport_print(get_report(), "== inverse hyperbolics ==");
    plant_iReport_print(get_report(), math_asinh(_from_double( 0 )));
    plant_iReport_print(get_report(), math_asinh(_from_double( 1 )));
    plant_iReport_print(get_report(), math_acosh(_from_double( 1 )));
    plant_iReport_print(get_report(), math_acosh(_from_double( 2 )));
    plant_iReport_print(get_report(), math_acosh(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_atanh(_from_double( 0 )));
    plant_iReport_print(get_report(), math_atanh(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_atanh(_from_double( 1 )));
    plant_iReport_print(get_report(), "== erf/erfc ==");
    plant_iReport_print(get_report(), math_erf(_from_double( 0 )));
    plant_iReport_print(get_report(), math_erf(_from_double( 1 )));
    plant_iReport_print(get_report(), math_erfc(_from_double( 0 )));
    plant_iReport_print(get_report(), math_erfc(_from_double( 1 )));
    plant_iReport_print(get_report(), "== gamma/lgamma ==");
    plant_iReport_print(get_report(), math_gamma(_from_double( 1 )));
    plant_iReport_print(get_report(), math_gamma(_from_double( 5 )));
    plant_iReport_print(get_report(), math_gamma(_from_double( 0.5 )));
    plant_iReport_print(get_report(), math_lgamma(_from_double( 1 )));
    plant_iReport_print(get_report(), math_lgamma(_from_double( 5 )));
    plant_iReport_print(get_report(), "== exp2 ==");
    plant_iReport_print(get_report(), math_exp2(_from_double( 0 )));
    plant_iReport_print(get_report(), math_exp2(_from_double( 3 )));
    plant_iReport_print(get_report(), math_exp2(_from_double( 0.5 )));
    plant_iReport_print(get_report(), "== log_base ==");
    plant_iReport_print(get_report(), math_log_base(_from_double( 8 ), _from_double( 2 )));
    plant_iReport_print(get_report(), math_log_base(_from_double( 100 ), _from_double( 10 )));
    plant_iReport_print(get_report(), math_log_base(_from_double( 0 ), _from_double( 2 )));
    plant_iReport_print(get_report(), math_log_base(_from_double( 8 ), _from_double( 1 )));
    plant_iReport_print(get_report(), math_log_base(_from_double( 8 ), _from_double( 0 )));
    plant_iReport_print(get_report(), "== nested ==");
    plant_iReport_print(get_report(), math_exp2( math_log_base(_from_double( 8 ), _from_double( 2 )) ));
    plant_iReport_print(get_report(), math_log_base( math_exp2(_from_double( 10 )) , _from_double( 2 )));
    plant_iReport_print(get_report(), math_sec( math_acosh(_from_double( 2 )) ));
    plant_iReport_print(get_report(), math_min( math_max( math_gamma(_from_double( 5 )) , _from_double( 1 )) , _from_double( 30 )));
    return 0;
}

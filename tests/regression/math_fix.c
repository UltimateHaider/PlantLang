#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t lr = "";
  tx_t a = "";
  tx_t s = "";
  tx_t c = "";
  tx_t t = "";
  tx_t p = "";
  tx_t mn = "";
  tx_t mx = "";
  tx_t hp = "";
  tx_t sc = "";
  tx_t cs = "";
  tx_t lb = "";
  tx_t gm = "";
  tx_t lg = "";
  tx_t e2 = "";
  tx_t pi = "";
    plant_iReport_print(get_report(), "== legacy decimals ==");
    plant_iReport_print(get_report(), plant_abs(_from_double( - 2.5 )));
    plant_iReport_print(get_report(), plant_abs(_from_double( 2.5 )));
    plant_iReport_print(get_report(), plant_round(_from_double( 3.7 )));
    plant_iReport_print(get_report(), plant_round(_from_double( - 3.2 )));
    plant_iReport_print(get_report(), plant_ceil(_from_double( 3.2 )));
    plant_iReport_print(get_report(), plant_floor(_from_double( 3.9 )));
    plant_iReport_print(get_report(), plant_pow(_from_double( 2.5 ), _from_double( 2 )));
    plant_iReport_print(get_report(), plant_sin(_from_double( 0.5 )));
    plant_iReport_print(get_report(), plant_cos(_from_double( 0.5 )));
    plant_iReport_print(get_report(), plant_sqrt(_from_double( 2.25 )));
    plant_iReport_print(get_report(), plant_sqrt(_from_double( - 1 )));
    plant_iReport_print(get_report(), "== bare log ==");
    plant_iReport_print(get_report(), math_log(_from_double( 10 )));
    plant_iReport_print(get_report(), math_log(_from_double( 100 )));
    plant_iReport_print(get_report(), math_log(_from_double( 1 )));
    plant_iReport_print(get_report(), math_log(_from_double( - 1 )));
    lr = math_log(_from_double( 8 ));
    plant_iReport_print(get_report(), lr);
    plant_iReport_print(get_report(), math_exp( math_log(_from_double( 10 )) ));
    plant_iReport_print(get_report(), "== module forms ==");
    a = math_ABS("-2.5");
    plant_iReport_print(get_report(), a);
    s = math_SIN("0.5");
    plant_iReport_print(get_report(), s);
    c = math_COS("0");
    plant_iReport_print(get_report(), c);
    t = math_TAN("1");
    plant_iReport_print(get_report(), t);
    p = math_POW("2", "10");
    plant_iReport_print(get_report(), p);
    mn = math_MIN("3", "7");
    plant_iReport_print(get_report(), mn);
    mx = math_MAX("-5", "3");
    plant_iReport_print(get_report(), mx);
    hp = math_HYPOT("3", "4");
    plant_iReport_print(get_report(), hp);
    sc = math_SEC("0");
    plant_iReport_print(get_report(), sc);
    cs = math_CSC("1");
    plant_iReport_print(get_report(), cs);
    lb = math_LOG_BASE("8", "2");
    plant_iReport_print(get_report(), lb);
    gm = math_GAMMA("5");
    plant_iReport_print(get_report(), gm);
    lg = math_LGAMMA("5");
    plant_iReport_print(get_report(), lg);
    e2 = math_EXP2("3");
    plant_iReport_print(get_report(), e2);
    pi = math_PI;
    plant_iReport_print(get_report(), pi);
    return 0;
}

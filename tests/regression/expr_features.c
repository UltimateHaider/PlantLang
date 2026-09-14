#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t eq = "";
  tx_t n = "";
  tx_t esc = "";
  tx_t t = "";
  tx_t u = "";
  tx_t ok = "";
  tx_t uv = "";
  tx_t e = "";
  tx_t ue = "";
  tx_t nn = "";
    plant_iReport_print(get_report(), "== single quotes ==");
    plant_iReport_print(get_report(), "hello");
    plant_iReport_print(get_report(), "it's");
    plant_iReport_print(get_report(), "double");
    eq = plant_list_includes( "hello world" , "wor" );
    plant_iReport_print(get_report(), eq);
    n = plant_list_includes( "abc" , "z" );
    plant_iReport_print(get_report(), n);
    esc = plant_list_includes( "don't" , "'" );
    plant_iReport_print(get_report(), esc);
    plant_iReport_print(get_report(), "== ranges ==");
    plant_iReport_print(get_report(), plant_join(plant_range_list(_from_double(1), _from_double( 4)), "," ));
    plant_iReport_print(get_report(), plant_join(plant_range_list(_from_double(- 2), _from_double( 2)), " " ));
    plant_iReport_print(get_report(), plant_join( plant_range_list(_from_double( 1 ), _from_double( 4 )) , "," ));
    plant_iReport_print(get_report(), plant_join(plant_range_list(_from_double(3), _from_double( 3)), "|" ));
    plant_iReport_print(get_report(), plant_join(plant_range_list(_from_double(5), _from_double( 5)), "|" ));
    plant_iReport_print(get_report(), "== monads ==");
    t = plant_option_some( "v7" );
    u = plant_unwrap(t);
    plant_iReport_print(get_report(), u);
    ok = plant_result_ok( "fine" );
    uv = plant_unwrap(ok);
    plant_iReport_print(get_report(), uv);
    e = plant_result_err( "boom" );
    ue = plant_unwrap_err(e);
    plant_iReport_print(get_report(), ue);
    nn = plant_option_none( );
    long chk = plant_is_none ( nn );
    plant_iReport_print(get_report(), _from_long(chk));
    long som = plant_is_some ( t );
    plant_iReport_print(get_report(), _from_long(som));
    return 0;
}

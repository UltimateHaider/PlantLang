#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t helper(long a);
tx_t zero();
tx_t main();


tx_t helper(long a) {
    return _from_long ( a * 2 );
}
tx_t zero() {
    return "Z";
}
tx_t main() {
  tx_t s = "";
  tx_t f = "";
  tx_t j = "";
  tx_t sl = "";
  tx_t u = "";
  tx_t lo = "";
  tx_t ab = "";
  tx_t ro = "";
  tx_t c = "";
  tx_t n = "";
  tx_t x = "";
  tx_t p = "";
  tx_t big = "";
  tx_t z = "";
  tx_t h = "";
  tx_t u2 = "";
  tx_t sp = "";
    PlantArray* lst = plant_list_make ( 3 , "aa" , "bb" , "cc" );
    s = "hello";
    plant_iReport_print(get_report(), _cat("lit=", s));
    f = plant_find( "abc" , "b" );
    plant_iReport_print(get_report(), _cat("find=", f));
    j = plant_join( lst , "-" );
    plant_iReport_print(get_report(), _cat("join=", j));
    sl = plant_slice( "abcdef" , 1 , 3 );
    plant_iReport_print(get_report(), _cat("slice=", sl));
    u = plant_upper( "AbC" );
    plant_iReport_print(get_report(), _cat("upper=", u));
    lo = plant_lower( "AbC" );
    plant_iReport_print(get_report(), _cat("lower=", lo));
    ab = plant_abs(_from_double( - 7 ));
    plant_iReport_print(get_report(), _cat("abs=", ab));
    ro = plant_round( "2.7" );
    plant_iReport_print(get_report(), _cat("round=", ro));
    c = _from_long(plant_array_length(lst));
    plant_iReport_print(get_report(), _cat("count=", c));
    n = _from_long(2+3);
    plant_iReport_print(get_report(), _cat("arith=", n));
    x = plant_list_get(lst ,  0 );
    plant_iReport_print(get_report(), _cat("idx=", x));
    p = _cat(plant_list_get(lst ,  1 ), "!");
    plant_iReport_print(get_report(), _cat("idxcat=", p));
    big = _cat(plant_upper( plant_join( lst , ":" ) ), "!");
    plant_iReport_print(get_report(), _cat("nested=", big));
    z = zero();
    plant_iReport_print(get_report(), _cat("zero=", z));
    h = helper(3);
    plant_iReport_print(get_report(), _cat("call=", h));
    u2 = plant_upper( helper ( 2 ) );
    plant_iReport_print(get_report(), _cat("callin=", u2));
    sp = strings_REPLACE("a,b", ",", "-");
    plant_iReport_print(get_report(), _cat("module=", sp));
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t i1 = "";
  tx_t i2 = "";
  tx_t i3 = "";
  tx_t i4 = "";
  tx_t i5 = "";
  tx_t s1 = "";
  tx_t s2 = "";
  tx_t s3 = "";
  tx_t s4 = "";
  tx_t e1 = "";
  tx_t e2 = "";
  tx_t e3 = "";
  tx_t e4 = "";
  tx_t x1 = "";
  tx_t x2 = "";
  tx_t x3 = "";
  tx_t x4 = "";
  tx_t x5 = "";
  tx_t p1 = "";
  tx_t p2 = "";
  tx_t p3 = "";
  tx_t p4 = "";
  tx_t q1 = "";
  tx_t q2 = "";
  tx_t q3 = "";
  tx_t t = "";
    plant_iReport_print(get_report(), "== includes ==");
    i1 = plant_list_includes( "hello world" , "lo wo" );
    plant_iReport_print(get_report(), i1);
    i2 = plant_list_includes( "hello" , "xyz" );
    plant_iReport_print(get_report(), i2);
    i3 = plant_list_includes( "hello" , "" );
    plant_iReport_print(get_report(), i3);
    i4 = plant_list_includes( "" , "x" );
    plant_iReport_print(get_report(), i4);
    i5 = plant_list_includes( "a" , "a" );
    plant_iReport_print(get_report(), i5);
    plant_iReport_print(get_report(), "== starts_with ==");
    s1 = string_starts_with( "hello" , "he" );
    plant_iReport_print(get_report(), s1);
    s2 = string_starts_with( "hello" , "lo" );
    plant_iReport_print(get_report(), s2);
    s3 = string_starts_with( "hello" , "" );
    plant_iReport_print(get_report(), s3);
    s4 = string_starts_with( "" , "h" );
    plant_iReport_print(get_report(), s4);
    plant_iReport_print(get_report(), "== ends_with ==");
    e1 = string_ends_with( "hello" , "llo" );
    plant_iReport_print(get_report(), e1);
    e2 = string_ends_with( "hello" , "hel" );
    plant_iReport_print(get_report(), e2);
    e3 = string_ends_with( "hello" , "" );
    plant_iReport_print(get_report(), e3);
    e4 = string_ends_with( "" , "o" );
    plant_iReport_print(get_report(), e4);
    plant_iReport_print(get_report(), "== repeat ==");
    x1 = string_repeat( "ab" , 3 );
    plant_iReport_print(get_report(), x1);
    x2 = string_repeat( "x" , 0 );
    plant_iReport_print(get_report(), _cat3("[", x2, "]"));
    x3 = string_repeat( "x" , 1 );
    plant_iReport_print(get_report(), x3);
    x4 = string_repeat( "" , 5 );
    plant_iReport_print(get_report(), _cat3("[", x4, "]"));
    x5 = string_repeat( "12" , 4 );
    plant_iReport_print(get_report(), x5);
    plant_iReport_print(get_report(), "== pad (right) ==");
    p1 = string_pad( "x" , 5 , "." );
    plant_iReport_print(get_report(), p1);
    p2 = string_pad( "hello" , 3 , "." );
    plant_iReport_print(get_report(), p2);
    p3 = string_pad( "ab" , 4 , "-" );
    plant_iReport_print(get_report(), p3);
    p4 = string_pad( "ok" , 2 , "." );
    plant_iReport_print(get_report(), p4);
    plant_iReport_print(get_report(), "== pad_left ==");
    q1 = string_pad_left( "x" , 5 , "." );
    plant_iReport_print(get_report(), q1);
    q2 = string_pad_left( "hello" , 3 , "." );
    plant_iReport_print(get_report(), q2);
    q3 = string_pad_left( "ab" , 4 , "-" );
    plant_iReport_print(get_report(), q3);
    plant_iReport_print(get_report(), "== expression positions ==");
    plant_iReport_print(get_report(), _cat3("[", plant_list_includes( "abcd" , "bc" ), "]"));
    t = string_repeat( "z" , 2 );
    plant_iReport_print(get_report(), t);
    plant_iReport_print(get_report(), string_pad( "a" , 3 , "*" ));
    plant_iReport_print(get_report(), string_pad_left( string_repeat( "y" , 2 ) , 4 , "_" ));
    plant_iReport_print(get_report(), string_starts_with( "prefix!" , "pre" ));
    plant_iReport_print(get_report(), string_ends_with( "suffix" , "fix" ));
    plant_iReport_print(get_report(), "done");
    return 0;
}

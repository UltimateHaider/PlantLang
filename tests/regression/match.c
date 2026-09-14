#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t classify(tx_t v);
tx_t tag_of(tx_t v);
tx_t flag_of(tx_t v);
tx_t main();


tx_t classify(tx_t v) {
    {
      tx_t __mt = v;
      if (_match_eq(__mt, "1")) {
    return "one";
      } else if (_match_eq(__mt, "2")) {
    return "two";
      } else {
    return "other";
      }
    }
  return classify;
}
tx_t tag_of(tx_t v) {
    {
      tx_t __mt = v;
      if (plant_array_length(__mt) == 2 && _match_eq(plant_list_get(__mt, 0), "Ok")) {
          tx_t x = _match_extract(__mt, "Ok");
    return _cat("ok:", x);
      } else if (plant_array_length(__mt) == 2 && _match_eq(plant_list_get(__mt, 0), "Err")) {
          tx_t e = _match_extract(__mt, "Err");
    return _cat("err:", e);
      } else {
    return "none";
      }
    }
  return tag_of;
}
tx_t flag_of(tx_t v) {
    {
      tx_t __mt = v;
      if (_match_eq(__mt, "Maybe")) {
    return "maybe";
      } else if (plant_array_length(__mt) == 2 && _match_eq(plant_list_get(__mt, 0), "Some")) {
          tx_t n = _match_extract(__mt, "Some");
    plant_iReport_print(get_report(), _cat("got ", n));
    return "some";
      } else {
    return "bare";
      }
    }
  return flag_of;
}
tx_t main() {
    plant_iReport_print(get_report(), _cat("lit: ", classify ( 1 )));
    plant_iReport_print(get_report(), _cat("lit: ", classify ( 2 )));
    plant_iReport_print(get_report(), _cat("lit: ", classify ( 9 )));
    plant_iReport_print(get_report(), _cat("str: ", classify ( "1" )));
    plant_iReport_print(get_report(), _cat("str: ", classify ( "x" )));
    plant_iReport_print(get_report(), _cat("bind: ", tag_of ( plant_list_make ( 2 , "Ok" , "7" ) )));
    plant_iReport_print(get_report(), _cat("bind: ", tag_of ( plant_list_make ( 2 , "Err" , "oops" ) )));
    plant_iReport_print(get_report(), _cat("bind: ", tag_of ( plant_list_make ( 2 , "Other" , "ignored" ) )));
    plant_iReport_print(get_report(), _cat("bind: ", tag_of ( plant_list_make ( 1 , "Ok" ) )));
    plant_iReport_print(get_report(), _cat("flag: ", flag_of ( plant_list_make ( 2 , "Maybe" , "x" ) )));
    plant_iReport_print(get_report(), _cat("flag: ", flag_of ( plant_list_make ( 2 , "Some" , "42" ) )));
  return main;
}

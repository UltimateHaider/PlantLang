#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();
tx_t plant_id_NUM(long v);
tx_t plant_id_TX(tx_t v);


tx_t main() {
  tx_t a = "";
  tx_t b = "";
  tx_t c = "";
    a = plant_id_NUM(7);
    plant_iReport_print(get_report(), _cat("a=", a));
    plant_iReport_print(get_report(), _cat3("a+", a, "!"));
    b = plant_id_TX("str");
    plant_iReport_print(get_report(), _cat("b=", b));
    c = plant_id_NUM(12);
    plant_iReport_print(get_report(), _cat4("c=", c, "+", a));
    return 0;
}
tx_t plant_id_NUM(long v) {
    return _from_long(v);
}
tx_t plant_id_TX(tx_t v) {
    return v;
}

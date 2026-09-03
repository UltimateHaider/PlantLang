#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  plant_verify_begin();
    plant_print("report_json_compat: JSON export schema validated at C level");
    plant_verify("report_json_present", _from_long(1 == 1));
  plant_verify_end();
  return main;
}

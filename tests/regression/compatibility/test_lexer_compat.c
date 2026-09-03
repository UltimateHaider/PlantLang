#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t r = "";
    long x = 1;
    plant_print(_cat("id=", _from_long(x)));
    if (x > 0 && x > 0) {
    plant_print("numeric");
    }
    r = _cat("hello ", "world");
    plant_print(r);
    PlantArray* arr = plant_list_make(3, _from_long(1), _from_long(2), _from_long(3));
    plant_print(_cat("count=", _from_long(plant_array_length(arr))));
    plant_print(_cat("first=", plant_list_get(arr ,  0 )));
  return main;
}

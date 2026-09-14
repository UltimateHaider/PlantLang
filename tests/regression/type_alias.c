#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

typedef long MyInt;
typedef tx_t Name;
typedef PlantArray* IntList;
typedef PlantArray* SameList;
tx_t main();


tx_t main() {
    long n = 7;
    plant_iReport_print(get_report(), _from_long(n));
    tx_t nm = "ada";
    plant_iReport_print(get_report(), nm);
    tx_t xs = plant_list_make(2, _from_long(1), _from_long(2));
    plant_iReport_print(get_report(), plant_join( xs , "," ));
    tx_t ys = plant_list_make(2, _from_long(5), _from_long(6));
    plant_iReport_print(get_report(), plant_join( ys , "-" ));
    long v = 42;
    plant_iReport_print(get_report(), _from_long(v));
    tx_t w = "grace";
    plant_iReport_print(get_report(), w);
    return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t g1 = "";
  tx_t g2 = "";
  tx_t g3 = "";
  tx_t g4 = "";
  tx_t g5 = "";
  tx_t g6 = "";
  tx_t g7 = "";
    PlantArray* m = plant_list_make ( 0 );
    m = plant_link(m, "k1", "v1");
    m = plant_link(m, "k2", "v2");
    g1 = _map_get(m, "k1");
    g2 = _map_get(m, "k2");
    plant_iReport_print(get_report(), _cat("first link k1 = ", g1));
    plant_iReport_print(get_report(), _cat("first link k2 = ", g2));
    plant_iReport_print(get_report(), _cat("pair count after 2 links = ", _from_long ( plant_array_length(m) )));
    m = plant_link(m, "k1", "updated");
    g3 = _map_get(m, "k1");
    g4 = _map_get(m, "k2");
    plant_iReport_print(get_report(), _cat("updated k1 = ", g3));
    plant_iReport_print(get_report(), _cat("k2 unchanged = ", g4));
    plant_iReport_print(get_report(), _cat("pair count after update = ", _from_long ( plant_array_length(m) )));
    m = plant_link(m, "k3", "v3");
    m = plant_link(m, "k3", "v3b");
    g5 = _map_get(m, "k3");
    plant_iReport_print(get_report(), _cat("dup link k3 = ", g5));
    plant_iReport_print(get_report(), _cat("pair count after dup link = ", _from_long ( plant_array_length(m) )));
    PlantArray* fresh = NULL;
    fresh = plant_link(fresh, "alpha", "beta");
    g6 = _map_get(fresh, "alpha");
    plant_iReport_print(get_report(), _cat("null target instantiated = ", g6));
    fresh = plant_link(fresh, "gamma", "delta");
    g7 = _map_get(fresh, "gamma");
    plant_iReport_print(get_report(), _cat("null target second link = ", g7));
    plant_iReport_print(get_report(), _cat("null target pair count = ", _from_long ( plant_array_length(fresh) )));
    return 0;
}

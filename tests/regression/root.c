#include <plant_compat.h>

static const char *g_early = "hoisted";
static const char *scoped_g = "scoped-root";
static const char *nested_g = "nested-root";
/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t show_root();


tx_t show_root() {
    static const char *scoped_g = "local-shadow";
    plant_iReport_print(get_report(), _cat(_cat4(g_early, "|", scoped_g, "|"), nested_g));
    return "ok-from-action";
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t r1 = "";
  plant_iReport_print(get_report(), g_early);
  r1 = show_root();
  plant_iReport_print(get_report(), r1);
  plant_iReport_print(get_report(), _cat(_cat4(g_early, "|", scoped_g, "|"), nested_g));
  plant_iReport_print(get_report(), "root done");
  plant_async_drain();
  return 0;
}

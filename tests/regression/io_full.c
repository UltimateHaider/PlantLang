#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/



int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t a = "";
  tx_t b = "";
  tx_t c = "";
  tx_t d = "";
  tx_t f = "";
  tx_t e1 = "";
  tx_t e2 = "";
  a = io_SHOWLN("hello io");
  b = io_SHOWLN("");
  c = io_SHOWLN("alpha\nbeta\ngamma");
  d = io_SHOWLN("   padded   ");
  f = io_FLUSH();
  e1 = io_SHOWLN("after flush");
  plant_iReport_print(get_report(), _cat("flush rc = ", f));
  e2 = io_SHOWLN("tail");
  plant_async_drain();
  return 0;
}

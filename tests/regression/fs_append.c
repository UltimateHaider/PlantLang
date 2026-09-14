#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/



int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t n1 = "";
  tx_t e1 = "";
  tx_t c1 = "";
  tx_t n2 = "";
  tx_t c2 = "";
  tx_t n3 = "";
  tx_t c3 = "";
  tx_t n4 = "";
  tx_t c4 = "";
  tx_t n5 = "";
  n1 = fs_APPEND("/tmp/plantlang_fs_append_a.txt", "first line");
  e1 = fs_EXISTS("/tmp/plantlang_fs_append_a.txt");
  c1 = fs_READ("/tmp/plantlang_fs_append_a.txt");
  plant_iReport_print(get_report(), _cat("append new file = ", n1));
  plant_iReport_print(get_report(), _cat("exists after append = ", e1));
  plant_iReport_print(get_report(), _cat("content1 = ", c1));
  n2 = fs_APPEND("/tmp/plantlang_fs_append_a.txt", "second line");
  c2 = fs_READ("/tmp/plantlang_fs_append_a.txt");
  plant_iReport_print(get_report(), _cat("append existing file = ", n2));
  plant_iReport_print(get_report(), _cat("content2 = ", c2));
  n3 = fs_APPEND("/tmp/plantlang_fs_append_a.txt", "");
  c3 = fs_READ("/tmp/plantlang_fs_append_a.txt");
  plant_iReport_print(get_report(), _cat("append empty text = ", n3));
  plant_iReport_print(get_report(), _cat("content3 = ", c3));
  n4 = fs_APPEND("/tmp/plantlang_fs_append_b.txt", "b-one");
  c4 = fs_READ("/tmp/plantlang_fs_append_b.txt");
  plant_iReport_print(get_report(), _cat("append second file = ", n4));
  plant_iReport_print(get_report(), _cat("content4 = ", c4));
  n5 = fs_APPEND("/tmp/plantlang_no_such_dir_plantlang/x.txt", "x");
  plant_iReport_print(get_report(), _cat("append bad path = ", n5));
  plant_async_drain();
  return 0;
}

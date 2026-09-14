#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/



int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t l1 = "";
  tx_t le = "";
  tx_t l10 = "";
  tx_t lhalf = "";
  tx_t l0 = "";
  tx_t lneg = "";
  tx_t pi = "";
  tx_t e = "";
  tx_t sneg = "";
  tx_t szero = "";
  tx_t spos = "";
  tx_t snegz = "";
  tx_t spdec = "";
  tx_t clow = "";
  tx_t cmid = "";
  tx_t chigh = "";
  tx_t cloed = "";
  tx_t chied = "";
  tx_t cdec = "";
  plant_iReport_print(get_report(), "math_full: LOG/PI/E/SIGN/CLAMP");
  l1 = math_LOG("1");
  le = math_LOG("2.718281828459045");
  l10 = math_LOG("10");
  lhalf = math_LOG("0.5");
  l0 = math_LOG("0");
  lneg = math_LOG("-5");
  plant_iReport_print(get_report(), _cat("LOG 1 = ", l1));
  plant_iReport_print(get_report(), _cat("LOG e = ", le));
  plant_iReport_print(get_report(), _cat("LOG 10 = ", l10));
  plant_iReport_print(get_report(), _cat("LOG 0.5 = ", lhalf));
  plant_iReport_print(get_report(), _cat("LOG 0 = ", l0));
  plant_iReport_print(get_report(), _cat("LOG -5 = ", lneg));
  pi = math_PI;
  e = math_E;
  plant_iReport_print(get_report(), _cat("PI = ", pi));
  plant_iReport_print(get_report(), _cat("E = ", e));
  sneg = math_SIGN("-7.5");
  szero = math_SIGN("0");
  spos = math_SIGN("42");
  snegz = math_SIGN("-0.0");
  spdec = math_SIGN("3.99");
  plant_iReport_print(get_report(), _cat("SIGN -7.5 = ", sneg));
  plant_iReport_print(get_report(), _cat("SIGN 0 = ", szero));
  plant_iReport_print(get_report(), _cat("SIGN 42 = ", spos));
  plant_iReport_print(get_report(), _cat("SIGN -0.0 = ", snegz));
  plant_iReport_print(get_report(), _cat("SIGN 3.99 = ", spdec));
  clow = math_CLAMP("-10", "-2", "5");
  cmid = math_CLAMP("3", "-2", "5");
  chigh = math_CLAMP("99", "-2", "5");
  cloed = math_CLAMP("-2", "-2", "5");
  chied = math_CLAMP("5", "-2", "5");
  cdec = math_CLAMP("1.75", "-2", "5");
  plant_iReport_print(get_report(), _cat("CLAMP -10 to -2..5 = ", clow));
  plant_iReport_print(get_report(), _cat("CLAMP 3 to -2..5 = ", cmid));
  plant_iReport_print(get_report(), _cat("CLAMP 99 to -2..5 = ", chigh));
  plant_iReport_print(get_report(), _cat("CLAMP -2 to -2..5 = ", cloed));
  plant_iReport_print(get_report(), _cat("CLAMP 5 to -2..5 = ", chied));
  plant_iReport_print(get_report(), _cat("CLAMP 1.75 to -2..5 = ", cdec));
  plant_async_drain();
  return 0;
}

#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "a");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 0);
      if (setjmp(__w1.buf) == 0) {
    plant_iReport_print(get_report(), "body");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm normal");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "b");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("ZERO_STORM", "z");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ZERO_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    plant_iReport_print(get_report(), "caught");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm caught");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "c");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    {
      PlantWeather __w3 = {0};
      plant_weather_enter(&__w3, 1);
      if (setjmp(__w3.buf) == 0) {
    plant_throw("LOCK_STORM", "l");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w3);
        plant_weather_handling_begin(&__w3);
        if (plant_storm_match(__et, "ZERO_STORM")) {
          plant_weather_shelter_enter(&__w3);
          __w3.handled = 1;
    plant_iReport_print(get_report(), "wrong");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w3);
        }
        plant_weather_handling_end(&__w3);
      }
    plant_iReport_print(get_report(), "calm unmatched");
      plant_calm(&__w3);
    }
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "LOCK_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    plant_iReport_print(get_report(), "outer caught");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm outer");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "d");
    for (long i = 1; i <= 3; i += 1) {
    plant_iReport_print(get_report(), _cat("iter ", _from_long(i)));
    {
      PlantWeather __w3 = {0};
      plant_weather_enter(&__w3, 0);
      if (setjmp(__w3.buf) == 0) {
    if (i > 1) {
                  plant_iReport_print(get_report(), "calm break");
  plant_calm(&__w3);
                break;
    }
    plant_iReport_print(get_report(), _cat("body ", _from_long(i)));
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w3);
        plant_weather_handling_begin(&__w3);
        plant_weather_handling_end(&__w3);
      }
    plant_iReport_print(get_report(), "calm break");
      plant_calm(&__w3);
    }
    plant_iReport_print(get_report(), _cat("tail ", _from_long(i)));
    }
    plant_iReport_print(get_report(), "loop done");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 0);
      if (setjmp(__w1.buf) == 0) {
      plant_iReport_print(get_report(), "calm give");
  plant_calm(&__w1);
  return 0;
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm give");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "after give - BUG");
    return 0;
}

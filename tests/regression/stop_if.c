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
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
        if (5 > 3) {
          plant_throw("STOP_STORM", NULL);
        }
    plant_iReport_print(get_report(), "never");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "STOP_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), _cat("stopped: ", err));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm ran");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "b");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
        if (3 > 5) {
          plant_throw("STOP_STORM", NULL);
        }
    plant_iReport_print(get_report(), "not stopped");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "STOP_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    plant_iReport_print(get_report(), "bad");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "c");
    long n = 7;
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
        if (n >= 7) {
          plant_throw("STOP_STORM", NULL);
        }
    plant_iReport_print(get_report(), "never2");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "STOP_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), _cat("num stop: ", err));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm2");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "done");
    return 0;
}

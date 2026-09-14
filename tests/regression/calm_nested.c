#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), "top");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_iReport_print(get_report(), "w1 body");
    {
      PlantWeather __w3 = {0};
      plant_weather_enter(&__w3, 1);
      if (setjmp(__w3.buf) == 0) {
    plant_iReport_print(get_report(), "w2 body");
    {
      PlantWeather __w5 = {0};
      plant_weather_enter(&__w5, 1);
      if (setjmp(__w5.buf) == 0) {
    plant_throw("ZERO_STORM", "deep");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w5);
        plant_weather_handling_begin(&__w5);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w5);
          __w5.handled = 1;
    plant_iReport_print(get_report(), "w3 caught");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w5);
        }
        plant_weather_handling_end(&__w5);
      }
    plant_iReport_print(get_report(), "w3 calm");
      plant_calm(&__w5);
    }
    plant_iReport_print(get_report(), "w2 after inner");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w3);
        plant_weather_handling_begin(&__w3);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w3);
          __w3.handled = 1;
    plant_iReport_print(get_report(), "w2 catchall - BUG");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w3);
        }
        plant_weather_handling_end(&__w3);
      }
    plant_iReport_print(get_report(), "w2 calm");
      plant_calm(&__w3);
    }
    plant_iReport_print(get_report(), "w1 after w2");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    plant_iReport_print(get_report(), "w1 catchall - BUG");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "w1 calm");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "done");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_iReport_print(get_report(), "empty calm block");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ZERO_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "empty ok");
    return 0;
}

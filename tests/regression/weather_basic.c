#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 2);
      if (setjmp(__w1.buf) == 0) {
    plant_iReport_print(get_report(), "protecting");
    plant_throw("ZERO_STORM", "division by zero");
    plant_iReport_print(get_report(), "unreached");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ZERO_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), _cat("caught: ", err));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        } else if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    plant_iReport_print(get_report(), "any handler");
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm ran");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "after weather");
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("LOCK_STORM", "lock");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t msg = __ev;
    plant_iReport_print(get_report(), _cat("catchall: ", msg));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "catchall calm");
      plant_calm(&__w1);
    }
    return 0;
}

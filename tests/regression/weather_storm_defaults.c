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
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("ZERO_STORM", NULL);
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
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("LOCK_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "LOCK_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("MISSING_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "MISSING_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("NETWORK_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "NETWORK_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("LOST_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "LOST_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("RANGE_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "RANGE_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("TYPE_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "TYPE_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("PARSE_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "PARSE_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("HANDLE_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "HANDLE_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("HARVEST_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "HARVEST_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("FALL_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "FALL_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("ANY_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("GHOST_STORM", NULL);
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t err = __ev;
    plant_iReport_print(get_report(), err);
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), "done");
    return 0;
}

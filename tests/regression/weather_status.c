#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("s0=", plant_map_to_string ( ffi_weather_status ( ) )));
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 0);
      if (setjmp(__w1.buf) == 0) {
    long p = ffi_arc_alloc ( 64 );
    plant_iReport_print(get_report(), _cat("reg=", ffi_weather_register ( p )));
    plant_iReport_print(get_report(), _cat("defer=", ffi_weather_defer ( p )));
    plant_iReport_print(get_report(), _cat("sin=", plant_map_to_string ( ffi_weather_status ( ) )));
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), _cat("s_deferred=", plant_map_to_string ( ffi_weather_status ( ) )));
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    {
      PlantWeather __w3 = {0};
      plant_weather_enter(&__w3, 0);
      if (setjmp(__w3.buf) == 0) {
    plant_throw("NETWORK_STORM", "inner unmatched");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w3);
        plant_weather_handling_begin(&__w3);
        plant_weather_handling_end(&__w3);
      }
      plant_calm(&__w3);
    }
    plant_iReport_print(get_report(), "BUG inner returned normally");
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
    plant_iReport_print(get_report(), _cat("outer caught: ", err));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "outer calm");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), _cat("final=", plant_map_to_string ( ffi_weather_status ( ) )));
  return main;
}

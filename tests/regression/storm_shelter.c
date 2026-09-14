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
    plant_throw_obj(plant_storm( "ARG_STORM" , "bad arg" , __FILE__, __LINE__, 0));
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ARG_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t e = __ev;
    plant_iReport_print(get_report(), _cat("caught=", _map_get ( e , "type" )));
    plant_iReport_print(get_report(), _cat("msg=", _map_get ( e , "message" )));
    plant_iReport_print(get_report(), _cat("file_len=", _from_long ( strlen( _map_get ( e , "file" ) ) )));
    plant_iReport_print(get_report(), _cat("line=", _map_get ( e , "line" )));
    plant_iReport_print(get_report(), _cat("col_absent=", _map_get ( e , "column" )));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    return 0;
}

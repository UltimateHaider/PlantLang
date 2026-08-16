#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t s1 = plant_map_to_string ( plant_storm( "ZERO_STORM" , "divide!" ) );
    plant_print(_cat("made=", s1));
    tx_t s2 = plant_map_to_string ( plant_storm( "ARG_STORM" , "" ) );
    plant_print(_cat("empty_msg=", s2));
    tx_t s3 = plant_map_to_string ( plant_storm( "" , "no type" ) );
    plant_print(_cat("no_type=", s3));
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
        plant_throw_obj(plant_storm( "ARG_STORM" , "bad arg value" ));
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
          plant_print(_cat("caught type=", _map_get ( e , "type" )));
          plant_print(_cat("caught msg=", _map_get ( e , "message" )));
          plant_print(_cat("ser=", plant_map_to_string ( e )));
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
        {
          PlantWeather __w3 = {0};
          plant_weather_enter(&__w3, 0);
          if (setjmp(__w3.buf) == 0) {
            plant_throw_obj(plant_storm( "CUSTOM" , "deep custom" ));
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
        plant_print("BUG inner escaped");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "ANY_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
          tx_t e2 = __ev;
          plant_print(_cat("outer type=", _map_get ( e2 , "type" )));
          plant_print(_cat("outer msg=", _map_get ( e2 , "message" )));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
      plant_calm(&__w1);
    }
    long i = 0;
    while (i < 5) {
        {
          PlantWeather __w3 = {0};
          plant_weather_enter(&__w3, 1);
          if (setjmp(__w3.buf) == 0) {
            plant_throw_obj(plant_storm( "RANGE_STORM" , "rapid" ));
          } else {
            const char* __et = plant_exc_type();
            const char* __em = plant_exc_msg();
            tx_t __ev = plant_exc_val();
            plant_weather_leave(&__w3);
            plant_weather_handling_begin(&__w3);
            if (plant_storm_match(__et, "RANGE_STORM")) {
              plant_weather_shelter_enter(&__w3);
              __w3.handled = 1;
              tx_t e3 = __ev;
              plant_print(_cat("rapid type=", _map_get ( e3 , "type" )));
              plant_print(_cat("rapid msg=", _map_get ( e3 , "message" )));
              plant_storm_release(__ev);
              plant_weather_shelter_leave(&__w3);
            }
            plant_weather_handling_end(&__w3);
          }
          plant_calm(&__w3);
        }
        i = i+1;
    }
    return 0;
}

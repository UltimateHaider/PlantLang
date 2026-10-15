/*
 * plant_geo.h — v0.54.0: PlantGeo type definition.
 *
 * A native geographic/geological value: a location (latitude/longitude,
 * degrees) plus elevation (meters). Heap-allocated PlantGeo* handle,
 * mirroring the PlantTensor composite-pointer family.
 */

#ifndef PLANT_GEO_H
#define PLANT_GEO_H

#include <stdint.h>

#define PLANT_GEO_MAGIC 0x47454F21ULL  /* "GEO!" */

typedef struct PlantGeo {
    uint64_t magic;   /* PLANT_GEO_MAGIC */
    double   lat;     /* latitude, degrees  (-90..90)   */
    double   lon;     /* longitude, degrees (-180..180) */
    double   elev;    /* elevation, meters              */
} PlantGeo;

/* Lifecycle */
PlantGeo* plant_geo_create(double lat, double lon, double elev);
void      plant_geo_free(PlantGeo* g);

/* Field accessors */
double    plant_geo_lat(PlantGeo* g);
double    plant_geo_lon(PlantGeo* g);
double    plant_geo_elev(PlantGeo* g);

/* Geodesic operations */
double    plant_geo_distance(PlantGeo* a, PlantGeo* b);  /* great-circle, meters */
double    plant_geo_bearing(PlantGeo* a, PlantGeo* b);   /* initial bearing, deg */

/* Display */
char*     plant_geo_to_string(PlantGeo* g);

#endif /* PLANT_GEO_H */

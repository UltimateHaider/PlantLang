/*
 * plant_geo.c — v0.54.0: PlantGeo runtime.
 *
 * All allocations use plant_malloc() (test-injectable failure
 * simulation), mirroring plant_tensor.c. C99.
 */

#include "plant_geo.h"
#include "plant_malloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define GEO_PI 3.14159265358979323846
#define GEO_R  6371000.0  /* mean Earth radius, meters */

static double _geo_deg2rad(double d) { return d * GEO_PI / 180.0; }

PlantGeo* plant_geo_create(double lat, double lon, double elev) {
    PlantGeo* g = (PlantGeo*)plant_malloc(sizeof(PlantGeo));
    if (!g) return NULL;
    g->magic = PLANT_GEO_MAGIC;
    g->lat = lat;
    g->lon = lon;
    g->elev = elev;
    return g;
}

void plant_geo_free(PlantGeo* g) {
    if (g) free(g);
}

double plant_geo_lat(PlantGeo* g)  { return g ? g->lat  : 0.0; }
double plant_geo_lon(PlantGeo* g)  { return g ? g->lon  : 0.0; }
double plant_geo_elev(PlantGeo* g) { return g ? g->elev : 0.0; }

double plant_geo_distance(PlantGeo* a, PlantGeo* b) {
    double p1, p2, dp, dl, h;
    if (!a || !b) return 0.0;
    p1 = _geo_deg2rad(a->lat);
    p2 = _geo_deg2rad(b->lat);
    dp = _geo_deg2rad(b->lat - a->lat);
    dl = _geo_deg2rad(b->lon - a->lon);
    h = sin(dp / 2.0) * sin(dp / 2.0) +
        cos(p1) * cos(p2) * sin(dl / 2.0) * sin(dl / 2.0);
    if (h > 1.0) h = 1.0;
    return 2.0 * GEO_R * asin(sqrt(h));  /* meters */
}

double plant_geo_bearing(PlantGeo* a, PlantGeo* b) {
    double p1, p2, dl, y, x, br;
    if (!a || !b) return 0.0;
    p1 = _geo_deg2rad(a->lat);
    p2 = _geo_deg2rad(b->lat);
    dl = _geo_deg2rad(b->lon - a->lon);
    y = sin(dl) * cos(p2);
    x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    br = atan2(y, x) * 180.0 / GEO_PI;
    return br < 0.0 ? br + 360.0 : br;
}

char* plant_geo_to_string(PlantGeo* g) {
    static char buf[128];
    if (!g) {
        snprintf(buf, sizeof(buf), "GEO(nil)");
        return buf;
    }
    snprintf(buf, sizeof(buf), "GEO(%.6f, %.6f, %.2f)",
             g->lat, g->lon, g->elev);
    return buf;
}

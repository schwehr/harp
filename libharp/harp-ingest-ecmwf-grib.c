/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "coda.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

/* The parameter id values and their link to GRIB1 table2Version/indicatorOfParameter and
 * GRIB2 discipline/parameterCategory/parameterNumber values are taken from
 * http://apps.ecmwf.int/codes/grib/param-db
 */
typedef enum grib_parameter_enum
{
    grib_param_unknown = -1,
    grib_param_tclw,    /* 78: Total column cloud liquid water [kg/m2] */
    grib_param_tciw,    /* 79: Total column cloud ice water [kg/m2] */
    grib_param_z,       /* 129: Geopotential [m2/s2] (at the surface: orography) */
    grib_param_t,       /* 130: Temperature [K] */
    grib_param_q,       /* 133: Specific humidity [kg/kg] */
    grib_param_tcwv,    /* 137: Total column water vapour [kg/m2] */
    grib_param_vo,      /* 138: Vorticity (relative) [1/s] */
    grib_param_lnsp,    /* 152: Logarithm of surface pressure [-] */
    grib_param_blh,     /* 159: Boundary layer height [m] */
    grib_param_tcc,     /* 164: Total cloud cover [-] */
    grib_param_10u,     /* 165: 10 metre U wind component [m/s2] */
    grib_param_10v,     /* 166: 10 metre V wind component [m/s2] */
    grib_param_2t,      /* 167: 2 meter temperature [K] */
    grib_param_lsm,     /* 172: Land-sea mask [(0-1)] */
    grib_param_clwc,    /* 246: Specific cloud liquid water content [kg/kg] */
    grib_param_ciwc,    /* 247: Specific cloud ice water content [kg/kg] */
    grib_param_co2,     /* 210061: Carbon Dioxide [kg/kg] */
    grib_param_ch4,     /* 210062/217004: Methane [kg/kg] */
    grib_param_pm1,     /* 210072: Particulate matter d < 1 um [kg/m3] */
    grib_param_pm2p5,   /* 210073: Particulate matter d < 2.5 um [kg/m3] */
    grib_param_pm10,    /* 210074: Particulate matter d < 10 um [kg/m3] */
    grib_param_no2,     /* 210121: Nitrogen dioxide [kg/kg] */
    grib_param_so2,     /* 210122: Sulphur dioxide [kg/kg] */
    grib_param_co,      /* 210123: Carbon monoxide [kg/kg] */
    grib_param_hcho,    /* 210124: Formaldehyde [kg/kg] */
    grib_param_tcno2,   /* 210125: Total column Nitrogen dioxide [kg/m2] */
    grib_param_tcso2,   /* 210126: Total column Sulphur dioxide [kg/m2] */
    grib_param_tcco,    /* 210127: Total column Carbon monoxide [kg/m2] */
    grib_param_tchcho,  /* 210128: Total column Formaldehyde [kg/m2] */
    grib_param_go3,     /* 210203: GEMS Ozone [kg/kg] */
    grib_param_gtco3,   /* 210206: GEMS Total column ozone [kg/m2] */
    grib_param_aod550,  /* 210207: Total Aerosol Optical Depth at 550nm [-] */
    grib_param_ssaod550,        /* 210208: Sea Salt Aerosol Optical Depth at 550nm [-] */
    grib_param_duaod550,        /* 210209: Dust Aerosol Optical Depth at 550nm [-] */
    grib_param_omaod550,        /* 210210: Organic Matter Aerosol Optical Depth at 550nm [-] */
    grib_param_bcaod550,        /* 210211: Block Carbon Aerosol Optical Depth at 550nm [-] */
    grib_param_suaod550,        /* 210212: Sulphate Aerosol Optical Depth at 550nm [-] */
    grib_param_aod469,  /* 210213: Total Aerosol Optical Depth at 469nm [-] */
    grib_param_aod670,  /* 210214: Total Aerosol Optical Depth at 670nm [-] */
    grib_param_aod865,  /* 210215: Total Aerosol Optical Depth at 865nm [-] */
    grib_param_aod1240, /* 210216: Total Aerosol Optical Depth at 1240nm [-] */
    grib_param_hno3,    /* 217006: Nitric acid [kg/kg] */
    grib_param_pan,     /* 217013: Peroxyacetyl nitrate [kg/kg] */
    grib_param_c5h8,    /* 217016: Isoprene [kg/kg] */
    grib_param_no,      /* 217027: Nitrogen monoxide [kg/kg] */
    grib_param_oh,      /* 217030: Hydroxyl radical [kg/kg] */
    grib_param_c2h6,    /* 217045: Ethane [kg/kg] */
    grib_param_c3h8,    /* 217047: Propane [kg/kg] */
    grib_param_tc_ch4,  /* 218004: Total column methane [kg/m2] */
    grib_param_tc_hno3, /* 218006: Total column nitric acid [kg/m2] */
    grib_param_tc_pan,  /* 218013: Total colunn peroxyacetyl nitrate [kg/m2] */
    grib_param_tc_c5h8, /* 218016: Total column isoprene [kg/m2] */
    grib_param_tc_no,   /* 218027: Total column nitrogen oxide [kg/m2] */
    grib_param_tc_oh,   /* 218030: Total column hydroxyl radical [kg/m2] */
    grib_param_tc_c2h6, /* 218045: Total column ethane [kg/m2] */
    grib_param_tc_c3h8  /* 218047: Total column propane [kg/m2] */
} grib_parameter;

#define NUM_GRIB_PARAMETERS (grib_param_tc_c3h8 + 1)

const char *param_name[NUM_GRIB_PARAMETERS] = {
    "tclw",
    "tciw",
    "z",
    "t",
    "q",
    "tcwv",
    "vo",
    "lnsp",
    "blh",
    "tcc",
    "10u",
    "10v",
    "2t",
    "lsm",
    "clwc",
    "ciwc",
    "co2",
    "ch4",
    "pm1",
    "pm2p5",
    "pm10",
    "no2",
    "so2",
    "co",
    "hcho",
    "tcno2",
    "tcso2",
    "tcco",
    "tchcho",
    "go3",
    "gtco3",
    "aod550",
    "ssaod550",
    "duaod550",
    "omaod550",
    "bcaod550",
    "suaod550",
    "aod469",
    "aod670",
    "aod865",
    "aod1240",
    "hno3",
    "pan",
    "c5h8",
    "no",
    "oh",
    "c2h6",
    "c3h8",
    "tc_ch4",
    "tc_hno3",
    "tc_pan",
    "tc_c5h8",
    "tc_no",
    "tc_oh",
    "tc_c2h6",
    "tc_c3h8"
};

int param_is_profile[NUM_GRIB_PARAMETERS] = {
    0,  /* tclw */
    0,  /* tciw */
    0,  /* z */
    1,  /* t */
    1,  /* q */
    0,  /* tcwv */
    1,  /* vo */
    0,  /* lnsp */
    0,  /* blh */
    0,  /* tcc */
    0,  /* 10u */
    0,  /* 10v */
    0,  /* 2t */
    0,  /* lsm */
    1,  /* clwc */
    1,  /* ciwc */
    1,  /* co2 */
    1,  /* ch4 */
    0,  /* pm1 */
    0,  /* pm2p5 */
    0,  /* pm10 */
    1,  /* no2 */
    1,  /* so2 */
    1,  /* co */
    1,  /* hcho */
    0,  /* tcno2 */
    0,  /* tcso2 */
    0,  /* tcco */
    0,  /* tchcho */
    1,  /* go3 */
    0,  /* gtco3 */
    0,  /* aod550 */
    0,  /* ssaod550 */
    0,  /* duaod550 */
    0,  /* omaod550 */
    0,  /* bcaod550 */
    0,  /* suaod550 */
    0,  /* aod469 */
    0,  /* aod670 */
    0,  /* aod865 */
    0,  /* aod1240 */
    1,  /* hno3 */
    1,  /* pan */
    1,  /* c5h8 */
    1,  /* no */
    1,  /* oh */
    1,  /* c2h6 */
    1,  /* c3h8 */
    0,  /* tc_ch4 */
    0,  /* tc_hno3 */
    0,  /* tc_pan */
    0,  /* tc_c5h8 */
    0,  /* tc_no */
    0,  /* tc_oh */
    0,  /* tc_c2h6 */
    0   /* tc_c3h8 */
};

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_messages;
    long num_grid_data;
    /* GRIB1 grid_data_parameter_ref = (1 * 256 * 256 + table2Version) * 256 + indicatorOfParameter
     * GRIB2 grid_data_parameter_ref = ((2 * 256 + discipline) * 256 + parameterCategory) * 256 + parameterNumber */
    long *grid_data_parameter_ref;      /* [num_grid_data] */
    coda_cursor *parameter_cursor;      /* [num_grid_data], array of cursors to /[]/data([])/values for each param */
    double *level;      /* [num_grid_data] */
    double wavelength;

    double datetime;
    double reference_datetime;
    int is_forecast_datetime;

    /* original grid definition */
    uint32_t Ni;        /* num_longitudes */
    uint32_t Nj;        /* num_latitudes */
    int32_t latitudeOfFirstGridPoint;
    int32_t longitudeOfFirstGridPoint;
    int32_t latitudeOfLastGridPoint;
    int32_t longitudeOfLastGridPoint;
    uint32_t iDirectionIncrement;
    uint32_t jDirectionIncrement;
    uint32_t N;
    int grid_grib_version;      /* GRIB version of message from which grid was taken */
    int is_gaussian;

    /* actual latitude/longitude axis values */
    long num_longitudes;
    double *longitude;  /* [num_longitudes] (stored in ascending order) */
    long num_latitudes;
    double *latitude;   /* [num_latitudes] (stored in descending order) */

    long num_levels;    /* max(1, num_grib_levels) */
    long num_grib_levels;       /* number of levels as reported in the GRIB file */
    double *coordinate_values;  /* [2 * (num_grib_levels + 1)], contains ap and bp coefficients */

    int has_parameter[NUM_GRIB_PARAMETERS];
    long *grid_data_index;      /* [NUM_GRIB_PARAMETERS, num_levels] */
} ingest_info;


/* The gaussian latitude calculation routines are taken from the grib_api software (Apache Licence Version 2.0) */

static void gauss_first_guess(long trunc, double *vals)
{
    long i = 0;

    double gvals[] = { 2.4048255577E0, 5.5200781103E0, 8.6537279129E0, 11.7915344391E0, 14.9309177086E0,
        18.0710639679E0, 21.2116366299E0, 24.3524715308E0, 27.4934791320E0, 30.6346064684E0, 33.7758202136E0,
        36.9170983537E0, 40.0584257646E0, 43.1997917132E0, 46.3411883717E0, 49.4826098974E0, 52.6240518411E0,
        55.7655107550E0, 58.9069839261E0, 62.0484691902E0, 65.1899648002E0, 68.3314693299E0, 71.4729816036E0,
        74.6145006437E0, 77.7560256304E0, 80.8975558711E0, 84.0390907769E0, 87.1806298436E0, 90.3221726372E0,
        93.4637187819E0, 96.6052679510E0, 99.7468198587E0, 102.8883742542E0, 106.0299309165E0, 109.1714896498E0,
        112.3130502805E0, 115.4546126537E0, 118.5961766309E0, 121.7377420880E0, 124.8793089132E0, 128.0208770059E0,
        131.1624462752E0, 134.3040166383E0, 137.4455880203E0, 140.5871603528E0, 143.7287335737E0, 146.8703076258E0,
        150.0118824570E0, 153.1534580192E0, 156.2950342685E0
    };

    for (i = 0; i < trunc; i++)
    {
        vals[i] = i < 50 ? gvals[i] : vals[i - 1] + M_PI;
    }
}


static int grib_get_gaussian_latitudes(long trunc, double *lats)
{
    long jlat, iter, legi;
    double rad2deg, convval, root, legfonc = 0;
    double mem1, mem2, conv;
    double precision = 1.0E-14;
    long nlat = trunc * 2;

    rad2deg = 180.0 / M_PI;
    convval = (1.0 - ((2.0 / M_PI) * (2.0 / M_PI)) * 0.25);

    gauss_first_guess(trunc, lats);

    for (jlat = 0; jlat < trunc; jlat++)
    {
        /* First approximation for root */
        root = cos(lats[jlat] / sqrt(((((double)nlat) + 0.5) * (((double)nlat) + 0.5)) + convval));

        /* Perform loop of Newton iterations */
        iter = 0;
        conv = 1;
        while (fabs(conv) >= precision)
        {
            mem2 = 1.0;
            mem1 = root;

            /* Compute Legendre polynomial */
            for (legi = 0; legi < nlat; legi++)
            {
                legfonc = ((2.0 * (legi + 1) - 1.0) * root * mem1 - legi * mem2) / ((double)(legi + 1));
                mem2 = mem1;
                mem1 = legfonc;
            }

            /* Perform Newton iteration */
            conv = legfonc / ((((double)nlat) * (mem2 - root * legfonc)) / (1.0 - (root * root)));
            root -= conv;

            /* Routine fails if no convergence after JPMAXITER iterations. */
            if (iter++ > 10)
            {
                return -1;
            }
        }

        /* Set North and South values using symmetry. */
        lats[jlat] = -asin(root) * rad2deg;
        lats[nlat - 1 - jlat] = -lats[jlat];
    }

    if (nlat != (trunc * 2))
    {
        lats[trunc + 1] = 0.0;
    }

    return 0;
}

static grib_parameter get_grib1_parameter(int parameter_ref)
{
    uint8_t table2Version = (parameter_ref >> 8) & 0xff;
    uint8_t indicatorOfParameter = parameter_ref & 0xff;

    switch (table2Version)
    {
        case 128:
            switch (indicatorOfParameter)
            {
                case 78:
                    return grib_param_tclw;
                case 79:
                    return grib_param_tciw;
                case 129:
                    return grib_param_z;
                case 130:
                    return grib_param_t;
                case 133:
                    return grib_param_q;
                case 137:
                    return grib_param_tcwv;
                case 138:
                    return grib_param_vo;
                case 152:
                    return grib_param_lnsp;
                case 159:
                    return grib_param_blh;
                case 164:
                    return grib_param_tcc;
                case 165:
                    return grib_param_10u;
                case 166:
                    return grib_param_10v;
                case 167:
                    return grib_param_2t;
                case 172:
                    return grib_param_lsm;
                case 246:
                    return grib_param_clwc;
                case 247:
                    return grib_param_ciwc;
            }
            break;
        case 160:
            switch (indicatorOfParameter)
            {
                case 129:
                    return grib_param_z;
                case 130:
                    return grib_param_t;
                case 133:
                    return grib_param_q;
                case 138:
                    return grib_param_vo;
                case 152:
                    return grib_param_lnsp;
                case 164:
                    return grib_param_tcc;
                case 165:
                    return grib_param_10u;
                case 166:
                    return grib_param_10v;
                case 167:
                    return grib_param_2t;
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 170:
            switch (indicatorOfParameter)
            {
                case 129:
                    return grib_param_z;
                case 130:
                    return grib_param_t;
                case 133:
                    return grib_param_q;
                case 138:
                    return grib_param_vo;
                case 164:
                    return grib_param_tcc;
            }
            break;
        case 171:
            switch (indicatorOfParameter)
            {
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 174:
            switch (indicatorOfParameter)
            {
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 175:
            switch (indicatorOfParameter)
            {
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 180:
            switch (indicatorOfParameter)
            {
                case 129:
                    return grib_param_z;
                case 130:
                    return grib_param_t;
                case 133:
                    return grib_param_q;
                case 137:
                    return grib_param_tcwv;
                case 138:
                    return grib_param_vo;
                case 164:
                    return grib_param_tcc;
                case 165:
                    return grib_param_10u;
                case 166:
                    return grib_param_10v;
                case 167:
                    return grib_param_2t;
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 190:
            switch (indicatorOfParameter)
            {
                case 129:
                    return grib_param_z;
                case 130:
                    return grib_param_t;
                case 133:
                    return grib_param_q;
                case 138:
                    return grib_param_vo;
                case 164:
                    return grib_param_tcc;
                case 165:
                    return grib_param_10u;
                case 166:
                    return grib_param_10v;
                case 167:
                    return grib_param_2t;
                case 172:
                    return grib_param_lsm;
            }
            break;
        case 210:
            switch (indicatorOfParameter)
            {
                case 61:
                    return grib_param_co2;
                case 62:
                    return grib_param_ch4;
                case 72:
                    return grib_param_pm1;
                case 73:
                    return grib_param_pm2p5;
                case 74:
                    return grib_param_pm10;
                case 121:
                    return grib_param_no2;
                case 122:
                    return grib_param_so2;
                case 123:
                    return grib_param_co;
                case 124:
                    return grib_param_hcho;
                case 125:
                    return grib_param_tcno2;
                case 126:
                    return grib_param_tcso2;
                case 127:
                    return grib_param_tcco;
                case 128:
                    return grib_param_tchcho;
                case 203:
                    return grib_param_go3;
                case 206:
                    return grib_param_gtco3;
                case 207:
                    return grib_param_aod550;
                case 208:
                    return grib_param_ssaod550;
                case 209:
                    return grib_param_duaod550;
                case 210:
                    return grib_param_omaod550;
                case 211:
                    return grib_param_bcaod550;
                case 212:
                    return grib_param_suaod550;
                case 213:
                    return grib_param_aod469;
                case 214:
                    return grib_param_aod670;
                case 215:
                    return grib_param_aod865;
                case 216:
                    return grib_param_aod1240;
            }
            break;
        case 217:
            switch (indicatorOfParameter)
            {
                case 4:
                    return grib_param_ch4;
                case 6:
                    return grib_param_hno3;
                case 13:
                    return grib_param_pan;
                case 16:
                    return grib_param_c5h8;
                case 27:
                    return grib_param_no;
                case 30:
                    return grib_param_oh;
                case 45:
                    return grib_param_c2h6;
                case 47:
                    return grib_param_c3h8;
            }
            break;
        case 218:
            switch (indicatorOfParameter)
            {
                case 4:
                    return grib_param_tc_ch4;
                case 6:
                    return grib_param_tc_hno3;
                case 13:
                    return grib_param_tc_pan;
                case 16:
                    return grib_param_tc_c5h8;
                case 27:
                    return grib_param_tc_no;
                case 30:
                    return grib_param_tc_oh;
                case 45:
                    return grib_param_tc_c2h6;
                case 47:
                    return grib_param_tc_c3h8;
            }
            break;
    }

    return grib_param_unknown;
}

static grib_parameter get_grib2_parameter(int parameter_ref)
{
    uint8_t discipline = (parameter_ref >> 16) & 0xff;
    uint8_t parameterCategory = (parameter_ref >> 8) & 0xff;
    uint8_t parameterNumber = parameter_ref & 0xff;

    switch (discipline)
    {
        case 0:
            switch (parameterCategory)
            {
                case 0:
                    switch (parameterNumber)
                    {
                        case 0:
                            return grib_param_t;
                    }
                    break;
                case 1:
                    switch (parameterNumber)
                    {
                        case 0:
                            return grib_param_q;
                        case 83:
                            return grib_param_clwc;
                        case 84:
                            return grib_param_ciwc;
                    }
                    break;
                case 2:
                    switch (parameterNumber)
                    {
                        case 2:
                            return grib_param_10u;
                        case 3:
                            return grib_param_10v;
                        case 12:
                            return grib_param_vo;
                    }
                    break;
                case 3:
                    switch (parameterNumber)
                    {
                        case 4:
                            return grib_param_z;
                        case 25:
                            return grib_param_lnsp;
                    }
                    break;
            }
            break;
        case 2:
            switch (parameterCategory)
            {
                case 0:
                    switch (parameterNumber)
                    {
                        case 0:
                            return grib_param_lsm;
                    }
                    break;
            }
            break;
        case 192:
            switch (parameterCategory)
            {
                case 128:
                    switch (parameterNumber)
                    {
                        case 78:
                            return grib_param_tclw;
                        case 79:
                            return grib_param_tciw;
                        case 137:
                            return grib_param_tcwv;
                        case 159:
                            return grib_param_blh;
                        case 164:
                            return grib_param_tcc;
                    }
                    break;
                case 210:
                    switch (parameterNumber)
                    {
                        case 61:
                            return grib_param_co2;
                        case 62:
                            return grib_param_ch4;
                        case 72:
                            return grib_param_pm1;
                        case 73:
                            return grib_param_pm2p5;
                        case 74:
                            return grib_param_pm10;
                        case 121:
                            return grib_param_no2;
                        case 122:
                            return grib_param_so2;
                        case 123:
                            return grib_param_co;
                        case 124:
                            return grib_param_hcho;
                        case 125:
                            return grib_param_tcno2;
                        case 126:
                            return grib_param_tcso2;
                        case 127:
                            return grib_param_tcco;
                        case 128:
                            return grib_param_tchcho;
                        case 203:
                            return grib_param_go3;
                        case 206:
                            return grib_param_gtco3;
                        case 207:
                            return grib_param_aod550;
                        case 208:
                            return grib_param_ssaod550;
                        case 209:
                            return grib_param_duaod550;
                        case 210:
                            return grib_param_omaod550;
                        case 211:
                            return grib_param_bcaod550;
                        case 212:
                            return grib_param_suaod550;
                        case 213:
                            return grib_param_aod469;
                        case 214:
                            return grib_param_aod670;
                        case 215:
                            return grib_param_aod865;
                        case 216:
                            return grib_param_aod1240;
                    }
                    break;
                case 217:
                    switch (parameterNumber)
                    {
                        case 4:
                            return grib_param_ch4;
                        case 6:
                            return grib_param_hno3;
                        case 13:
                            return grib_param_pan;
                        case 16:
                            return grib_param_c5h8;
                        case 27:
                            return grib_param_no;
                        case 30:
                            return grib_param_oh;
                        case 45:
                            return grib_param_c2h6;
                        case 47:
                            return grib_param_c3h8;
                    }
                    break;
                case 218:
                    switch (parameterNumber)
                    {
                        case 4:
                            return grib_param_tc_ch4;
                        case 6:
                            return grib_param_tc_hno3;
                        case 13:
                            return grib_param_tc_pan;
                        case 16:
                            return grib_param_tc_c5h8;
                        case 27:
                            return grib_param_tc_no;
                        case 30:
                            return grib_param_tc_oh;
                        case 45:
                            return grib_param_tc_c2h6;
                        case 47:
                            return grib_param_tc_c3h8;
                    }
                    break;
            }
            break;
    }

    return grib_param_unknown;
}

static grib_parameter get_grib_parameter(int parameter_ref)
{
    if (parameter_ref >> 24 == 1)
    {
        return get_grib1_parameter(parameter_ref & 0xffffff);
    }
    return get_grib2_parameter(parameter_ref & 0xffffff);
}

static int read_grid_data(ingest_info *info, long grid_data_index, long latitude_index, harp_array data)
{
    /* flip latitude dimension, so it becomes ascending */
    latitude_index = info->num_latitudes - latitude_index - 1;

    assert(grid_data_index >= 0);
    return coda_cursor_read_float_partial_array(&info->parameter_cursor[grid_data_index],
                                                latitude_index * info->num_longitudes, info->num_longitudes,
                                                data.float_data);
}

static int read_2d_grid_data(ingest_info *info, grib_parameter parameter, long index, harp_array data)
{
    assert(info->has_parameter[parameter]);
    return read_grid_data(info, info->grid_data_index[parameter * info->num_levels], index, data);
}

static int read_3d_grid_data(ingest_info *info, grib_parameter parameter, long index, harp_array data)
{
    long dimension_transpose[2];
    long i;

    assert(info->has_parameter[parameter]);

    dimension_transpose[0] = info->num_levels;
    dimension_transpose[1] = info->num_longitudes;

    /* we read the data as [vertical,longitude] */
    for (i = 0; i < info->num_levels; i++)
    {
        harp_array subgrid;

        subgrid.float_data = &data.float_data[i * info->num_longitudes];
        /* invert the loop because level 0 = TOA */
        if (read_grid_data(info, info->grid_data_index[(parameter + 1) * info->num_levels - 1 - i], index, subgrid) !=
            0)
        {
            return -1;
        }
    }
    /* and then reorder dimensions from [vertical,longitude] to [longitude,vertical] */
    if (harp_array_transpose(harp_type_float, 2, dimension_transpose, NULL, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_longitude] = info->num_longitudes;
    dimension[harp_dimension_latitude] = info->num_latitudes;
    dimension[harp_dimension_vertical] = info->num_levels;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    *data.double_data = info->datetime;

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    *data.double_data = ((ingest_info *)user_data)->latitude[index];
    return 0;
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    *data.double_data = ((ingest_info *)user_data)->longitude[index];
    return 0;
}

static int read_wavelength(void *user_data, harp_array data)
{
    *data.double_data = ((ingest_info *)user_data)->wavelength;
    return 0;
}

static int read_tclw(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tclw, index, data);
}

static int read_tciw(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tciw, index, data);
}

static int read_z(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_z, index, data);
}

static int read_t(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_t, index, data);
}

static int read_q(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_q, index, data);
}

static int read_tcwv(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tcwv, index, data);
}

static int read_vo(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_vo, index, data);
}

static int read_lnsp(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_2d_grid_data(info, grib_param_lnsp, index, data) != 0)
    {
        return -1;
    }

    /* turn lognormal surface pressure (Pa) into surface pressure values (Pa) */
    for (i = 0; i < info->num_longitudes; i++)
    {
        data.float_data[i] = expf(data.float_data[i]);
    }

    return 0;
}

static int read_pressure(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *ap = info->coordinate_values;
    double *bp = &info->coordinate_values[info->num_levels + 1];
    long i, k;

    if (read_lnsp(user_data, index, data) != 0)
    {
        return -1;
    }

    for (k = info->num_longitudes - 1; k >= 0; k--)
    {
        double surface_pressure = data.float_data[k];

        for (i = 0; i < info->num_levels; i++)
        {
            /* invert the loop because level 0 = TOA */
            data.float_data[((k + 1) * info->num_levels - 1 - i)] =
                (float)(0.5 * (ap[i] + ap[i + 1] + (bp[i] + bp[i + 1]) * surface_pressure));
        }
    }

    return 0;
}

static int read_pressure_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *ap = info->coordinate_values;
    double *bp = &info->coordinate_values[info->num_levels + 1];
    long i, k;

    if (read_lnsp(user_data, index, data) != 0)
    {
        return -1;
    }

    for (k = info->num_longitudes - 1; k >= 0; k--)
    {
        double surface_pressure = data.float_data[k];

        for (i = 0; i < info->num_levels; i++)
        {
            /* invert the loop because level 0 = TOA */
            data.float_data[2 * ((k + 1) * info->num_levels - 1 - i)] =
                (float)(ap[i + 1] + bp[i + 1] * surface_pressure);
            data.float_data[2 * ((k + 1) * info->num_levels - 1 - i) + 1] = (float)(ap[i] + bp[i] * surface_pressure);
        }
    }

    return 0;
}

static int read_blh(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_blh, index, data);
}

static int read_tcc(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tcc, index, data);
}

static int read_10u(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_10u, index, data);
}

static int read_10v(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_10v, index, data);
}

static int read_2t(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_2t, index, data);
}

static int read_lsm(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_lsm, index, data);
}

static int read_clwc(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_clwc, index, data);
}

static int read_ciwc(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_ciwc, index, data);
}

static int read_co2(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_co2, index, data);
}

static int read_ch4(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_ch4, index, data);
}

static int read_pm1(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_pm1, index, data);
}

static int read_pm2p5(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_pm2p5, index, data);
}

static int read_pm10(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_pm10, index, data);
}

static int read_no2(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_no2, index, data);
}

static int read_so2(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_so2, index, data);
}

static int read_co(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_co, index, data);
}

static int read_hcho(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_hcho, index, data);
}

static int read_tcno2(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tcno2, index, data);
}

static int read_tcso2(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tcso2, index, data);
}

static int read_tcco(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tcco, index, data);
}

static int read_tchcho(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tchcho, index, data);
}

static int read_go3(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_go3, index, data);
}

static int read_gtco3(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_gtco3, index, data);
}

static int read_aod(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->has_parameter[grib_param_aod550])
    {
        return read_2d_grid_data((ingest_info *)user_data, grib_param_aod550, index, data);
    }
    if (info->has_parameter[grib_param_aod469])
    {
        return read_2d_grid_data((ingest_info *)user_data, grib_param_aod469, index, data);
    }
    if (info->has_parameter[grib_param_aod670])
    {
        return read_2d_grid_data((ingest_info *)user_data, grib_param_aod670, index, data);
    }
    if (info->has_parameter[grib_param_aod865])
    {
        return read_2d_grid_data((ingest_info *)user_data, grib_param_aod865, index, data);
    }
    if (info->has_parameter[grib_param_aod1240])
    {
        return read_2d_grid_data((ingest_info *)user_data, grib_param_aod1240, index, data);
    }

    assert(0);
    exit(1);
}

static int read_ssaod(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_ssaod550, index, data);
}

static int read_duaod(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_duaod550, index, data);
}

static int read_omaod(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_omaod550, index, data);
}

static int read_bcaod(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_bcaod550, index, data);
}

static int read_suaod(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_suaod550, index, data);
}

static int read_hno3(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_hno3, index, data);
}

static int read_pan(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_pan, index, data);
}

static int read_c5h8(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_c5h8, index, data);
}

static int read_no(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_no, index, data);
}

static int read_oh(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_oh, index, data);
}

static int read_c2h6(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_c2h6, index, data);
}

static int read_c3h8(void *user_data, long index, harp_array data)
{
    return read_3d_grid_data((ingest_info *)user_data, grib_param_c3h8, index, data);
}

static int read_tc_ch4(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_ch4, index, data);
}

static int read_tc_hno3(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_hno3, index, data);
}

static int read_tc_pan(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_pan, index, data);
}

static int read_tc_c5h8(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_c5h8, index, data);
}

static int read_tc_no(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_no, index, data);
}

static int read_tc_oh(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_oh, index, data);
}

static int read_tc_c2h6(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_c2h6, index, data);
}

static int read_tc_c3h8(void *user_data, long index, harp_array data)
{
    return read_2d_grid_data((ingest_info *)user_data, grib_param_tc_c3h8, index, data);
}

static int get_datetime(coda_cursor *cursor, ingest_info *info, double *datetime)
{
    uint8_t unit_indicator;
    uint32_t forecast_time;
    double scalefactor;

    /* this function is only used for GRIB2 messages */
    if (coda_cursor_goto_record_field_by_name(cursor, "indicatorOfUnitOfTimeRange") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &unit_indicator) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    switch (unit_indicator)
    {
        case 0:        /* minute */
            scalefactor = 60;
            break;
        case 1:        /* hour */
            scalefactor = 60 * 60;
            break;
        case 2:        /* day */
            scalefactor = 24 * 60 * 60;
            break;
        case 10:       /* 3 hours */
            scalefactor = 3 * 60 * 60;
            break;
        case 11:       /* 6 hours */
            scalefactor = 6 * 60 * 60;
            break;
        case 12:       /* 12 hours */
            scalefactor = 12 * 60 * 60;
            break;
        case 13:       /* second */
            scalefactor = 1;
            break;
        default:
            harp_set_error(HARP_ERROR_INGESTION, "unsupported indicatorOfUnitOfTimeRange value (%d)", unit_indicator);
            return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32(cursor, &forecast_time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    *datetime = info->reference_datetime + scalefactor * forecast_time;

    return 0;
}

static int get_reference_datetime(coda_cursor *cursor, int grib_version, ingest_info *info)
{
    uint8_t significanceOfReferenceTime;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second = 0;

    if (grib_version == 1)
    {
        uint8_t centuryOfReferenceTimeOfData;
        uint8_t yearOfCentury;

        if (coda_cursor_goto_record_field_by_name(cursor, "centuryOfReferenceTimeOfData") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &centuryOfReferenceTimeOfData) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(cursor);
        if (coda_cursor_goto_record_field_by_name(cursor, "yearOfCentury") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &yearOfCentury) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (yearOfCentury > 0)
        {
            /* The 20th century ends at 1 Jan 2001, so (20,00) -> 2000 and (21,01) -> 2001 */
            centuryOfReferenceTimeOfData -= 1;
        }
        year = 100 * centuryOfReferenceTimeOfData + yearOfCentury;
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(cursor, "significanceOfReferenceTime") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &significanceOfReferenceTime) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->is_forecast_datetime = 0;
        if (significanceOfReferenceTime == 1 || significanceOfReferenceTime == 2)
        {
            info->is_forecast_datetime = 1;
        }
        else if (!(significanceOfReferenceTime == 0 || significanceOfReferenceTime == 3))
        {
            harp_set_error(HARP_ERROR_INGESTION, "unsupported significanceOfReferenceTime value (%d)",
                           significanceOfReferenceTime);
            return -1;
        }

        if (coda_cursor_goto_next_record_field(cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint16(cursor, &year) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &month) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &day) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &hour) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &minute) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (grib_version == 2)
    {
        if (coda_cursor_goto_next_record_field(cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &second) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    coda_cursor_goto_parent(cursor);

    if (coda_datetime_to_double(year, month, day, hour, minute, second, 0, &info->reference_datetime) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (grib_version == 1)
    {
        uint8_t unitOfTimeRange;
        uint8_t P1;

        if (coda_cursor_goto_record_field_by_name(cursor, "unitOfTimeRange") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &unitOfTimeRange) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_next_record_field(cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &P1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(cursor);

        if (unitOfTimeRange != 0)
        {
            double scalefactor;

            info->is_forecast_datetime = 1;
            switch (unitOfTimeRange)
            {
                case 1:        /* hour */
                    scalefactor = 60 * 60;
                    break;
                case 2:        /* day */
                    scalefactor = 24 * 60 * 60;
                    break;
                case 10:       /* 3 hours */
                    scalefactor = 3 * 60 * 60;
                    break;
                case 11:       /* 6 hours */
                    scalefactor = 6 * 60 * 60;
                    break;
                case 12:       /* 12 hours */
                    scalefactor = 12 * 60 * 60;
                    break;
                case 13:       /* second */
                    scalefactor = 1;
                    break;
                default:
                    harp_set_error(HARP_ERROR_INGESTION, "unsupported unitOfTimeRange value (%d)", unitOfTimeRange);
                    return -1;
            }
            info->reference_datetime += scalefactor * P1;
        }
    }

    return 0;
}

static int get_num_grid_data(coda_cursor *cursor, ingest_info *info)
{
    long field_index;
    long i;

    if (coda_cursor_get_num_elements(cursor, &info->num_messages) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->num_grid_data = 0;
    if (coda_cursor_goto_first_array_element(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_messages; i++)
    {
        if (coda_cursor_get_available_union_field_index(cursor, &field_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (field_index == 0)
        {
            /* GRIB1 */
            info->num_grid_data++;
        }
        else
        {
            long num_data;

            /* GRIB2 */
            if (coda_cursor_goto_record_field_by_index(cursor, field_index) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_record_field_by_name(cursor, "data") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_get_num_elements(cursor, &num_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(cursor);
            if (num_data == 0)
            {
                harp_set_error(HARP_ERROR_INGESTION, "missing data section for GRIB2 message %ld", i);
                return -1;
            }
            info->num_grid_data += num_data;
            coda_cursor_goto_parent(cursor);
        }

        if (i < info->num_messages - 1)
        {
            if (coda_cursor_goto_next_array_element(cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    coda_cursor_goto_parent(cursor);

    return 0;
}

static int get_lat_lon_grid(coda_cursor *cursor, int grib_version, ingest_info *info, int first)
{
    uint32_t Ni = 0;
    uint32_t Nj = 0;
    int32_t latitudeOfFirstGridPoint = 0;
    int32_t longitudeOfFirstGridPoint = 0;
    int32_t latitudeOfLastGridPoint = 0;
    int32_t longitudeOfLastGridPoint = 0;
    uint32_t iDirectionIncrement = 0;
    uint32_t jDirectionIncrement = 0;
    uint32_t N = 0;
    uint8_t scanningMode = 0;
    int is_gaussian = 0;

    if (grib_version == 1)
    {
        uint8_t dataRepresentationType;

        if (coda_cursor_goto_record_field_by_name(cursor, "dataRepresentationType") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(cursor, &dataRepresentationType) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* supported dataRepresentationType values
         * 0: latitude/longitude grid (equidistant cylindrical or Plate Carree projection)
         * 4: Gaussian latitude/longitude grid
         */
        if (dataRepresentationType != 0 && dataRepresentationType != 4)
        {
            harp_set_error(HARP_ERROR_INGESTION, "unsupported GRIB1 grid definition (%d)", (int)dataRepresentationType);
            return -1;
        }
        is_gaussian = (dataRepresentationType == 4);
        coda_cursor_goto_parent(cursor);
    }
    else
    {
        uint16_t gridDefinitionTemplateNumber;

        if (coda_cursor_goto_record_field_by_name(cursor, "gridDefinitionTemplateNumber") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint16(cursor, &gridDefinitionTemplateNumber) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* supported gridDefinitionTemplateNumber values
         * 0: latitude/longitude grid (equidistant cylindrical or Plate Carree projection)
         * 40: Gaussian latitude/longitude grid
         */
        if (gridDefinitionTemplateNumber != 0 && gridDefinitionTemplateNumber != 40)
        {
            harp_set_error(HARP_ERROR_INGESTION, "unsupported GRIB2 grid definition (%d)",
                           (int)gridDefinitionTemplateNumber);
            return -1;
        }
        is_gaussian = (gridDefinitionTemplateNumber == 40);
        coda_cursor_goto_parent(cursor);
    }
    if (coda_cursor_goto_record_field_by_name(cursor, "Ni") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32(cursor, &Ni) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32(cursor, &Nj) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    if ((grib_version == 1 && (Ni == 65535 || Nj == 65535)) ||
        (grib_version == 2 && (Ni == 4294967295 || Nj == 4294967295)))
    {
        harp_set_error(HARP_ERROR_INGESTION, "reduced Gaussian grids are not supported");
        return -1;
    }

    if (coda_cursor_goto_record_field_by_name(cursor, "latitudeOfFirstGridPoint") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(cursor, &latitudeOfFirstGridPoint) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(cursor, &longitudeOfFirstGridPoint) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);

    if (coda_cursor_goto_record_field_by_name(cursor, "latitudeOfLastGridPoint") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(cursor, &latitudeOfLastGridPoint) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(cursor, &longitudeOfLastGridPoint) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32(cursor, &iDirectionIncrement) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    if (is_gaussian)
    {
        if (coda_cursor_goto_record_field_by_name(cursor, "N") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint32(cursor, &N) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(cursor, "jDirectionIncrement") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint32(cursor, &jDirectionIncrement) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    coda_cursor_goto_parent(cursor);
    if (coda_cursor_goto_record_field_by_name(cursor, "scanningMode") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(cursor, &scanningMode) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    if (scanningMode != 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "unsupported scanningMode (%d) for grid", scanningMode);
        return -1;
    }

    if (grib_version == 1)
    {
        /* multiply values by 1e3 to align them with GRIB2 values */
        latitudeOfFirstGridPoint *= 1000;
        longitudeOfFirstGridPoint *= 1000;
        latitudeOfLastGridPoint *= 1000;
        longitudeOfLastGridPoint *= 1000;
        iDirectionIncrement *= 1000;
        jDirectionIncrement *= 1000;
    }

    if (first)
    {
        int k;

        info->Ni = Ni;
        info->Nj = Nj;
        info->latitudeOfFirstGridPoint = latitudeOfFirstGridPoint;
        info->longitudeOfFirstGridPoint = longitudeOfFirstGridPoint;
        info->latitudeOfLastGridPoint = latitudeOfLastGridPoint;
        info->longitudeOfLastGridPoint = longitudeOfLastGridPoint;
        info->iDirectionIncrement = iDirectionIncrement;
        info->jDirectionIncrement = jDirectionIncrement;
        info->N = N;
        info->is_gaussian = is_gaussian;
        info->grid_grib_version = grib_version;
        info->num_longitudes = Ni;
        info->num_latitudes = Nj;

        info->longitude = malloc(info->num_longitudes * sizeof(double));
        if (info->longitude == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           info->num_longitudes * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        info->latitude = malloc(info->num_latitudes * sizeof(double));
        if (info->latitude == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           info->num_latitudes * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        if (longitudeOfFirstGridPoint > longitudeOfLastGridPoint)
        {
            harp_set_error(HARP_ERROR_INGESTION, "longitude grid is not in ascending order");
            return -1;
        }
        if (latitudeOfFirstGridPoint < latitudeOfLastGridPoint)
        {
            harp_set_error(HARP_ERROR_INGESTION, "latitude grid is not in descending order");
            return -1;
        }
        info->longitude[0] = longitudeOfFirstGridPoint * 1e-6;
        info->longitude[info->num_longitudes - 1] = longitudeOfLastGridPoint * 1e-6;
        for (k = 1; k < info->num_longitudes - 1; k++)
        {
            info->longitude[k] = info->longitude[k - 1] + iDirectionIncrement * 1e-6;
        }
        if (is_gaussian)
        {
            if (N != info->num_latitudes / 2)
            {
                harp_set_error(HARP_ERROR_INGESTION, "invalid value for N for Gaussian grid");
                return -1;
            }
            if (grib_get_gaussian_latitudes(N, info->latitude) != 0)
            {
                return -1;
            }
        }
        else
        {
            info->latitude[0] = latitudeOfLastGridPoint * 1e-6;
            info->latitude[info->num_latitudes - 1] = latitudeOfFirstGridPoint * 1e-6;
            for (k = info->num_latitudes - 2; k > 0; k--)
            {
                info->latitude[k] = info->latitude[k + 1] - iDirectionIncrement * 1e-6;
            }
        }
    }
    else
    {
        if (Ni != info->Ni || Nj != info->Nj)
        {
            harp_set_error(HARP_ERROR_INGESTION, "not all lat/lon grids in the GRIB file have the same size");
            return -1;
        }
        if (is_gaussian != info->is_gaussian)
        {
            harp_set_error(HARP_ERROR_INGESTION, "not all lat/lon grids in the GRIB file use the same grid type");
            return -1;
        }
        if (info->grid_grib_version != grib_version)
        {
            /* since GRIB1 and GRIB2 use different resolutions we need to compare with a tollerance of 1000 */
            if (fabs((double)(longitudeOfFirstGridPoint - info->longitudeOfFirstGridPoint)) > 1e3 ||
                fabs((double)(longitudeOfLastGridPoint - info->longitudeOfLastGridPoint)) > 1e3 ||
                fabs((double)(iDirectionIncrement - info->iDirectionIncrement)) > 1e3)
            {
                harp_set_error(HARP_ERROR_INGESTION, "not all longitude grids in the GRIB file are the same");
                return -1;
            }
            if (fabs((double)(latitudeOfFirstGridPoint - info->latitudeOfFirstGridPoint)) > 1e3 ||
                fabs((double)(latitudeOfLastGridPoint - info->latitudeOfLastGridPoint)) > 1e3 ||
                fabs((double)(jDirectionIncrement - info->jDirectionIncrement)) > 1e3 || N != info->N)
            {
                harp_set_error(HARP_ERROR_INGESTION, "not all latitude grids in the GRIB file are the same");
                return -1;
            }
            if (grib_version == 2)
            {
                /* prefer more accurate GRIB2 grid over that of less accurate GRIB1 grid */
                info->latitudeOfFirstGridPoint = latitudeOfFirstGridPoint;
                info->longitudeOfFirstGridPoint = longitudeOfFirstGridPoint;
                info->latitudeOfLastGridPoint = latitudeOfLastGridPoint;
                info->longitudeOfLastGridPoint = longitudeOfLastGridPoint;
                info->iDirectionIncrement = iDirectionIncrement;
                info->jDirectionIncrement = jDirectionIncrement;
                info->grid_grib_version = grib_version;
            }
        }
        else
        {
            if (longitudeOfFirstGridPoint != info->longitudeOfFirstGridPoint ||
                longitudeOfLastGridPoint != info->longitudeOfLastGridPoint ||
                iDirectionIncrement != info->iDirectionIncrement)
            {
                harp_set_error(HARP_ERROR_INGESTION, "not all longitude grids in the GRIB file are the same");
                return -1;
            }
            if (latitudeOfFirstGridPoint != info->latitudeOfFirstGridPoint ||
                latitudeOfLastGridPoint != info->latitudeOfLastGridPoint ||
                jDirectionIncrement != info->jDirectionIncrement || N != info->N)
            {
                harp_set_error(HARP_ERROR_INGESTION, "not all latitude grids in the GRIB file are the same");
                return -1;
            }
        }
    }

    return 0;
}

static int init_cursors_and_grid(ingest_info *info)
{
    coda_cursor cursor;
    int datetime_initialised = 0;
    long parameter_index = 0;
    long i, j;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (get_num_grid_data(&cursor, info) != 0)
    {
        return -1;
    }

    info->grid_data_parameter_ref = malloc(info->num_grid_data * sizeof(long));
    if (info->grid_data_parameter_ref == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_grid_data * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    info->parameter_cursor = malloc(info->num_grid_data * sizeof(coda_cursor));
    if (info->parameter_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_grid_data * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    info->level = malloc(info->num_grid_data * sizeof(double));
    if (info->level == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_grid_data * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_messages; i++)
    {
        long parameter_ref;
        int grib_version;
        long field_index;
        long num_data = 1;

        if (coda_cursor_get_available_union_field_index(&cursor, &field_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        switch (field_index)
        {
            case 0:
                grib_version = 1;
                break;
            case 1:
                grib_version = 2;
                break;
            default:
                assert(0);
                exit(1);
        }
        parameter_ref = grib_version << 24;
        if (coda_cursor_goto_record_field_by_index(&cursor, field_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (grib_version == 2)
        {
            if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_get_num_elements(&cursor, &num_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
        }

        if (get_reference_datetime(&cursor, grib_version, info) != 0)
        {
            return -1;
        }

        if (grib_version == 2)
        {
            uint8_t discipline = 0;

            if (coda_cursor_goto(&cursor, "discipline") != 0)
            {
                harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8(&cursor, &discipline) != 0)
            {
                harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            parameter_ref += discipline * 256 * 256;
        }

        if (coda_cursor_goto_record_field_by_name(&cursor, "grid") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (grib_version == 2)
        {
            long num_grids;
            long j;

            if (coda_cursor_get_num_elements(&cursor, &num_grids) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (num_grids == 0)
            {
                harp_set_error(HARP_ERROR_INGESTION, "missing grid section for GRIB message %ld", i);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (j = 0; j < num_grids; j++)
            {
                if (get_lat_lon_grid(&cursor, grib_version, info, i == 0 && j == 0) != 0)
                {
                    return -1;
                }
            }
            if (j < num_grids - 1)
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
            coda_cursor_goto_parent(&cursor);
        }
        else
        {
            if (get_lat_lon_grid(&cursor, grib_version, info, i == 0) != 0)
            {
                return -1;
            }
        }
        coda_cursor_goto_parent(&cursor);

        if (grib_version == 2)
        {
            long j;

            if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (j = 0; j < num_data; j++)
            {
                uint8_t typeOfFirstFixedSurface;
                uint8_t parameterCategory;
                uint8_t parameterNumber;
                grib_parameter parameter;
                double datetime;

                if (coda_cursor_goto(&cursor, "parameterCategory") != 0)
                {
                    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                    return -1;
                }
                if (coda_cursor_read_uint8(&cursor, &parameterCategory) != 0)
                {
                    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                parameter_ref += parameterCategory * 256;

                if (coda_cursor_goto(&cursor, "parameterNumber") != 0)
                {
                    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                    return -1;
                }
                if (coda_cursor_read_uint8(&cursor, &parameterNumber) != 0)
                {
                    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                parameter_ref += parameterNumber;
                info->grid_data_parameter_ref[parameter_index] = parameter_ref;
                parameter = get_grib_parameter(parameter_ref);

                if (get_datetime(&cursor, info, &datetime) != 0)
                {
                    return -1;
                }
                if (!datetime_initialised)
                {
                    info->datetime = datetime;
                    datetime_initialised = 1;
                }
                else if (info->datetime != datetime)
                {
                    harp_set_error(HARP_ERROR_INGESTION, "not all data in the GRIB file is for the same time");
                    return -1;
                }

                if (parameter != grib_param_unknown)
                {
                    if (coda_cursor_goto_record_field_by_name(&cursor, "typeOfFirstFixedSurface") != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_read_uint8(&cursor, &typeOfFirstFixedSurface) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    /* we only know how to deal with hybrid levels */
                    /* even surface properties are expected to be provided at level=1 using hybrid levels */
                    if (typeOfFirstFixedSurface != 105)
                    {
                        harp_set_error(HARP_ERROR_INGESTION, "unsupported value for 'type of first fixed surface' "
                                       "(%d) for vertical axis", typeOfFirstFixedSurface);
                        return -1;
                    }
                    if (coda_cursor_goto_next_record_field(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    /* firstFixedSurface -> hybrid level value for vertical axis */
                    if (coda_cursor_read_double(&cursor, &info->level[parameter_index]) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    coda_cursor_goto_parent(&cursor);
                    if (info->level[parameter_index] != 1)
                    {
                        long num_coordinate_values;

                        if (coda_cursor_goto_record_field_by_name(&cursor, "coordinateValues") != 0)
                        {
                            harp_set_error(HARP_ERROR_CODA, NULL);
                            return -1;
                        }
                        if (coda_cursor_get_num_elements(&cursor, &num_coordinate_values) != 0)
                        {
                            harp_set_error(HARP_ERROR_CODA, NULL);
                            return -1;
                        }
                        if (info->coordinate_values == NULL)
                        {
                            info->num_grib_levels = (num_coordinate_values / 2) - 1;
                            info->coordinate_values = malloc(num_coordinate_values * sizeof(double));
                            if (info->coordinate_values == NULL)
                            {
                                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) "
                                               "(%s:%u)", num_coordinate_values * sizeof(double), __FILE__, __LINE__);
                                return -1;
                            }
                            if (coda_cursor_read_double_array(&cursor, info->coordinate_values, coda_array_ordering_c)
                                != 0)
                            {
                                harp_set_error(HARP_ERROR_CODA, NULL);
                                return -1;
                            }
                        }
                        else if (num_coordinate_values != 2 * (info->num_grib_levels + 1))
                        {
                            /* we only check for the number of vertical levels. currently no check is performed to
                             * verify that the coordinate values are actually the same */
                            harp_set_error(HARP_ERROR_INGESTION,
                                           "not all data in the GRIB file has the same number of vertical levels");
                            return -1;
                        }
                        coda_cursor_goto_parent(&cursor);
                    }
                }

                if (coda_cursor_goto_record_field_by_name(&cursor, "values") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                info->parameter_cursor[parameter_index] = cursor;
                coda_cursor_goto_parent(&cursor);

                if (j < num_data - 1)
                {
                    parameter_index++;
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            coda_cursor_goto_parent(&cursor);
            coda_cursor_goto_parent(&cursor);
        }
        else
        {
            uint8_t table2Version;
            uint8_t indicatorOfParameter;
            uint8_t indicatorOfTypeOfLevel;
            uint16_t level;

            if (coda_cursor_goto(&cursor, "table2Version") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8(&cursor, &table2Version) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            parameter_ref += table2Version * 256;
            if (coda_cursor_goto(&cursor, "indicatorOfParameter") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8(&cursor, &indicatorOfParameter) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            parameter_ref += indicatorOfParameter;
            info->grid_data_parameter_ref[parameter_index] = parameter_ref;

            if (!datetime_initialised)
            {
                info->datetime = info->reference_datetime;
                datetime_initialised = 1;
            }
            else if (info->datetime != info->reference_datetime)
            {
                harp_set_error(HARP_ERROR_INGESTION, "not all data in the GRIB file is for the same time");
                return -1;
            }

            if (coda_cursor_goto(&cursor, "indicatorOfTypeOfLevel") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8(&cursor, &indicatorOfTypeOfLevel) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            /* we only support surface level properties and hybrid levels for GRIB1 data */
            if (indicatorOfTypeOfLevel != 1 && indicatorOfTypeOfLevel != 109)
            {
                harp_set_error(HARP_ERROR_INGESTION, "unsupported value for 'type of level' (%d) for vertical axis",
                               indicatorOfTypeOfLevel);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "level") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &level) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            if (indicatorOfTypeOfLevel == 109 && level != 1)
            {
                long num_coordinate_values;

                if (coda_cursor_goto(&cursor, "grid/coordinateValues") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_num_elements(&cursor, &num_coordinate_values) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (info->coordinate_values == NULL)
                {
                    info->num_grib_levels = (num_coordinate_values / 2) - 1;
                    info->coordinate_values = malloc(num_coordinate_values * sizeof(double));
                    if (info->coordinate_values == NULL)
                    {
                        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) "
                                       "(%s:%u)", num_coordinate_values * sizeof(double), __FILE__, __LINE__);
                        return -1;
                    }
                    if (coda_cursor_read_double_array(&cursor, info->coordinate_values, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else if (num_coordinate_values != 2 * (info->num_grib_levels + 1))
                {
                    /* we only check for the number of vertical levels. currently no check is performed to
                     * verify that the coordinate values are actually the same */
                    harp_set_error(HARP_ERROR_INGESTION,
                                   "not all data in the GRIB file has the same number of vertical levels");
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                coda_cursor_goto_parent(&cursor);
            }

            if (coda_cursor_goto(&cursor, "data/values") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            info->parameter_cursor[parameter_index] = cursor;
            coda_cursor_goto_parent(&cursor);
            coda_cursor_goto_parent(&cursor);

            info->level[parameter_index] = (double)level;
        }
        parameter_index++;

        coda_cursor_goto_parent(&cursor);
        if (i < info->num_messages - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    /* initialize grid_data_index */
    info->num_levels = info->num_grib_levels > 0 ? info->num_grib_levels : 1;
    info->grid_data_index = malloc(NUM_GRIB_PARAMETERS * info->num_levels * sizeof(long *));
    if (info->grid_data_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       NUM_GRIB_PARAMETERS * info->num_levels * sizeof(long *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < NUM_GRIB_PARAMETERS * info->num_levels; i++)
    {
        /* initialize to invalid value */
        info->grid_data_index[i] = -1;
    }
    for (i = 0; i < info->num_grid_data; i++)
    {
        grib_parameter param;

        param = get_grib_parameter(info->grid_data_parameter_ref[i]);
        if (param == grib_param_unknown)
        {
            for (j = 0; j < i; j++)
            {
                if (info->grid_data_parameter_ref[i] == info->grid_data_parameter_ref[j])
                {
                    break;
                }
            }
            /* only report the warning for the first occurrence */
            if (i == j)
            {
                if (info->grid_data_parameter_ref[i] >> 24 == 1)
                {
                    harp_report_warning("unsupported GRIB1 parameter (table2Version %d, indicatorOfParameter %d)",
                                        (info->grid_data_parameter_ref[i] >> 8) & 0xff,
                                        info->grid_data_parameter_ref[i] & 0xff);
                }
                else
                {
                    harp_report_warning("unsupported GRIB2 parameter (discipline %d, parameterCategory %d, "
                                        "parameterNumber %d)", (info->grid_data_parameter_ref[i] >> 16) & 0xff,
                                        (info->grid_data_parameter_ref[i] >> 8) & 0xff,
                                        info->grid_data_parameter_ref[i] & 0xff);
                }
            }
        }
        else
        {
            long level = (long)info->level[i];

            if (param_is_profile[param])
            {
                if (level < 1 || level > info->num_grib_levels)
                {
                    harp_set_error(HARP_ERROR_INGESTION, "invalid level value (%lf) for parameter %s", info->level[i],
                                   param_name[param]);
                    return -1;
                }
            }
            else
            {
                if (level != 0 && level != 1)
                {
                    harp_set_error(HARP_ERROR_INGESTION, "invalid level value (%lf) for surface parameter %s",
                                   info->level[i], param_name[param]);
                    return -1;
                }
                level = 1;
            }
            info->has_parameter[param] = 1;
            if (info->grid_data_index[param * info->num_levels + level - 1] != -1)
            {
                harp_set_error(HARP_ERROR_INGESTION, "parameter %s and level (%lf) occur more than once in file",
                               param_name[param], info->level[i]);
                return -1;
            }
            else
            {
                info->grid_data_index[param * info->num_levels + level - 1] = i;
            }
        }
    }

    return 0;
}

static void ingest_info_delete(ingest_info *info)
{
    if (info != NULL)
    {
        if (info->grid_data_parameter_ref != NULL)
        {
            free(info->grid_data_parameter_ref);
        }
        if (info->parameter_cursor != NULL)
        {
            free(info->parameter_cursor);
        }
        if (info->level != NULL)
        {
            free(info->level);
        }
        if (info->longitude != NULL)
        {
            free(info->longitude);
        }
        if (info->latitude != NULL)
        {
            free(info->latitude);
        }
        if (info->coordinate_values != NULL)
        {
            free(info->coordinate_values);
        }
        if (info->grid_data_index != NULL)
        {
            free(info->grid_data_index);
        }
        free(info);
    }
}

static int ingest_info_new(coda_product *product, ingest_info **new_info)
{
    ingest_info *info;
    int i;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->num_messages = 0;
    info->num_grid_data = 0;
    info->grid_data_parameter_ref = NULL;
    info->parameter_cursor = NULL;
    info->level = NULL;
    info->wavelength = harp_nan();
    info->datetime = 0;
    info->reference_datetime = 0;
    info->is_forecast_datetime = 0;
    info->Ni = 0;
    info->Nj = 0;
    info->latitudeOfFirstGridPoint = 0;
    info->longitudeOfFirstGridPoint = 0;
    info->latitudeOfLastGridPoint = 0;
    info->longitudeOfLastGridPoint = 0;
    info->iDirectionIncrement = 0;
    info->jDirectionIncrement = 0;
    info->N = 0;
    info->is_gaussian = 0;
    info->grid_grib_version = 0;
    info->num_longitudes = 0;
    info->longitude = NULL;
    info->num_latitudes = 0;
    info->latitude = NULL;
    info->num_levels = 1;
    info->num_grib_levels = 0;
    info->coordinate_values = NULL;
    info->grid_data_index = NULL;

    for (i = 0; i < NUM_GRIB_PARAMETERS; i++)
    {
        info->has_parameter[i] = 0;
    }

    *new_info = info;

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info_delete((ingest_info *)user_data);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;
    coda_format format;

    (void)options;
    if (ingest_info_new(product, &info) != 0)
    {
        return -1;
    }

    if (coda_get_product_format(product, &format) != 0)
    {
        return -1;
    }
    assert(format == coda_format_grib);

    if (init_cursors_and_grid(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int exclude_wavelength(void *user_data)
{
    return harp_isnan(((ingest_info *)user_data)->wavelength);
}

static int exclude_tclw(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tclw];
}

static int exclude_tciw(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tciw];
}

static int exclude_z(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_z];
}

static int exclude_t(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_t];
}

static int exclude_q(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_q];
}

static int exclude_tcwv(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tcwv];
}

static int exclude_vo(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_vo];
}

static int exclude_lnsp(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_lnsp];
}

static int exclude_pressure(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return !info->has_parameter[grib_param_lnsp] || info->coordinate_values == NULL;
}

static int exclude_blh(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_blh];
}

static int exclude_tcc(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tcc];
}

static int exclude_10u(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_10u];
}

static int exclude_10v(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_10v];
}

static int exclude_2t(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_2t];
}

static int exclude_lsm(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_lsm];
}

static int exclude_clwc(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_clwc];
}

static int exclude_ciwc(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_ciwc];
}

static int exclude_co2(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_co2];
}

static int exclude_ch4(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_ch4];
}

static int exclude_pm1(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_pm1];
}

static int exclude_pm2p5(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_pm2p5];
}

static int exclude_pm10(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_pm10];
}

static int exclude_no2(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_no2];
}

static int exclude_so2(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_so2];
}

static int exclude_co(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_co];
}

static int exclude_hcho(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_hcho];
}

static int exclude_tcno2(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tcno2];
}

static int exclude_tcso2(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tcso2];
}

static int exclude_tcco(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tcco];
}

static int exclude_tchcho(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tchcho];
}

static int exclude_go3(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_go3];
}

static int exclude_gtco3(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_gtco3];
}

static int exclude_aod(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->has_parameter[grib_param_aod550])
    {
        return 0;
    }
    if (info->has_parameter[grib_param_aod469])
    {
        return 0;
    }
    if (info->has_parameter[grib_param_aod670])
    {
        return 0;
    }
    if (info->has_parameter[grib_param_aod865])
    {
        return 0;
    }
    if (info->has_parameter[grib_param_aod1240])
    {
        return 0;
    }

    return 1;
}

static int exclude_ssaod(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_ssaod550];
}

static int exclude_duaod(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_duaod550];
}

static int exclude_omaod(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_omaod550];
}

static int exclude_bcaod(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_bcaod550];
}

static int exclude_suaod(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_suaod550];
}

static int exclude_hno3(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_hno3];
}

static int exclude_pan(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_pan];
}

static int exclude_c5h8(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_c5h8];
}

static int exclude_no(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_no];
}

static int exclude_oh(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_oh];
}

static int exclude_c2h6(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_c2h6];
}

static int exclude_c3h8(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_c3h8];
}

static int exclude_tc_ch4(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_ch4];
}

static int exclude_tc_hno3(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_hno3];
}

static int exclude_tc_pan(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_pan];
}

static int exclude_tc_c5h8(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_c5h8];
}

static int exclude_tc_no(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_no];
}

static int exclude_tc_oh(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_oh];
}

static int exclude_tc_c2h6(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_c2h6];
}

static int exclude_tc_c3h8(void *user_data)
{
    return !((ingest_info *)user_data)->has_parameter[grib_param_tc_c3h8];
}

static void add_value_variable_mapping(harp_variable_definition *variable_definition, const char *grib1_description,
                                       const char *grib2_description)
{
    if (grib1_description != NULL)
    {
        harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB1", "/[]/data/values[]",
                                             grib1_description);
    }
    if (grib2_description != NULL)
    {
        harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB2", "/[]/data[]/values[]",
                                             grib2_description);
    }
}

int harp_ingestion_module_ecmwf_grib_init(void)
{
    harp_dimension_type dimension_type[5] = {
        harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude, harp_dimension_vertical,
        harp_dimension_independent
    };
    long bounds_dimension[5] = { -1, -1, -1, -1, 2 };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("ECMWF_GRIB", "ECMWF GRIB", "ECMWF", "GRIB",
                                                 "ECMWF model data in GRIB format", ingestion_init, ingestion_done);

    /* ECMWF GRIB product */
    description = "The file can use either the GRIB1 or GRIB2 format. "
        "Only GRIB files that use a 'centre' value referring to ECMWF are supported. "
        "The parameters in the file should have the same time value, the same lat/lon grid and the same vertical grid.";
    product_definition = harp_ingestion_register_product(module, "ECMWF_GRIB", description, read_dimensions);

    /* datetime */
    description = "time of the model state";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "seconds since 2000-01-01", NULL, read_datetime);

    description = "time of the measurement converted from TAI93 to seconds since 2000-01-01T00:00:00";
    path = "/[]/grib1/yearOfCentury, /[]/grib1/month, /[]/grib1/day, /[]/grib1/hour, /[]/grib1/minute, "
        "/[]/grib1/centuryOfReferenceTimeOfData";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB1", path, description);
    path = "/[]/grib2/year, /[]/grib2/month, /[]/grib2/day, /[]/grib2/hour, /[]/grib2/minute, /[]/grib2/second";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB2", path, description);

    /* longitude */
    description = "longitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude", harp_type_double,
                                                                      1, &dimension_type[2], NULL, description,
                                                                      "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    description = "based on linear interpolation using Ni points from first to last grid point";
    path = "/[]/grib1/grid/Ni, /[]/grib1/grid/longitudeOfFirstGridPoint, /[]/grib1/grid/longitudeOfLastGridPoint";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB1", path, description);
    path = "/[]/grib2/grid[]/Ni, /[]/grib2/grid[]/longitudeOfFirstGridPoint, /[]/grib2/grid[]/longitudeOfLastGridPoint";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB2", path, description);

    /* latitude */
    description = "latitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double,
                                                                      1, &dimension_type[1], NULL, description,
                                                                      "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    description = "based on linear interpolation using Nj points from first to last grid point";
    path = "/[]/grib1/grid/Nj, /[]/grib1/grid/latitudeOfFirstGridPoint, /[]/grib1/grid/latitudeOfLastGridPoint";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB1", path, description);
    path = "/[]/grib2/grid[]/Nj, /[]/grib2/grid[]/latitudeOfFirstGridPoint, /[]/grib2/grid[]/latitudeOfLastGridPoint";
    harp_variable_definition_add_mapping(variable_definition, NULL, "GRIB2", path, description);

    /* wavelength */
    description = "wavelength of the aerosol property";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 0, NULL, NULL,
                                                   description, "nm", exclude_wavelength, read_wavelength);
    description =
        "the wavelength value is based on the AOD; possible values are: 469, 550, 670, 865, 1240; a single "
        "HARP file will not have AODs at more than one wavelength";
    harp_variable_definition_add_mapping(variable_definition, NULL, "AOD quantity is present", NULL, description);


    /* tclw: LWC_column_density */
    description = "total column cloud liquid water";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "LWC_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tclw, read_tclw);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,78)",
                               "(discipline,category,number) = (192,128,78)");

    /* tciw: IWC_column_density */
    description = "total column cloud ice water";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "IWC_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tciw, read_tciw);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,79)",
                               "(discipline,category,number) = (192,128,79)");

    /* z: surface_geopotential */
    description = "geopotential at the surface";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_geopotential",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "m2/s2", exclude_z, read_z);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,129), (160,129), (170,129), (180,129), "
                               "or (190,129)", "(discipline,category,number) = (0,3,4)");

    /* t: temperature */
    description = "temperature";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "temperature",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "K", exclude_t, read_t);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,130), (160,130), (170,130), (180,130), "
                               "or (190,130)", "(discipline,category,number) = (0,0,0)");

    /* q: H2O_mass_mixing_ratio */
    description = "specific humidity";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_q, read_q);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,133), (160,133), (170,133), (180,133), "
                               "or (190,133)", "(discipline,category,number) = (0,1,0)");

    /* tcwv: H2O_column_density */
    description = "total column water vapour";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tcwv, read_tcwv);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,137) or (180,137)",
                               "(discipline,category,number) = (192,128,137)");

    /* vo: relative_vorticity */
    description = "relative vorticity";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "relative_vorticity",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "1/s", exclude_vo, read_vo);
    add_value_variable_mapping(variable_definition,
                               "(table,indicator) = (160,138), (128,138), (170,138), (180, 138) or (190,138)",
                               "(discipline,category,number) = (0,2,12)");

    /* lnsp: surface_pressure */
    description = "pressure at the surface";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_pressure",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "Pa", exclude_lnsp, read_lnsp);
    add_value_variable_mapping(variable_definition,
                               "(table,indicator) = (128,152) or (190,152); returned value = exp(lnsp)",
                               "(discipline,category,number) = (0,3,25); returned value = exp(lnsp)");

    /* pressure */
    description = "pressure";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "pressure",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "Pa", exclude_pressure,
                                                                      read_pressure);
    description = "the coordinateValues contain [a(1), ..., a(N+1), b(1), ..., b(N+1)] coefficients for the N+1 "
        "vertical layer boundaries; p(N-i) = (a(i) + a(i+1) + (b(i) + b(i+1))lnsp)/2";
    harp_variable_definition_add_mapping(variable_definition, NULL, "surface_pressure is available and at least one "
                                         "parameter has vertical coordinate values", "..../coordinateValues[]",
                                         description);

    /* pressure_bounds */
    description = "pressure_bounds";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "pressure_bounds",
                                                                      harp_type_float, 4, &dimension_type[1],
                                                                      &bounds_dimension[1], description, "Pa",
                                                                      exclude_pressure, read_pressure_bounds);
    description = "the coordinateValues contain [a(1), ..., a(N+1), b(1), ..., b(N+1)] coefficients for the N+1 "
        "vertical layer boundaries; p(N-i,1) = a(i) + b(i)lnsp; p(N-i,2) = a(i+1) + b(i+1)lnsp";
    harp_variable_definition_add_mapping(variable_definition, NULL, "surface_pressure is available and at least one "
                                         "parameter has vertical coordinate values", "..../coordinateValues[]",
                                         description);

    /* blh: planetary_boundary_layer_height */
    description = "planetary boundary layer height";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "planetary_boundary_layer_height",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "m", exclude_blh, read_blh);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,159)",
                               "(discipline,category,number) = (192,128,159)");

    /* tcc: cloud_fraction */
    description = "cloud fraction";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_fraction",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, exclude_tcc,
                                                                      read_tcc);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,164), (160,164), (170,164), (180,164), "
                               "or (190,164)", "(discipline,category,number) = (192,128,164)");

    /* 10u: surface_zonal_wind_velocity */
    description = "10 meter U wind component";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_zonal_wind_velocity",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "m/s", exclude_10u, read_10u);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,165), (160,165), (180,165), or (190,165)",
                               "(discipline,category,number) = (0,2,2)");

    /* 10v: surface_meridional_wind_velocity */
    description = "10 meter V wind component";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "surface_meridional_wind_velocity",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "m/s", exclude_10v, read_10v);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,166), (160,166), (180,166), or (190,166)",
                               "(discipline,category,number) = (0,2,3)");

    /* 2t: surface_temperature */
    description = "2 metre temperature";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_temperature",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "K", exclude_2t, read_2t);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,167), (160,167), (180,167), or (190,167)",
                               NULL);

    /* lsm: land_flag */
    description = "land-sea mask";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "land_flag",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, NULL, exclude_lsm, read_lsm);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,172), (160,172), (171,172), (174,172), "
                               "(175,172), (180,172), or (190,172)", "(discipline,category,number) = (2,0,0)");

    /* clwc: LWC_mass_mixing_ratio */
    description = "specific cloud liquid water content";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "LWC_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_clwc, read_clwc);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,246)",
                               "(discipline,category,number) = (0,1,83)");

    /* ciwc: IWC_mass_mixing_ratio */
    description = "specific cloud ice water content";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "IWC_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_ciwc, read_ciwc);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (128,247)",
                               "(discipline,category,number) = (0,1,84)");

    /* co2: CO2_mass_mixing_ratio */
    description = "carbon dioxide mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CO2_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_co2, read_co2);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,61)",
                               "(discipline,category,number) = (192,210,61)");

    /* ch4: CH4_mass_mixing_ratio */
    description = "methane mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CH4_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_ch4, read_ch4);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,62) or (217,4)",
                               "(discipline,category,number) = (192,210,62) or (192,217,4)");

    /* pm1: surface_PM1_density */
    description = "surface density of particulate matter with d < 1 um";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_PM1_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m3", exclude_pm1, read_pm1);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,72)",
                               "(discipline,category,number) = (192,210,72)");

    /* pm2p5: surface_PM2p5_density */
    description = "surface density of particulate matter with d < 2.5 um";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_PM2p5_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m3", exclude_pm2p5, read_pm2p5);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,73)",
                               "(discipline,category,number) = (192,210,73)");

    /* pm10: surface_PM10_density */
    description = "surface density of particulate matter with d < 10 um";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_PM10_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m3", exclude_pm10, read_pm10);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,74)",
                               "(discipline,category,number) = (192,210,74)");

    /* no2: NO2_mass_mixing_ratio */
    description = "nitrogen dioxide mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_no2, read_no2);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,121)",
                               "(discipline,category,number) = (192,210,121)");

    /* so2: SO2_mass_mixing_ratio */
    description = "sulphur dioxide mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "SO2_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_so2, read_so2);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,122)",
                               "(discipline,category,number) = (192,210,122)");

    /* co: CO_mass_mixing_ratio */
    description = "carbon monoxide mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CO_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_co, read_co);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,123)",
                               "(discipline,category,number) = (192,210,123)");

    /* hcho: HCHO_mass_mixing_ratio */
    description = "formaldehyde mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HCHO_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_hcho, read_hcho);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,124)",
                               "(discipline,category,number) = (192,210,124)");

    /* tcno2: NO2_column_density */
    description = "total column nitrogen dioxide";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tcno2, read_tcno2);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,125)",
                               "(discipline,category,number) = (192,210,125)");

    /* tcso2: SO2_column_density */
    description = "total column sulphur dioxide";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "SO2_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tcso2, read_tcso2);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,126)",
                               "(discipline,category,number) = (192,210,126)");

    /* tcco: CO_column_density */
    description = "total column carbon monoxide";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CO_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tcco, read_tcco);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,127)",
                               "(discipline,category,number) = (192,210,127)");

    /* HCHO: HCHO_column_density */
    description = "total column formaldehyde";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HCHO_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tchcho,
                                                                      read_tchcho);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,128)",
                               "(discipline,category,number) = (192,210,128)");

    /* go3: O3_mass_mixing_ratio */
    description = "ozone mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_go3, read_go3);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,203)",
                               "(discipline,category,number) = (192,210,203)");

    /* gtco3: O3_column_density */
    description = "total column ozone";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_gtco3, read_gtco3);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,206)",
                               "(discipline,category,number) = (192,210,206)");

    /* aod550/aod469/aod670/aod865/aod1240: aerosol_optical_depth */
    description = "total aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "aerosol_optical_depth",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, exclude_aod,
                                                                      read_aod);
    add_value_variable_mapping(variable_definition, "Only one AOD wavelength is allowed; (table,indicator) = (210,207) "
                               "[550nm], (210,213) [469nm], (210,214) [670nm], (210,215) [865nm], or (210,216) "
                               "[1240nm]", "(discipline,category,number) = (192,210,207) [550nm], (192,210,213) "
                               "[469nm], (192,210,214) [670nm], (192,210,215) [865nm], or (192,210,216) [1240nm]");

    /* ssaod550: sea_salt_aerosol_optical_depth */
    description = "sea salt aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "sea_salt_aerosol_optical_depth", harp_type_float,
                                                                      2, &dimension_type[1], NULL, description,
                                                                      HARP_UNIT_DIMENSIONLESS, exclude_ssaod,
                                                                      read_ssaod);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,208) [550nm]",
                               "(discipline,category,number) = (192,210,208) [550nm]");

    /* duaod550: dust_aerosol_optical_depth */
    description = "dust aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "dust_aerosol_optical_depth",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      exclude_duaod, read_duaod);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,209) [550nm]",
                               "(discipline,category,number) = (192,210,209) [550nm]");

    /* omaod550: organic_matter_aerosol_optical_depth */
    description = "organic matter aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "organic_matter_aerosol_optical_depth",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      exclude_omaod, read_omaod);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,210) [550nm]",
                               "(discipline,category,number) = (192,210,210) [550nm]");

    /* bcaod550: black_carbon_aerosol_optical_depth */
    description = "black carbon aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "black_carbon_aerosol_optical_depth",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      exclude_bcaod, read_bcaod);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,211) [550nm]",
                               "(discipline,category,number) = (192,210,211) [550nm]");

    /* suaod550: sulphate_aerosol_optical_depth */
    description = "sulphate aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "sulphate_aerosol_optical_depth",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      exclude_suaod, read_suaod);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (210,212) [550nm]",
                               "(discipline,category,number) = (192,210,212) [550nm]");

    /* hno3: HNO3_mass_mixing_ratio */
    description = "nitric acid mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HNO3_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_hno3, read_hno3);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,6)",
                               "(discipline,category,number) = (192,217,6)");

    /* pan: C2H3NO5_mass_mixing_ratio */
    description = "peroxyacetyl nitrate (PAN) mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C2H3NO5_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_pan, read_pan);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,13)",
                               "(discipline,category,number) = (192,217,13)");

    /* c5h8: C5H8_mass_mixing_ratio */
    description = "isoprene mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C5H8_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_c5h8, read_c5h8);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,16)",
                               "(discipline,category,number) = (192,217,16)");

    /* no: NO_mass_mixing_ratio */
    description = "nitrogen monoxide mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_no, read_no);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,27)",
                               "(discipline,category,number) = (192,217,27)");

    /* oh: OH_mass_mixing_ratio */
    description = "hydroxyl radical mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "OH_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_oh, read_oh);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,30)",
                               "(discipline,category,number) = (192,217,30)");

    /* c2h6: C2H6_mass_mixing_ratio */
    description = "ethane mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C2H6_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_c2h6, read_c2h6);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,45)",
                               "(discipline,category,number) = (192,217,45)");

    /* c3h8: C3H8_mass_mixing_ratio */
    description = "propane mass mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C3H8_mass_mixing_ratio",
                                                                      harp_type_float, 3, &dimension_type[1], NULL,
                                                                      description, "kg/kg", exclude_c3h8, read_c3h8);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (217,47)",
                               "(discipline,category,number) = (192,217,47)");

    /* tc_ch4: CH4_column_density */
    description = "total column methane";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CH4_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_ch4,
                                                                      read_tc_ch4);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,4)",
                               "(discipline,category,number) = (192,218,4)");

    /* tc_hno3: HNO3_column_density */
    description = "total column nitric acid";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HNO3_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_hno3,
                                                                      read_tc_hno3);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,6)",
                               "(discipline,category,number) = (192,218,6)");

    /* tc_pan: C2H3NO5_column_density */
    description = "total colunn peroxyacetyl nitrate";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C2H3NO5_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_pan,
                                                                      read_tc_pan);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,13)",
                               "(discipline,category,number) = (192,218,13)");

    /* tc_c5h8: C5H8_column_density */
    description = "total column isoprene";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C5H8_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_c5h8,
                                                                      read_tc_c5h8);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,16)",
                               "(discipline,category,number) = (192,218,16)");

    /* tc_no: NO_column_density */
    description = "total column nitrogen oxide";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_no, read_tc_no);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,27)",
                               "(discipline,category,number) = (192,218,27)");

    /* tc_oh: OH_column_density */
    description = "total column hydroxyl radical";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "OH_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_oh, read_tc_oh);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,30)",
                               "(discipline,category,number) = (192,218,30)");

    /* tc_c2h6: C2H6_column_density */
    description = "total column ethane";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C2H6_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_c2h6,
                                                                      read_tc_c2h6);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,45)",
                               "(discipline,category,number) = (192,218,45)");

    /* tc_c3h8: C3H8_column_density */
    description = "total column propane";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "C3H8_column_density",
                                                                      harp_type_float, 2, &dimension_type[1], NULL,
                                                                      description, "kg/m^2", exclude_tc_c3h8,
                                                                      read_tc_c3h8);
    add_value_variable_mapping(variable_definition, "(table,indicator) = (218,47)",
                               "(discipline,category,number) = (192,218,47)");

    return 0;
}

/**
 * Bosch Sensortec BME690/BME69x forced mode example - Raspberry Pi I2C
 * Modified: thermal profile + hysteresis feature vector output
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "bme69x.h"
#include "common.h"

/***********************************************************************/
/*                         Config                                      */
/***********************************************************************/
#define SAMPLE_COUNT      UINT16_C(600)   /* total samples to print */
#define WARMUP_CYCLES     2              /* cycles to ignore for feature output */

/***********************************************************************/
/*                 Milliseconds timestamp helper                       */
/***********************************************************************/
static uint32_t millis_since_start(void)
{
    static struct timespec t0;
    static int initialized = 0;

    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    if (!initialized)
    {
        t0 = t;
        initialized = 1;
        return 0;
    }

    uint64_t ms = (uint64_t)(t.tv_sec - t0.tv_sec) * 1000ULL;
    int64_t ns  = (int64_t)(t.tv_nsec - t0.tv_nsec);
    ms += (uint64_t)(ns / 1000000LL);

    return (uint32_t)ms;
}

/***********************************************************************/
/*                 Feature vector computation                           */
/***********************************************************************/
static void print_feature_vector(uint32_t cycle_id,
                                 const double *gas_cycle,
                                 int profile_len)
{
    int half = profile_len / 2;

    /* log(R) transform -> diff = up - flipped(down) */
    printf("FEATURE_VEC,%lu", (unsigned long)cycle_id);

    for (int i = 0; i < half; i++)
    {
        double up = gas_cycle[i];
        double down_flip = gas_cycle[profile_len - 1 - i];
        double diff = down_flip - up;

        printf(",%.6f", diff);
    }
    printf("\n");
    fflush(stdout);
}

/***********************************************************************/
/*                         Main                                         */
/***********************************************************************/
int main(void)
{
    struct bme69x_dev bme;
    int8_t rslt;

    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data;

    uint32_t del_period;
    uint8_t n_fields;
    uint16_t sample_count = 1;

    /* ---- 10 temps up, 10 temps down (symmetric!) ---- */
    static const uint16_t profile[] = {
      100, 175, 250, 325, 325, 250, 175, 100
    };
    const int profile_len = (int)(sizeof(profile) / sizeof(profile[0]));

    /* Buffer for gas resistance over one full cycle */
    double gas_cycle[sizeof(profile)/sizeof(profile[0])] = {0};
    int cycle_index = 0;        /* 0..profile_len-1 */
    uint32_t cycle_id = 0;

    /* Init I2C */
    rslt = bme69x_interface_init(&bme, BME69X_I2C_INTF);
    bme69x_check_rslt("bme69x_interface_init", rslt);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_init(&bme);
    bme69x_check_rslt("bme69x_init", rslt);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme); return rslt; }

    /* Faster config (important for 100ms step timing) */
    conf.filter  = BME69X_FILTER_OFF;
    conf.odr     = BME69X_ODR_NONE;
    conf.os_hum  = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_1X;

    rslt = bme69x_set_conf(&conf, &bme);
    bme69x_check_rslt("bme69x_set_conf", rslt);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme); return rslt; }

    /* Heater config */
    heatr_conf.enable    = BME69X_ENABLE;
    heatr_conf.heatr_dur = 250; /* ms */

    /* Timestamp baseline */
    (void)millis_since_start();

    /* Print raw stream header */
    printf("Sample,StepTemp(C),Time(ms),Temperature(C),Pressure(Pa),Humidity(%%),Gas(ohm),Status\n");

    while (sample_count <= SAMPLE_COUNT)
    {
        /* Heater step temp */
        uint16_t step_temp = profile[(sample_count - 1U) % (uint16_t)profile_len];
        heatr_conf.heatr_temp = step_temp;

        rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, &bme);
        bme69x_check_rslt("bme69x_set_heatr_conf", rslt);
        if (rslt != BME69X_OK) break;

        rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &bme);
        bme69x_check_rslt("bme69x_set_op_mode", rslt);
        if (rslt != BME69X_OK) break;

        /* Delay = measurement duration + heater duration */
        del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, &conf, &bme)
                   + (heatr_conf.heatr_dur * 1000U);
        bme.delay_us(del_period, bme.intf_ptr);

        uint32_t t_ms = millis_since_start();

        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &bme);
        bme69x_check_rslt("bme69x_get_data", rslt);
        if (rslt != BME69X_OK) break;

        if (n_fields)
        {
            /* Raw print */
#ifdef BME69X_USE_FPU
            printf("%u,%u,%lu,%.2f,%.2f,%.2f,%.2f,0x%x\n",
                   sample_count,
                   step_temp,
                   (long unsigned int)t_ms,
                   data.temperature,
                   data.pressure,
                   data.humidity,
                   data.gas_resistance,
                   data.status);
#else
            printf("%u,%u,%lu,%d,%ld,%ld,%ld,0x%x\n",
                   sample_count,
                   step_temp,
                   (long unsigned int)t_ms,
                   data.temperature,
                   data.pressure,
                   data.humidity,
                   data.gas_resistance,
                   data.status);
#endif

            /* Store into cycle buffer */
            gas_cycle[cycle_index] = (double)data.gas_resistance;
            cycle_index++;

            /* If full cycle collected: compute & print features */
            if (cycle_index >= profile_len)
            {
                cycle_id++;

                if (cycle_id > WARMUP_CYCLES)
                {
                    print_feature_vector(cycle_id, gas_cycle, profile_len);
                }

                cycle_index = 0;
            }

            sample_count++;
        }
    }

    bme69x_interface_deinit(&bme);
    return rslt;
}

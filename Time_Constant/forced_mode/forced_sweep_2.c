/**
 * forced_sweep_2.c
 * Sweep heater square-wave frequency and log gas resistance vs time for TWO BME69x sensors.
 *
 * Output:
 *   header,t_ms,addr,heater_C,gas_ohm
 *   SWEEP,half_ms,f_hz,cycles,Fs
 *   t_ms,0x76,heater_C,gas_ohm
 *   t_ms,0x77,heater_C,gas_ohm
 *   ...
 *   ENDSWEEP,half_ms
 */

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "bme69x.h"
#include "common.h"

/* ---------- Addresses ---------- */
#define ADDR1 0x76
#define ADDR2 0x77

/* ---------- Heater square wave ---------- */
#define T_LOW_C         250
#define T_HIGH_C        320

/* Sampling (fixed) */
#define TS_MS           10     /* 10ms => Fs=100Hz ,Nyquist = 50Hz*/
#define HEATER_DUR_MS   3      /* keep small vs TS_MS */

/* Sweep list: half-periods in ms (toggle every half_ms) */
static const int half_list_ms[] = { 50, 75, 100, 125, 150, 200, 250, 300, 400, 500 };

#define CYCLES_PER_FREQ 15
#define WARMUP_CYCLES   3

static uint64_t monotonic_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL;
}

static void sleep_until_ms(uint64_t target_ms)
{
    while (1)
    {
        uint64_t now = monotonic_ms();
        if (now >= target_ms) return;

        uint64_t diff = target_ms - now;
        struct timespec ts;
        ts.tv_sec  = (time_t)(diff / 1000ULL);
        ts.tv_nsec = (long)((diff % 1000ULL) * 1000000ULL);
        nanosleep(&ts, NULL);
    }
}

static int init_sensor(struct bme69x_dev *bme, uint8_t addr, struct bme69x_conf *conf)
{
    int8_t rslt;

    rslt = bme69x_interface_init(bme, BME69X_I2C_INTF, addr);
    bme69x_check_rslt("bme69x_interface_init", rslt);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_init(bme);
    bme69x_check_rslt("bme69x_init", rslt);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_set_conf(conf, bme);
    bme69x_check_rslt("bme69x_set_conf", rslt);
    return rslt;
}

static int sample_once(struct bme69x_dev *bme,
                       struct bme69x_conf *conf,
                       struct bme69x_heatr_conf *heatr_conf,
                       double *gas_out)
{
    int8_t rslt;
    struct bme69x_data data;
    uint8_t n_fields = 0;
    uint32_t del_period;

    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, heatr_conf, bme);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, bme);
    if (rslt != BME69X_OK) return rslt;

    del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, conf, bme)
               + (heatr_conf->heatr_dur * 1000U);
    bme->delay_us(del_period, bme->intf_ptr);

    rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, bme);
    if (rslt != BME69X_OK) return rslt;
    if (!n_fields) return BME69X_E_COM_FAIL;

    *gas_out = (double)data.gas_resistance;
    return BME69X_OK;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    struct bme69x_dev bme1, bme2;
    int8_t rslt;

    /* Fast config */
    struct bme69x_conf conf;
    conf.filter  = BME69X_FILTER_OFF;
    conf.odr     = BME69X_ODR_NONE;
    conf.os_hum  = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_1X;

    rslt = init_sensor(&bme1, ADDR1, &conf);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme1); return rslt; }

    rslt = init_sensor(&bme2, ADDR2, &conf);
    if (rslt != BME69X_OK) {
        bme69x_interface_deinit(&bme1);
        bme69x_interface_deinit(&bme2);
        return rslt;
    }

    struct bme69x_heatr_conf heatr_conf;
    heatr_conf.enable    = BME69X_ENABLE;
    heatr_conf.heatr_dur = HEATER_DUR_MS;

    const double Fs = 1000.0 / (double)TS_MS;

    uint64_t t0 = monotonic_ms();
    uint64_t next = t0;

    printf("header,t_ms,addr,heater_C,gas_ohm\n");

    for (unsigned i = 0; i < sizeof(half_list_ms)/sizeof(half_list_ms[0]); i++)
    {
        int half_ms = half_list_ms[i];
        int period_ms = 2 * half_ms;
        double f_hz = 1000.0 / (double)period_ms;

        int total_cycles = WARMUP_CYCLES + CYCLES_PER_FREQ;
        uint64_t run_ms = (uint64_t)total_cycles * (uint64_t)period_ms;

        printf("SWEEP,%d,%.6f,%d,%.2f\n", half_ms, f_hz, CYCLES_PER_FREQ, Fs);

        uint64_t seg_start = monotonic_ms();

        while (monotonic_ms() - seg_start < run_ms)
        {
            uint64_t rel = monotonic_ms() - seg_start;
            int phase = (int)(rel % (uint64_t)period_ms);
            uint16_t heater = (phase < half_ms) ? T_LOW_C : T_HIGH_C;

            heatr_conf.heatr_temp = heater;

            double gas1 = 0.0, gas2 = 0.0;
            int ok1 = (sample_once(&bme1, &conf, &heatr_conf, &gas1) == BME69X_OK);
            int ok2 = (sample_once(&bme2, &conf, &heatr_conf, &gas2) == BME69X_OK);

            uint64_t t_ms = monotonic_ms() - t0;

            if (ok1) printf("%llu,0x%02X,%u,%.6f\n",
                            (unsigned long long)t_ms, ADDR1, (unsigned)heater, gas1);
            if (ok2) printf("%llu,0x%02X,%u,%.6f\n",
                            (unsigned long long)t_ms, ADDR2, (unsigned)heater, gas2);

            next += (uint64_t)TS_MS;
            sleep_until_ms(next);
        }

        printf("ENDSWEEP,%d\n", half_ms);
    }

    bme69x_interface_deinit(&bme1);
    bme69x_interface_deinit(&bme2);
    return 0;
}

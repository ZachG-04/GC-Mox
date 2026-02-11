/**
 * forced_2_gas_ratio.c
 *
 * Dual BME69x drift-free square-wave sensing using ratio:
 *     ratio = R_high / R_low
 *
 * Heater square wave:
 *   250C <-> 320C every 100 ms (200 ms period)
 *
 * Output:
 *   RATIO,t_ms,addr,ratio
 */

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "bme69x.h"
#include "common.h"

/* I2C addresses */
#define ADDR1 0x76
#define ADDR2 0x77

/* Square wave temps */
#define T_LOW_C   150
#define T_HIGH_C  320

#define SQ_PERIOD_MS 200
#define HALF_MS      100

/* Sampling */
#define TS_MS 25           /* 4 samples per half-cycle */
#define HEATER_DUR_MS 5

static uint64_t monotonic_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000ULL + t.tv_nsec / 1000000ULL;
}

static void sleep_until_ms(uint64_t target)
{
    while (monotonic_ms() < target)
        nanosleep(&(struct timespec){0, 500000}, NULL);
}

static int init_sensor(struct bme69x_dev *bme, uint8_t addr,
                       struct bme69x_conf *conf)
{
    int8_t r;

    r = bme69x_interface_init(bme, BME69X_I2C_INTF, addr);
    if (r != BME69X_OK) return r;

    r = bme69x_init(bme);
    if (r != BME69X_OK) return r;

    return bme69x_set_conf(conf, bme);
}

static int sample_once(struct bme69x_dev *bme,
                       struct bme69x_conf *conf,
                       struct bme69x_heatr_conf *hc,
                       double *gas)
{
    struct bme69x_data d;
    uint8_t nf;
    uint32_t del;

    if (bme69x_set_heatr_conf(BME69X_FORCED_MODE, hc, bme) != BME69X_OK)
        return -1;

    if (bme69x_set_op_mode(BME69X_FORCED_MODE, bme) != BME69X_OK)
        return -1;

    del = bme69x_get_meas_dur(BME69X_FORCED_MODE, conf, bme)
        + hc->heatr_dur * 1000U;

    bme->delay_us(del, bme->intf_ptr);

    if (bme69x_get_data(BME69X_FORCED_MODE, &d, &nf, bme) != BME69X_OK || !nf)
        return -1;

    *gas = (double)d.gas_resistance;
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    struct bme69x_dev bme1, bme2;

    struct bme69x_conf conf = {
        .filter = BME69X_FILTER_OFF,
        .odr = BME69X_ODR_NONE,
        .os_hum = BME69X_OS_1X,
        .os_pres = BME69X_OS_1X,
        .os_temp = BME69X_OS_1X
    };

    init_sensor(&bme1, ADDR1, &conf);
    init_sensor(&bme2, ADDR2, &conf);

    struct bme69x_heatr_conf hc = {
        .enable = BME69X_ENABLE,
        .heatr_dur = HEATER_DUR_MS
    };

    double low1=0, high1=0, low2=0, high2=0;
    int lowc=0, highc=0;

    uint64_t t0 = monotonic_ms();
    uint64_t next = t0;
    uint64_t last_cycle = 0;

    printf("RATIO,t_ms,addr,value\n");

    while (1)
    {
        uint64_t now = monotonic_ms();
        uint64_t rel = now - t0;
        uint64_t cycle = rel / SQ_PERIOD_MS;
        int phase = rel % SQ_PERIOD_MS;

        uint16_t heater = (phase < HALF_MS) ? T_LOW_C : T_HIGH_C;
        hc.heatr_temp = heater;

        double g1=0, g2=0;
        int ok1 = (sample_once(&bme1, &conf, &hc, &g1) == 0);
        int ok2 = (sample_once(&bme2, &conf, &hc, &g2) == 0);

        if (heater == T_LOW_C)
        {
            if (ok1) { low1 += g1; lowc++; }
            if (ok2) { low2 += g2; }
        }
        else
        {
            if (ok1) { high1 += g1; highc++; }
            if (ok2) { high2 += g2; }
        }

        if (cycle != last_cycle)
        {
            last_cycle = cycle;

            if (lowc && highc)
            {
                double r1 = (high1/highc) / (low1/lowc);
                double r2 = (high2/highc) / (low2/lowc);

                printf("RATIO,%llu,0x76,%.6f\n", (unsigned long long)rel, r1);
                printf("RATIO,%llu,0x77,%.6f\n", (unsigned long long)rel, r2);
            }

            low1=high1=low2=high2=0;
            lowc=highc=0;
        }

        next += TS_MS;
        sleep_until_ms(next);
    }
}

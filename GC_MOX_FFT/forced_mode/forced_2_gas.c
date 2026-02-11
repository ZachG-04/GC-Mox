#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "bme69x.h"
#include "common.h"

#define ADDR1 0x76
#define ADDR2 0x77

#define HEATER_TEMP     250
#define HEATER_DUR_MS   100
#define SAMPLE_MS       200
#define WARMUP_SAMPLES  10

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

static int init_sensor(struct bme69x_dev *bme, uint8_t addr,
                       struct bme69x_conf *conf,
                       struct bme69x_heatr_conf *heatr_conf)
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
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, heatr_conf, bme);
    bme69x_check_rslt("bme69x_set_heatr_conf", rslt);
    return rslt;
}

static int sample_once(struct bme69x_dev *bme,
                       struct bme69x_conf *conf,
                       struct bme69x_heatr_conf *heatr_conf,
                       struct bme69x_data *out_data)
{
    int8_t rslt;
    uint8_t n_fields = 0;
    uint32_t del_period;

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, bme);
    if (rslt != BME69X_OK) return rslt;

    del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, conf, bme)
               + (heatr_conf->heatr_dur * 1000U);
    bme->delay_us(del_period, bme->intf_ptr);

    rslt = bme69x_get_data(BME69X_FORCED_MODE, out_data, &n_fields, bme);
    if (rslt != BME69X_OK) return rslt;

    return (n_fields > 0) ? BME69X_OK : BME69X_E_COM_FAIL;
}

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
    struct bme69x_dev bme1, bme2;
    int8_t rslt;

    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;

    /* Fast config */
    conf.filter  = BME69X_FILTER_OFF;
    conf.odr     = BME69X_ODR_NONE;
    conf.os_hum  = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_1X;

    /* Heater config */
    heatr_conf.enable     = BME69X_ENABLE;
    heatr_conf.heatr_temp = HEATER_TEMP;
    heatr_conf.heatr_dur  = HEATER_DUR_MS;

    /* Init both sensors */
    rslt = init_sensor(&bme1, ADDR1, &conf, &heatr_conf);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme1); return rslt; }

    rslt = init_sensor(&bme2, ADDR2, &conf, &heatr_conf);
    if (rslt != BME69X_OK) {
        bme69x_interface_deinit(&bme1);
        bme69x_interface_deinit(&bme2);
        return rslt;
    }

    uint64_t t0 = monotonic_ms();
    uint64_t next = t0;

    printf("t_ms,addr,gas_ohm,temp_C,hum_pct,press_Pa,status\n");
    fflush(stdout);

    int sample = 0;

    while (1)
    {
        struct bme69x_data d1, d2;

        int ok1 = (sample_once(&bme1, &conf, &heatr_conf, &d1) == BME69X_OK);
        int ok2 = (sample_once(&bme2, &conf, &heatr_conf, &d2) == BME69X_OK);

        uint64_t t_ms = monotonic_ms() - t0;
        sample++;

        if (sample > WARMUP_SAMPLES)
        {
#ifdef BME69X_USE_FPU
            if (ok1)
                printf("%llu,0x%02X,%.2f,%.2f,%.2f,%.2f,0x%x\n",
                       (unsigned long long)t_ms, ADDR1,
                       d1.gas_resistance, d1.temperature, d1.humidity, d1.pressure, d1.status);

            if (ok2)
                printf("%llu,0x%02X,%.2f,%.2f,%.2f,%.2f,0x%x\n",
                       (unsigned long long)t_ms, ADDR2,
                       d2.gas_resistance, d2.temperature, d2.humidity, d2.pressure, d2.status);
#else
            if (ok1)
                printf("%llu,0x%02X,%ld,%d,%ld,%ld,0x%x\n",
                       (unsigned long long)t_ms, ADDR1,
                       d1.gas_resistance, d1.temperature, d1.humidity, d1.pressure, d1.status);

            if (ok2)
                printf("%llu,0x%02X,%ld,%d,%ld,%ld,0x%x\n",
                       (unsigned long long)t_ms, ADDR2,
                       d2.gas_resistance, d2.temperature, d2.humidity, d2.pressure, d2.status);
#endif
            fflush(stdout);
        }

        next += (uint64_t)SAMPLE_MS;
        sleep_until_ms(next);
    }

    bme69x_interface_deinit(&bme1);
    bme69x_interface_deinit(&bme2);
    return 0;
}



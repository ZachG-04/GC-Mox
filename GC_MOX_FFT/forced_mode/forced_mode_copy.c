/**
 * BME690/BME69x forced mode - Raspberry Pi I2C
 * 2-step square wave (200C<->320C), subsamples per step,
 * hysteresis y = high - low per subsample,
 * FFT spans multiple cycles (rolling window), DC removed.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "bme69x.h"
#include "common.h"

#define FFT_STRIDE 10   /* print FFT every 10 cycles */

#define T_LOW_C       200
#define T_HIGH_C      320                                                                                                                            

/* Timing */
#define HALF_MS       1000     /* 1s at low + 1s at high => 2s cycle */
#define SUB_MS        50       /* sample every 50ms inside each step => Fs=20Hz, fmax=10Hz */
#define S             (HALF_MS / SUB_MS)   /* subsamples per step, here 20 */

/* FFT over multiple cycles */
#define FFT_CYCLES    16
#define FFT_N         (S * FFT_CYCLES)     /* 20*16 = 320-point FFT window */

#define WARMUP_CYCLES 2

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

static int sample_gas_once(struct bme69x_dev *bme,
                           struct bme69x_conf *conf,
                           struct bme69x_heatr_conf *heatr_conf,
                           double *gas_out)
{
    int8_t rslt;
    struct bme69x_data data;
    uint8_t n_fields;
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

    if (n_fields)
    {
        *gas_out = (double)data.gas_resistance;
        return BME69X_OK;
    }
    return BME69X_E_COM_FAIL;
}

/* DFT on length N, DC removed by mean subtraction, print bins 1..N/2 */
static void dft_print(uint32_t cycle_id, const double *x, int N, double Fs)
{
    double mean = 0.0;
    for (int i = 0; i < N; i++) mean += x[i];
    mean /= (double)N;

    printf("FFT,%lu,%.6f", (unsigned long)cycle_id, Fs);

    for (int k = 1; k <= N/2; k++)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; n++)
        {
            double xn = x[n] - mean;
            double ang = -2.0 * M_PI * (double)k * (double)n / (double)N;
            re += xn * cos(ang);
            im += xn * sin(ang);
        }
        re /= (double)N;
        im /= (double)N;

        double mag = sqrt(re*re + im*im);
        printf(",%.6f", mag);
    }
    printf("\n");
    fflush(stdout);
}

int main(void)
{
    struct bme69x_dev bme;
    int8_t rslt;

    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;

    /* step buffers */
    double low[S]  = {0};
    double high[S] = {0};
    double y_cycle[S] = {0};

    /* rolling FFT buffer over multiple cycles */
    double fft_buf[FFT_N] = {0};
    int fft_pos = 0;
    int fft_filled = 0;

    uint32_t cycle_id = 0;

    const double Fs = 1000.0 / (double)SUB_MS; /* 20Hz */

    /* Init I2C */
    rslt = bme69x_interface_init(&bme, BME69X_I2C_INTF);
    bme69x_check_rslt("bme69x_interface_init", rslt);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_init(&bme);
    bme69x_check_rslt("bme69x_init", rslt);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme); return rslt; }

    /* quick measurement config */
    conf.filter  = BME69X_FILTER_OFF;
    conf.odr     = BME69X_ODR_NONE;
    conf.os_hum  = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_1X;

    rslt = bme69x_set_conf(&conf, &bme);
    bme69x_check_rslt("bme69x_set_conf", rslt);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme); return rslt; }

    heatr_conf.enable    = BME69X_ENABLE;
    heatr_conf.heatr_dur = 10; /* ms, keep small compared to SUB_MS */

    fprintf(stderr,
            "2-step %dC<->%dC | HALF_MS=%d | SUB_MS=%d => S=%d | FFT_N=%d | Fs=%.2fHz | fmax=%.2fHz\n",
            T_LOW_C, T_HIGH_C, HALF_MS, SUB_MS, S, FFT_N, Fs, Fs/2.0);

    while (1)
    {
        uint64_t t_start = monotonic_ms();
        uint64_t next = t_start;

        /* -------- LOW step: collect S samples at fixed spacing -------- */
        heatr_conf.heatr_temp = T_LOW_C;
        for (int i = 0; i < S; i++)
        {
            double g = 0.0;
            rslt = sample_gas_once(&bme, &conf, &heatr_conf, &g);
            if (rslt != BME69X_OK) goto out;
            low[i] = g;

            next += (uint64_t)SUB_MS;
            sleep_until_ms(next);
        }

        /* -------- HIGH step: collect S samples at fixed spacing -------- */
        heatr_conf.heatr_temp = T_HIGH_C;
        for (int i = 0; i < S; i++)
        {
            double g = 0.0;
            rslt = sample_gas_once(&bme, &conf, &heatr_conf, &g);
            if (rslt != BME69X_OK) goto out;
            high[i] = g;

            next += (uint64_t)SUB_MS;
            sleep_until_ms(next);
        }

        cycle_id++;

        /* -------- per-cycle hysteresis samples -------- */
        printf("FEATURE_CYCLE,%lu", (unsigned long)cycle_id);
        for (int i = 0; i < S; i++)
        {
            y_cycle[i] = high[i] - low[i];
            printf(",%.6f", y_cycle[i]);
        }
        printf("\n");
        fflush(stdout);

        /* -------- append to rolling FFT buffer -------- */
        for (int i = 0; i < S; i++)
        {
            fft_buf[fft_pos] = y_cycle[i];
            fft_pos = (fft_pos + 1) % FFT_N;
            if (!fft_filled && fft_pos == 0) fft_filled = 1;
        }

        /* -------- FFT over multiple cycles -------- */
        if (cycle_id > WARMUP_CYCLES && fft_filled && (cycle_id % FFT_STRIDE == 0))
{
        double x[FFT_N];
        for (int i = 0; i < FFT_N; i++)
            x[i] = fft_buf[(fft_pos + i) % FFT_N]; /* oldest->newest */

        dft_print(cycle_id, x, FFT_N, Fs);
}
    }

out:
    bme69x_interface_deinit(&bme);
    return rslt;
}



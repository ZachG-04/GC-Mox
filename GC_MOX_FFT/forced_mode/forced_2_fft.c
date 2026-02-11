/**
 * forced_2_fft.c
 *
 * Dual BME69x (0x76 + 0x77) forced-mode sampling with heater square wave modulation
 * and FFT (DFT magnitudes) every 2 seconds.
 *
 * Modulation:
 *   T_LOW=275C, T_HIGH=325C
 *   Square wave period = 200ms, half-period=100ms  -> 5 Hz modulation
 *
 * Sampling:
 *   Ts = 50ms (4 samples per 200ms wave) -> Fs = 20Hz, Nyquist=10Hz
 *
 * FFT window:
 *   2 seconds -> N = 40 samples
 *   Prints magnitudes for k=0..N/2 (0..10Hz in 0.5Hz steps)
 *
 * Output:
 *   FFT,t_ms,addr,Fs,mag0,mag1,...,mag20
 *   PEAK,t_ms,addr,f1,mag1,f2,mag2,f3,mag3     (top 3 peaks excluding DC)
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "bme69x.h"
#include "common.h"

/* ---------- Addresses ---------- */
#define ADDR1 0x76
#define ADDR2 0x77

/* ---------- Heater modulation ---------- */
#define T_LOW_C     275
#define T_HIGH_C    325

#define T_SW_MS     200     /* square wave period */
#define T_HALF_MS   100     /* half period */

/* ---------- Sampling / FFT ---------- */
#define TS_MS       50                  /* 4 samples per 200ms wave */
#define FS_HZ       (1000.0 / TS_MS)    /* 20 Hz */

#define FFT_N       40                  /* 2 seconds of data: 2000ms / 50ms = 40 */
#define FFT_BINS    (FFT_N/2 + 1)       /* 21 bins: k=0..20 */

/* Warmup windows to skip FFT printing */
#define WARMUP_WINDOWS  2

/* Debug raw output */
#define PRINT_RAW   0

/* Heater duration: keep << TS_MS so timing works */
#define HEATER_DUR_MS  10

/* ---------- Time helpers ---------- */
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

/* ---------- Sensor init/sample ---------- */
static int init_sensor(struct bme69x_dev *bme, uint8_t addr,
                       struct bme69x_conf *conf)
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
                       struct bme69x_data *out_data)
{
    int8_t rslt;
    uint8_t n_fields = 0;
    uint32_t del_period;

    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, heatr_conf, bme);
    if (rslt != BME69X_OK) return rslt;

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, bme);
    if (rslt != BME69X_OK) return rslt;

    del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, conf, bme)
               + (heatr_conf->heatr_dur * 1000U);

    bme->delay_us(del_period, bme->intf_ptr);

    rslt = bme69x_get_data(BME69X_FORCED_MODE, out_data, &n_fields, bme);
    if (rslt != BME69X_OK) return rslt;

    return (n_fields > 0) ? BME69X_OK : BME69X_E_COM_FAIL;
}

/* ---------- DFT magnitude (DC removed via mean subtraction) ---------- */
static void dft_mags_dc_removed(const double *x, int N, double *mags_out)
{
    double mean = 0.0;
    for (int i = 0; i < N; i++) mean += x[i];
    mean /= (double)N;

    for (int k = 0; k <= N/2; k++)
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

        mags_out[k] = sqrt(re*re + im*im);
    }
}

static void top3_peaks(const double *mags, int bins, double fs, int N,
                       double *f1, double *m1,
                       double *f2, double *m2,
                       double *f3, double *m3)
{
    /* exclude DC => start at k=1 */
    int k1 = 1, k2 = 1, k3 = 1;
    double a1 = -1.0, a2 = -1.0, a3 = -1.0;

    for (int k = 1; k < bins; k++)
    {
        double a = mags[k];
        if (a > a1) { a3=a2; k3=k2; a2=a1; k2=k1; a1=a; k1=k; }
        else if (a > a2) { a3=a2; k3=k2; a2=a; k2=k; }
        else if (a > a3) { a3=a; k3=k; }
    }

    *f1 = (double)k1 * fs / (double)N; *m1 = a1;
    *f2 = (double)k2 * fs / (double)N; *m2 = a2;
    *f3 = (double)k3 * fs / (double)N; *m3 = a3;
}

/* ---------- Main ---------- */
int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0); /* line-buffered stdout */

    struct bme69x_dev bme1, bme2;
    int8_t rslt;

    /* Fast measurement config */
    struct bme69x_conf conf;
    conf.filter  = BME69X_FILTER_OFF;
    conf.odr     = BME69X_ODR_NONE;
    conf.os_hum  = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_1X;

    /* Init sensors */
    rslt = init_sensor(&bme1, ADDR1, &conf);
    if (rslt != BME69X_OK) { bme69x_interface_deinit(&bme1); return rslt; }

    rslt = init_sensor(&bme2, ADDR2, &conf);
    if (rslt != BME69X_OK) {
        bme69x_interface_deinit(&bme1);
        bme69x_interface_deinit(&bme2);
        return rslt;
    }

    /* Heater config (temp will be updated each sample) */
    struct bme69x_heatr_conf heatr_conf;
    heatr_conf.enable    = BME69X_ENABLE;
    heatr_conf.heatr_dur = HEATER_DUR_MS;

#if PRINT_RAW
    printf("t_ms,addr,gas_ohm,temp_C,hum_pct,press_Pa,status\n");
#endif

    fprintf(stderr,
            "Dual FFT setup: Ts=%dms Fs=%.2fHz | square=%dms (half=%dms) | N=%d (2s) | bins=%d | Nyq=%.2fHz\n",
            TS_MS, FS_HZ, T_SW_MS, T_HALF_MS, FFT_N, FFT_BINS, FS_HZ/2.0);

    /* Rolling buffers (we compute FFT every full window, no overlap) */
    double x1[FFT_N] = {0};
    double x2[FFT_N] = {0};
    int idx = 0;
    uint32_t window_id = 0;

    uint64_t t0 = monotonic_ms();
    uint64_t next_tick = t0;

    while (1)
    {
        /* Time within square-wave period decides heater temp */
        uint64_t now_ms = monotonic_ms();
        uint64_t rel_ms = now_ms - t0;
        int phase = (int)(rel_ms % T_SW_MS);
        uint16_t heater_temp = (phase < T_HALF_MS) ? T_LOW_C : T_HIGH_C;

        /* Sample both sensors using the same heater temp this tick */
        struct bme69x_data d1, d2;

        heatr_conf.heatr_temp = heater_temp;
        int ok1 = (sample_once(&bme1, &conf, &heatr_conf, &d1) == BME69X_OK);
        int ok2 = (sample_once(&bme2, &conf, &heatr_conf, &d2) == BME69X_OK);

        uint64_t t_ms = monotonic_ms() - t0;

        if (ok1) x1[idx] = (double)d1.gas_resistance;
        else     x1[idx] = x1[(idx > 0) ? (idx - 1) : 0]; /* hold last */

        if (ok2) x2[idx] = (double)d2.gas_resistance;
        else     x2[idx] = x2[(idx > 0) ? (idx - 1) : 0];

#if PRINT_RAW
        if (ok1) {
#ifdef BME69X_USE_FPU
            printf("%llu,0x%02X,%.2f,%.2f,%.2f,%.2f,0x%x\n",
                   (unsigned long long)t_ms, ADDR1,
                   d1.gas_resistance, d1.temperature, d1.humidity, d1.pressure, d1.status);
#else
            printf("%llu,0x%02X,%ld,%d,%ld,%ld,0x%x\n",
                   (unsigned long long)t_ms, ADDR1,
                   d1.gas_resistance, d1.temperature, d1.humidity, d1.pressure, d1.status);
#endif
        }
        if (ok2) {
#ifdef BME69X_USE_FPU
            printf("%llu,0x%02X,%.2f,%.2f,%.2f,%.2f,0x%x\n",
                   (unsigned long long)t_ms, ADDR2,
                   d2.gas_resistance, d2.temperature, d2.humidity, d2.pressure, d2.status);
#else
            printf("%llu,0x%02X,%ld,%d,%ld,%ld,0x%x\n",
                   (unsigned long long)t_ms, ADDR2,
                   d2.gas_resistance, d2.temperature, d2.humidity, d2.pressure, d2.status);
#endif
        }
#endif

        idx++;

        /* FFT every 2 seconds (every N samples) */
        if (idx >= FFT_N)
        {
            idx = 0;
            window_id++;

            if (window_id > WARMUP_WINDOWS)
            {
                double mags1[FFT_BINS], mags2[FFT_BINS];
                dft_mags_dc_removed(x1, FFT_N, mags1);
                dft_mags_dc_removed(x2, FFT_N, mags2);

                /* Print full spectra */
                printf("FFT,%llu,0x%02X,%.6f", (unsigned long long)t_ms, ADDR1, FS_HZ);
                for (int k = 0; k < FFT_BINS; k++) printf(",%.6f", mags1[k]);
                printf("\n");

                printf("FFT,%llu,0x%02X,%.6f", (unsigned long long)t_ms, ADDR2, FS_HZ);
                for (int k = 0; k < FFT_BINS; k++) printf(",%.6f", mags2[k]);
                printf("\n");

                /* Print top-3 peaks (excluding DC) */
                double f1,m1,f2,m2,f3,m3;

                top3_peaks(mags1, FFT_BINS, FS_HZ, FFT_N, &f1,&m1,&f2,&m2,&f3,&m3);
                printf("PEAK,%llu,0x%02X,%.3f,%.6f,%.3f,%.6f,%.3f,%.6f\n",
                       (unsigned long long)t_ms, ADDR1, f1,m1, f2,m2, f3,m3);

                top3_peaks(mags2, FFT_BINS, FS_HZ, FFT_N, &f1,&m1,&f2,&m2,&f3,&m3);
                printf("PEAK,%llu,0x%02X,%.3f,%.6f,%.3f,%.6f,%.3f,%.6f\n",
                       (unsigned long long)t_ms, ADDR2, f1,m1, f2,m2, f3,m3);
            }
        }

        /* Maintain fixed sample rate */
        next_tick += (uint64_t)TS_MS;
        sleep_until_ms(next_tick);
    }

    /* not reached */
    bme69x_interface_deinit(&bme1);
    bme69x_interface_deinit(&bme2);
    return 0;
}

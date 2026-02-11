#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define I2C_DEV_PATH "/dev/i2c-1"

/* shared fd for bus */
static int shared_fd = -1;

/* ---------------- delay ---------------- */
static void delay_us_pi(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    usleep(period);
}

/* ---------------- i2c select addr ---------------- */
static int select_addr(struct pi_i2c_ctx *ctx)
{
    if (!ctx || ctx->fd < 0)
        return -1;

    if (ioctl(ctx->fd, I2C_SLAVE, ctx->addr) < 0)
    {
        perror("ioctl(I2C_SLAVE)");
        return -1;
    }
    return 0;
}

/* ---------------- read ---------------- */
static int8_t i2c_read_pi(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct pi_i2c_ctx *ctx = (struct pi_i2c_ctx *)intf_ptr;
    if (!ctx || ctx->fd < 0 || !reg_data || len == 0)
        return BME69X_E_NULL_PTR;

    if (select_addr(ctx) < 0)
        return BME69X_E_COM_FAIL;

    if (write(ctx->fd, &reg_addr, 1) != 1)
        return BME69X_E_COM_FAIL;

    ssize_t r = read(ctx->fd, reg_data, len);
    if (r != (ssize_t)len)
        return BME69X_E_COM_FAIL;

    return BME69X_OK;
}

/* ---------------- write ---------------- */
static int8_t i2c_write_pi(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct pi_i2c_ctx *ctx = (struct pi_i2c_ctx *)intf_ptr;
    if (!ctx || ctx->fd < 0)
        return BME69X_E_NULL_PTR;

    if (select_addr(ctx) < 0)
        return BME69X_E_COM_FAIL;

    if (len > 256)
        return BME69X_E_INVALID_LENGTH;

    uint8_t buf[1 + 256];
    buf[0] = reg_addr;
    if (len && reg_data)
        memcpy(&buf[1], reg_data, len);

    ssize_t w = write(ctx->fd, buf, 1 + len);
    if (w != (ssize_t)(1 + len))
        return BME69X_E_COM_FAIL;

    return BME69X_OK;
}

/* ---------------- init interface ---------------- */
int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t i2c_addr)
{
    if (!bme)
        return BME69X_E_NULL_PTR;

    if (intf != BME69X_I2C_INTF)
        return BME69X_E_INVALID_LENGTH;

    /* open shared fd once */
    if (shared_fd < 0)
    {
        shared_fd = open(I2C_DEV_PATH, O_RDWR);
        if (shared_fd < 0)
        {
            perror("open(/dev/i2c-1)");
            return BME69X_E_COM_FAIL;
        }
    }

    /* allocate per-device ctx */
    struct pi_i2c_ctx *ctx = (struct pi_i2c_ctx *)malloc(sizeof(struct pi_i2c_ctx));
    if (!ctx)
        return BME69X_E_COM_FAIL;

    ctx->fd = shared_fd;
    ctx->addr = i2c_addr;

    /* Fill bme struct */
    bme->intf = BME69X_I2C_INTF;
    bme->read = i2c_read_pi;
    bme->write = i2c_write_pi;
    bme->delay_us = delay_us_pi;
    bme->intf_ptr = ctx;
    bme->amb_temp = 25;

    return BME69X_OK;
}

/* ---------------- deinit ---------------- */
void bme69x_interface_deinit(struct bme69x_dev *bme)
{
    if (!bme)
        return;

    if (bme->intf_ptr)
    {
        free(bme->intf_ptr);
        bme->intf_ptr = NULL;
    }

    /* NOTE: we keep shared_fd open until program exits
       because both sensors depend on it.
       can add a refcount system, but not needed. */
}

/* ---------------- print errors ---------------- */
void bme69x_check_rslt(const char api_name[], int8_t rslt)
{
    if (rslt != BME69X_OK)
        fprintf(stderr, "%s failed: %d\n", api_name, rslt);
}

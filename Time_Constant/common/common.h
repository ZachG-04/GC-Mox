#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include "bme69x.h"

/* Pi I2C context (one per sensor) */
struct pi_i2c_ctx
{
    int fd;
    uint8_t addr;
};

/* Init the interface for a sensor */
int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t i2c_addr);

/* Close I2C fd (safe to call multiple times) */
void bme69x_interface_deinit(struct bme69x_dev *bme);

/* Print error codes */
void bme69x_check_rslt(const char api_name[], int8_t rslt);

#endif

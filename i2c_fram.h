#ifndef I2C_FRAM_H
#define I2C_FRAM_H

#include <config/i2c_fram_config.h>

typedef enum i2c_fram_op {
    I2C_FRAM_OP_INVALID = 0,
    I2C_FRAM_OP_WRITE,
    I2C_FRAM_OP_READ,
    I2C_FRAM_OP_MAX
} i2c_fram_op;

/**
 * \brief Structure to hold information about individual i2c fram memory.
 *
 * \param i2c_address The i2c bus address of the i2c memory chip
 * \param initialized Memory chip intialized / tested
 * \param size Total size of the memory in bytes
 * \param prev Pointer to the structure of the previous memory chip
 * \param next Pointer to the structure of the next memory chip
 * \param wp_gpio_handler Pointer to the function that can be used to manipulate the wp pin of the fram
 */
typedef struct i2c_fram {
    uint8_t i2c_address;
    uint8_t initialized;
    uint8_t resv[2];
    uint32_t size;
    void * prev;
    void * next;
    void (*wp_gpio_handler)(uint8_t);
} i2c_fram_s;

int32_t i2c_fram_register(i2c_fram_s * fram);
int32_t i2c_fram_init(void);
int32_t i2c_fram_read_write(uint32_t address, uint8_t op, uint8_t * bytes, uint32_t len);
uint8_t i2c_fram_test_iterate(void);

#endif /* I2C_FRAM_H */

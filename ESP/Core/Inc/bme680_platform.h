#ifndef INC_BME680_PLATFORM_H_
#define INC_BME680_PLATFORM_H_

#include "bme680.h"
#include "stm32g0xx_hal.h"

int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len);
int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, const uint8_t *data, uint16_t len);
void user_delay_ms(uint32_t period);

#endif /* INC_BME680_PLATFORM_H_ */

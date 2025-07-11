//#include "bme680_platform.h"
//
//extern I2C_HandleTypeDef hi2c1;
//
//int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr,
//                     uint8_t *data, uint16_t len) {
//    return HAL_I2C_Mem_Read(&hi2c1, dev_id << 1, reg_addr,
//                            I2C_MEMADD_SIZE_8BIT, data, len, 100);
//}
//
//int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr,
//                      const uint8_t *data, uint16_t len) {
//    return HAL_I2C_Mem_Write(&hi2c1, dev_id << 1, reg_addr,
//                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, 100);
//}
//
//void user_delay_ms(uint32_t period) {
//    HAL_Delay(period);
//}

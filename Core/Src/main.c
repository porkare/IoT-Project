/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body with BME68x + UART CLI + E22 LoRa
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "bme68x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2; // CLI (first Putty)
UART_HandleTypeDef huart1; // LoRa (second Putty)

struct bme68x_dev gas_sensor;
uint16_t bme68x_i2c_addr = 0x76;

#define E22_M0_Pin GPIO_PIN_3
#define E22_M0_GPIO_Port GPIOC

#define E22_M1_Pin GPIO_PIN_8
#define E22_M1_GPIO_Port GPIOC

#define E22_AUX_Pin GPIO_PIN_2
#define E22_AUX_GPIO_Port GPIOC

/* Function prototypes ---------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
void BME68x_Init(void);
void Show_Help_Menu(void);
void Show_Temperature(void);
void Show_Humidity(void);
void Show_Pressure(void);
void Show_All_Sensors(void);
void Do_Sum(char *args);
void Error_Handler(void);
void E22_SetMode(uint8_t m0, uint8_t m1);
void Wait_For_AUX_High(uint32_t timeout_ms);
void Send_LoRa_Command(uint8_t *cmd, uint16_t len);
uint8_t Read_LoRa_Register(uint8_t addr);
void Write_LoRa_Register(uint8_t addr, uint8_t value);
void Read_LoRa_Config(void);
void Send_LoRa_Config(void);
void Set_LoRa_Channel(uint8_t channel);
void Prompt_Set_LoRa_Channel(void);
void Enable_LoRa_Relay(void);
void Disable_LoRa_Relay(void);
void Send_LoRa_Message(char *msg);
void Send_LoRa_All_Sensors(void);

/* printf redirection to UART2 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* GPIO mode switcher */
void E22_SetMode(uint8_t m0, uint8_t m1) {
    HAL_GPIO_WritePin(E22_M0_GPIO_Port, E22_M0_Pin, m0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(E22_M1_GPIO_Port, E22_M1_Pin, m1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(50);
    Wait_For_AUX_High(1000); // Ensure mode switch completed
}

/* I2C read/write/delay functions for BME68x */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint16_t dev_addr = *(uint16_t *)intf_ptr;
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, dev_addr << 1, reg_addr,
                                                I2C_MEMADD_SIZE_8BIT, reg_data, len, 100);
    return (status == HAL_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint16_t dev_addr = *(uint16_t *)intf_ptr;
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, dev_addr << 1, reg_addr,
                                                 I2C_MEMADD_SIZE_8BIT, (uint8_t *)reg_data, len, 100);
    return (status == HAL_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

void user_delay_us(uint32_t period, void *intf_ptr) {
    HAL_Delay(period / 1000); // Convert microseconds to milliseconds
}

/* Send ASCII or binary command to LoRa */
void Send_LoRa_Command(uint8_t *cmd, uint16_t len) {
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, cmd, len, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        printf("[ERROR] Failed to send LoRa command\r\n");
    }
}

/* Send text message via LoRa */
void Send_LoRa_Message(char *msg) {
    E22_SetMode(0, 0); // Normal mode
    HAL_Delay(50);

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    if (status == HAL_OK) {
        printf("[SUCCESS] Message sent via LoRa: %s", msg);
    } else {
        printf("[ERROR] Failed to send LoRa message\r\n");
    }
}

/* Read a single register from LoRa */
uint8_t Read_LoRa_Register(uint8_t addr) {
    uint8_t cmd[3] = {0xC1, addr, 0x01};
    uint8_t resp[4];

    Wait_For_AUX_High(1000);
    E22_SetMode(0, 1); // Config mode

    __HAL_UART_FLUSH_DRREGISTER(&huart1);

    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), HAL_MAX_DELAY);
    Wait_For_AUX_High(1000);

    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart1, resp, sizeof(resp), 1000);
    E22_SetMode(0, 0); // Back to normal

    if (rx_status == HAL_OK && resp[0] == 0xC1 && resp[1] == addr && resp[2] == 0x01) {
        return resp[3];
    } else {
        printf("[ERROR] Failed to read register 0x%02X\r\n", addr);
        return 0xFF; // Error value
    }
}

/* Write a single register to LoRa */
void Write_LoRa_Register(uint8_t addr, uint8_t value) {
    uint8_t cmd[4] = {0xC0, addr, 0x01, value};
    uint8_t resp[4];

    Wait_For_AUX_High(1000);
    E22_SetMode(0, 1); // Config mode

    __HAL_UART_FLUSH_DRREGISTER(&huart1);

    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), HAL_MAX_DELAY);
    Wait_For_AUX_High(1000);

    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart1, resp, sizeof(resp), 1000);
    E22_SetMode(0, 0); // Back to normal

    if (rx_status == HAL_OK && resp[0] == 0xC1 && resp[1] == addr && resp[3] == value) {
        printf("[SUCCESS] Register 0x%02X set to 0x%02X\r\n", addr, value);
    } else {
        printf("[ERROR] Failed to write register 0x%02X\r\n", addr);
    }
}

/* Enable relay on LoRa */
void Enable_LoRa_Relay(void) {
    uint8_t reg3 = Read_LoRa_Register(0x06);
    if (reg3 != 0xFF) {
        reg3 |= (1 << 5); // Set bit 5
        Write_LoRa_Register(0x06, reg3);
    }
}

/* Disable relay on LoRa */
void Disable_LoRa_Relay(void) {
    uint8_t reg3 = Read_LoRa_Register(0x06);
    if (reg3 != 0xFF) {
        reg3 &= ~(1 << 5); // Clear bit 5
        Write_LoRa_Register(0x06, reg3);
    }
}

/* Set LoRa channel with given value */
void Set_LoRa_Channel(uint8_t channel) {
    uint8_t cmd[4] = {0xC0, 0x05, 0x01, channel};
    uint8_t resp[4];

    printf("\r\n[DEBUG] Setting CH to 0x%02X...\r\n", channel);

    Wait_For_AUX_High(1000);
    E22_SetMode(0, 1); // Config mode

    __HAL_UART_FLUSH_DRREGISTER(&huart1);

    HAL_StatusTypeDef tx_status = HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), HAL_MAX_DELAY);
    if (tx_status != HAL_OK) {
        printf("[ERROR] Failed to send command\r\n");
        E22_SetMode(0, 0);
        return;
    }

    Wait_For_AUX_High(1000);

    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart1, resp, sizeof(resp), 1000);
    if (rx_status == HAL_OK) {
        printf("[DEBUG] Response: ");
        for (int i = 0; i < sizeof(resp); i++) {
            printf("0x%02X ", resp[i]);
        }
        printf("\r\n");

        if (resp[0] == 0xC1 && resp[1] == 0x05 && resp[3] == channel) {
            printf("[SUCCESS] Channel successfully set to 0x%02X.\r\n", channel);
        } else {
            printf("[WARN] Unexpected response.\r\n");
        }
    } else {
        printf("[ERROR] No response. Status: %d\r\n", rx_status);
    }

    E22_SetMode(0, 0); // Back to normal
    HAL_Delay(50);
}

/* Prompt user for channel and set it */
void Prompt_Set_LoRa_Channel(void) {
    char input[10];
    uint8_t idx = 0;
    uint8_t rx_char;

    printf("Enter channel (0-83 for 400MHz band): ");
    memset(input, 0, sizeof(input));

    while (1) {
        if (HAL_UART_Receive(&huart2, &rx_char, 1, 100) == HAL_OK) {
            if (rx_char == '\r' || rx_char == '\n') {
                input[idx] = '\0';
                break;
            }
            if (idx < sizeof(input) - 1) {
                input[idx++] = rx_char;
                HAL_UART_Transmit(&huart2, &rx_char, 1, HAL_MAX_DELAY);
            }
        }
    }
    printf("\r\n");

    int ch = atoi(input);
    if (ch >= 0 && ch <= 83) {
        Set_LoRa_Channel((uint8_t)ch);
    } else {
        printf("[ERROR] Invalid channel range.\r\n");
    }
}

/* Read LoRa module config (returns 12-byte response) and parse */
void Read_LoRa_Config(void) {
    uint8_t read_cmd[] = { 0xC1, 0x00, 0x09 }; // Read all 9 registers
    uint8_t response[12];

    printf("\r\n[DEBUG] Setting E22 to CONFIG mode (M1=1, M0=0)...\r\n");

    Wait_For_AUX_High(1000); // Check PC2
    E22_SetMode(0, 1);       // Config mode (M0=0, M1=1)
    Wait_For_AUX_High(1000);

    GPIO_PinState m0 = HAL_GPIO_ReadPin(E22_M0_GPIO_Port, E22_M0_Pin);
    GPIO_PinState m1 = HAL_GPIO_ReadPin(E22_M1_GPIO_Port, E22_M1_Pin);
    printf("[DEBUG] GPIO Readback: M0 = %d, M1 = %d\r\n", m0, m1);

    __HAL_UART_FLUSH_DRREGISTER(&huart1);

    printf("[DEBUG] Sending 0xC1 00 0x09 to LoRa...\r\n");
    HAL_StatusTypeDef tx_status = HAL_UART_Transmit(&huart1, read_cmd, sizeof(read_cmd), 500);
    if (tx_status != HAL_OK) {
        printf("[ERROR] Transmit failed! Status: %d\r\n", tx_status);
        goto cleanup;
    }

    HAL_Delay(10);
    Wait_For_AUX_High(1000); // Wait for PC6 to signal response ready

    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart1, response, sizeof(response), 1000);
    if (rx_status == HAL_OK) {
        printf("\r\n--- LoRa Raw Hex Response ---\r\n");
        for (int i = 0; i < sizeof(response); i++) {
            printf("Byte[%02d]: 0x%02X\r\n", i, response[i]);
        }
        if (response[0] == 0xC1 && response[1] == 0x00 && response[2] == 0x09) {
            // Parse the config bytes (response[3] to response[11])
            uint8_t addh = response[3];
            uint8_t addl = response[4];
            uint8_t netid = response[5];
            uint8_t reg0 = response[6];
            uint8_t reg1 = response[7];
            uint8_t chan = response[8];
            uint8_t reg3 = response[9];
            uint8_t crypt_h = response[10];
            uint8_t crypt_l = response[11];

            printf("\r\n--- Parsed LoRa Configuration ---\r\n");
            printf("Module Address: 0x%02X%02X\r\n", addh, addl);
            printf("NETID: %d (0x%02X)\r\n", netid, netid);

            // REG0 (0x03)
            uint8_t uart_baud_bits = (reg0 >> 5) & 0x07;
            const int uart_bauds[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
            printf("UART Baud Rate: %d bps\r\n", uart_bauds[uart_baud_bits]);

            uint8_t parity_bits = (reg0 >> 3) & 0x03;
            const char *parities[] = {"8N1", "8O1", "8E1", "8N1"};
            printf("UART Parity: %s\r\n", parities[parity_bits]);

            uint8_t air_rate_bits = reg0 & 0x07;
            const int air_rates[] = {2400, 2400, 2400, 4800, 9600, 19200, 38400, 62500}; // For 400/900T
            printf("Air Data Rate: %d bps\r\n", air_rates[air_rate_bits]);

            // REG1 (0x04)
            uint8_t subpkt_bits = (reg1 >> 6) & 0x03;
            const int subpkts[] = {240, 128, 64, 32};
            printf("Subpacket Size: %d bytes\r\n", subpkts[subpkt_bits]);

            printf("RSSI Ambient Noise: %s\r\n", (reg1 & (1<<5)) ? "Enabled" : "Disabled");
            printf("Software Mode Switching: %s\r\n", (reg1 & (1<<2)) ? "Enabled" : "Disabled");

            uint8_t tx_power_bits = reg1 & 0x03;
            const int tx_powers[] = {22, 17, 13, 10};
            printf("TX Power: %d dBm\r\n", tx_powers[tx_power_bits]);

            // REG2 (0x05)
            printf("Channel: %d (Frequency: %.3f MHz)\r\n", chan, 410.125 + chan * 1.0); // Assume 400MHz band

            // REG3 (0x06)
            printf("RSSI Byte: %s\r\n", (reg3 & (1<<7)) ? "Enabled" : "Disabled");
            printf("Transmission Mode: %s\r\n", (reg3 & (1<<6)) ? "Fixed Point" : "Transparent");
            printf("Repeater Mode: %s\r\n", (reg3 & (1<<5)) ? "Enabled" : "Disabled");
            printf("LBT: %s\r\n", (reg3 & (1<<4)) ? "Enabled" : "Disabled");
            printf("WOR Control: %s\r\n", (reg3 & (1<<3)) ? "Transmitter" : "Receiver");

            uint8_t wor_cycle_bits = reg3 & 0x07;
            printf("WOR Cycle: %d ms\r\n", 500 + wor_cycle_bits * 500);

            // CRYPT (0x07-0x08)
            uint16_t crypt_key = (crypt_h << 8) | crypt_l;
            printf("Crypt Key: 0x%04X\r\n", crypt_key);

            printf("--------------------------------\r\n");
        } else {
            printf("[WARN] Invalid response header!\r\n");
        }
    } else {
        printf("\r\n[ERROR] Failed to receive config response. Status: %d\r\n", rx_status);
    }

cleanup:
    E22_SetMode(0, 0);
    Wait_For_AUX_High(1000);
    HAL_Delay(50);
}

/* Send LoRa All Sensors as text */
void Send_LoRa_All_Sensors(void) {
    struct bme68x_data data;
    uint8_t n_fields;
    int8_t rslt;

    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("\r\n[ERROR] Failed to set operation mode\r\n");
        return;
    }

    HAL_Delay(200); // Wait for measurement

    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &gas_sensor);
    if (rslt == BME68X_OK && n_fields) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                "Temp: %.2f C, Hum: %.2f %%, Pres: %.2f hPa\r\n",
                data.temperature, data.humidity, data.pressure / 100.0f);

        printf("%s", msg);
        Send_LoRa_Message(msg);
    } else {
        printf("\r\n[ERROR] Failed to read sensors. Result: %d, Fields: %d\r\n", rslt, n_fields);
    }
}

/* Send ASCII command (AT+CFG) to LoRa */
void Send_LoRa_Config(void) {
    uint8_t config_cmd[] = {0xC0, 0x00, 0x09, 0x00, 0x00, 0x00, 0x62, 0x00, 0x17, 0x03, 0x00, 0x00}; // Example defaults; adjust per needs
    uint8_t response[12];

    printf("\r\n[DEBUG] Setting E22 to CONFIG mode...\r\n");
    Wait_For_AUX_High(1000);
    E22_SetMode(0, 1);

    HAL_UART_Transmit(&huart1, config_cmd, sizeof(config_cmd), HAL_MAX_DELAY);
    Wait_For_AUX_High(1000);

    HAL_StatusTypeDef rx_status = HAL_UART_Receive(&huart1, response, sizeof(response), 1000);
    if (rx_status == HAL_OK) {
        printf("[SUCCESS] Config set. Response: ");
        for (int i = 0; i < sizeof(response); i++) {
            printf("0x%02X ", response[i]);
        }
        printf("\r\n");
    } else {
        printf("[ERROR] No response from config write.\r\n");
    }

    E22_SetMode(0, 0);
    HAL_Delay(50);
}

/* BME68x Initialization */
void BME68x_Init(void) {
    gas_sensor.intf = BME68X_I2C_INTF;
    gas_sensor.read = user_i2c_read;
    gas_sensor.write = user_i2c_write;
    gas_sensor.delay_us = user_delay_us;
    gas_sensor.intf_ptr = &bme68x_i2c_addr;
    gas_sensor.amb_temp = 25;

    int8_t rslt = bme68x_init(&gas_sensor);
    if (rslt != BME68X_OK) {
        printf("[ERROR] BME68x init failed with code %d\r\n", rslt);

        // Try alternate address
        bme68x_i2c_addr = 0x77;
        rslt = bme68x_init(&gas_sensor);
        if (rslt != BME68X_OK) {
            printf("[ERROR] BME68x init failed on alternate address too\r\n");
            Error_Handler();
        } else {
            printf("[INFO] BME68x found at address 0x77\r\n");
        }
    } else {
        printf("[INFO] BME68x initialized successfully at address 0x76\r\n");
    }

    struct bme68x_conf conf = {
        .os_hum = BME68X_OS_2X,
        .os_pres = BME68X_OS_4X,
        .os_temp = BME68X_OS_8X,
        .filter = BME68X_FILTER_OFF,
        .odr = BME68X_ODR_NONE
    };
    rslt = bme68x_set_conf(&conf, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("[ERROR] Failed to set BME68x config\r\n");
    }

    struct bme68x_heatr_conf heatr_conf = {
        .enable = BME68X_ENABLE,
        .heatr_temp = 320,
        .heatr_dur = 150
    };
    rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("[ERROR] Failed to set BME68x heater config\r\n");
    }
}

void Show_Temperature(void) {
    struct bme68x_data data;
    uint8_t n_fields;
    int8_t rslt;

    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("\r\n[ERROR] Failed to set operation mode\r\n");
        return;
    }

    HAL_Delay(200); // Wait for measurement

    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &gas_sensor);
    if (rslt == BME68X_OK && n_fields) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nTemperature: %.2f °C\r\n", data.temperature);
        printf("%s", msg);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    } else {
        printf("\r\n[ERROR] Failed to read temperature. Result: %d, Fields: %d\r\n", rslt, n_fields);
    }
}

void Show_Humidity(void) {
    struct bme68x_data data;
    uint8_t n_fields;
    int8_t rslt;

    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("\r\n[ERROR] Failed to set operation mode\r\n");
        return;
    }

    HAL_Delay(200); // Wait for measurement

    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &gas_sensor);
    if (rslt == BME68X_OK && n_fields) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nHumidity: %.2f %%\r\n", data.humidity);
        printf("%s", msg);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    } else {
        printf("\r\n[ERROR] Failed to read humidity. Result: %d, Fields: %d\r\n", rslt, n_fields);
    }
}

void Show_Pressure(void) {
    struct bme68x_data data;
    uint8_t n_fields;
    int8_t rslt;

    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("\r\n[ERROR] Failed to set operation mode\r\n");
        return;
    }

    HAL_Delay(200); // Wait for measurement

    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &gas_sensor);
    if (rslt == BME68X_OK && n_fields) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nPressure: %.2f hPa\r\n", data.pressure / 100.0f);
        printf("%s", msg);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    } else {
        printf("\r\n[ERROR] Failed to read pressure. Result: %d, Fields: %d\r\n", rslt, n_fields);
    }
}

void Show_All_Sensors(void) {
    struct bme68x_data data;
    uint8_t n_fields;
    int8_t rslt;

    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &gas_sensor);
    if (rslt != BME68X_OK) {
        printf("\r\n[ERROR] Failed to set operation mode\r\n");
        return;
    }

    HAL_Delay(200); // Wait for measurement

    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &gas_sensor);
    if (rslt == BME68X_OK && n_fields) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                "\r\n=== Sensor Readings ===\r\n"
                "Temperature: %.2f °C\r\n"
                "Humidity: %.2f %%\r\n"
                "Pressure: %.2f hPa\r\n"
                "Gas Resistance: %.2f Ohms\r\n"
                "======================\r\n",
                data.temperature, data.humidity,
                data.pressure / 100.0f, data.gas_resistance);

        printf("%s", msg);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    } else {
        printf("\r\n[ERROR] Failed to read sensors. Result: %d, Fields: %d\r\n", rslt, n_fields);
    }
}

void Show_Help_Menu(void) {
    printf("\r\n========== Available Commands ==========\r\n");
    printf("start/help   -> Show this help menu\r\n");
    printf("temp         -> Show temperature\r\n");
    printf("hum          -> Show humidity\r\n");
    printf("pres         -> Show pressure\r\n");
    printf("all          -> Show all sensor readings\r\n");
    printf("sum n1 n2    -> Sum two integers\r\n");
    printf("lora         -> Send LoRa AT+CFG command\r\n");
    printf("lora_read    -> Read and parse LoRa module config\r\n");
    printf("lora_channel -> Prompt and set LoRa channel\r\n");
    printf("lora_re      -> Enable relay on LoRa\r\n");
    printf("lora_rd      -> Disable relay on LoRa\r\n");
    printf("lora_msg <text> -> Send text via LoRa (use 'all' to send temp,hum,pres)\r\n");
    printf("========================================\r\n");
}

void Do_Sum(char *args) {
    int a = 0, b = 0;
    if (sscanf(args, "%d %d", &a, &b) == 2) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nSum of %d + %d = %d\r\n", a, b, a + b);
        printf("%s", msg);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    } else {
        printf("\r\n[ERROR] Invalid arguments. Usage: sum n1 n2\r\n");
    }
}


/* HAL config functions */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
        Error_Handler();
}

static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();
}

static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00303D58;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Configure M0 and M1 as OUTPUT
    HAL_GPIO_WritePin(GPIOC, E22_M0_Pin | E22_M1_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = E22_M0_Pin | E22_M1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Configure AUX as INPUT
    GPIO_InitStruct.Pin = E22_AUX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Configure UART1 pins (PC4/PC5 for USART1) */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Wait_For_AUX_High(uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(E22_AUX_GPIO_Port, E22_AUX_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - start > timeout_ms) {
            printf("[ERROR] AUX timeout! Module may be busy or disconnected.\r\n");
            return;
        }
        HAL_Delay(1);
    }
    HAL_Delay(2); // Extra 2ms per E22 datasheet
}
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        // Could add LED blinking here for visual error indication
    }
}

void Lora_Connection_Test(void) {
    uint8_t config_cmd[] = {
        0xC0, 0x00, 0x09,
        0x00, 0x00, 0x00,  // ADDH, ADDL, NETID
        0x62,              // 9600bps UART, 62H
        0x00,              // Transparent mode, default
        0x17,              // CHAN = 0x17 (433 + 0x17 = 440MHz)
        0x03,              // Transmit power, RSSI enabled
        0x00, 0x00         // Optional
    };

    uint8_t response[12];

    printf("\r\n[TEST] Sending config to LoRa (CH = 0x17)...\r\n");

    Wait_For_AUX_High(1000);
    E22_SetMode(0, 1);  // Enter config mode

    HAL_UART_Transmit(&huart1, config_cmd, sizeof(config_cmd), HAL_MAX_DELAY);
    Wait_For_AUX_High(1000);

    HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, response, sizeof(response), 1000);
    if (status == HAL_OK) {
        printf("[SUCCESS] Config response:\r\n");
        for (int i = 0; i < sizeof(response); i++) {
            printf("0x%02X ", response[i]);
        }
        printf("\r\n");
    } else {
        printf("[ERROR] No response from config write.\r\n");
    }

    E22_SetMode(0, 0);  // Return to normal mode
    Wait_For_AUX_High(1000);
}
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET); // Force PC3 LOW
    HAL_Delay(1000);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);   // Force PC3 HIGH
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();
    MX_I2C1_Init();

    /* Initialize BME68x sensor */
    BME68x_Init();

    /* Set LoRa to normal mode */
    E22_SetMode(0, 1);  // Config mode
    Wait_For_AUX_High(1000);  // Ensure ready
    GPIO_PinState m0 = HAL_GPIO_ReadPin(E22_M0_GPIO_Port, E22_M0_Pin);
    GPIO_PinState m1 = HAL_GPIO_ReadPin(E22_M1_GPIO_Port, E22_M1_Pin);
    printf("M0: %d, M1: %d\r\n", m0, m1);  // Expected: M0=0, M1=1

    char rx_buffer[64];
    uint8_t rx_char;
    uint8_t idx = 0;

    /* Greeting message */
    printf("\r\n========================================\r\n");
    printf("       NordHausen Hochschule\r\n");
    printf("  Embedded Systems - GROUP 5 Present\r\n");
    printf("========================================\r\n");
    printf(" Ali | Georgy | AbolRahman | Aswathy\r\n");
    printf(" Abhirami | Sern\r\n");
    printf("----------------------------------------\r\n");
    printf("System initialized successfully!\r\n");

    /* Show main menu on CLI */
    Show_Help_Menu();

    /* Send initial message to UART1 (LoRa) */
    char startup_msg[] = "STM32 + BME68x + E22 LoRa System Started\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)startup_msg, strlen(startup_msg), HAL_MAX_DELAY);

    while (1) {
        printf("\r\nSTM32> ");  // Command prompt
        memset(rx_buffer, 0, sizeof(rx_buffer));
        idx = 0;

        /* Read command with echo and buffer overflow protection */
        while (1) {
            if (HAL_UART_Receive(&huart2, &rx_char, 1, 100) == HAL_OK) {
                /* Handle backspace */
                if (rx_char == '\b' || rx_char == 127) {
                    if (idx > 0) {
                        idx--;
                        printf("\b \b"); // Erase character on screen
                    }
                    continue;
                }

                /* Echo character */
                HAL_UART_Transmit(&huart2, &rx_char, 1, HAL_MAX_DELAY);

                /* Check for enter */
                if (rx_char == '\r' || rx_char == '\n') {
                    rx_buffer[idx] = '\0';
                    printf("\r\n"); // New line after command
                    break;
                }

                /* Add to buffer with overflow protection */
                if (idx < sizeof(rx_buffer) - 1) {
                    rx_buffer[idx++] = rx_char;
                } else {
                    printf("\r\n[WARN] Command buffer full!\r\n");
                    rx_buffer[idx] = '\0'; // Terminate buffer
                    break; // Exit input loop
                }
            }
        }

        // Parse command and arguments
        char *cmd = rx_buffer;
        char *args = strstr(rx_buffer, " ");
        if (args) {
            *args = '\0'; // Null-terminate command
            args++; // Skip space for arguments
        } else {
            args = ""; // No arguments
        }

        if (strncmp(cmd, "start", 5) == 0 || strncmp(cmd, "help", 4) == 0) {
            Show_Help_Menu();
        } else if (strncmp(cmd, "temp", 4) == 0) {
            Show_Temperature();
        } else if (strncmp(cmd, "hum", 3) == 0) {
            Show_Humidity();
        } else if (strncmp(cmd, "pres", 4) == 0) {
            Show_Pressure();
        } else if (strncmp(cmd, "all", 3) == 0) {
            Show_All_Sensors();
        } else if (strncmp(cmd, "sum", 3) == 0) {
            Do_Sum(args);
        } else if (strncmp(cmd, "lora", 4) == 0) {
            Send_LoRa_Config();
        } else if (strncmp(cmd, "lora_read", 9) == 0) {
            Read_LoRa_Config();
        } else if (strncmp(cmd, "lora_channel", 12) == 0) {
            Prompt_Set_LoRa_Channel();
        } else if (strncmp(cmd, "lora_re", 7) == 0) {
            Enable_LoRa_Relay();
        } else if (strncmp(cmd, "lora_rd", 7) == 0) {
            Disable_LoRa_Relay();
        } else if (strncmp(cmd, "lora_msg", 8) == 0) {
            if (strncmp(args, "all", 3) == 0) {
                Send_LoRa_All_Sensors();
            } else if (strlen(args) > 0) {
                Send_LoRa_Message(args);
            } else {
                printf("\r\n[ERROR] No message provided. Usage: lora_msg <text> or lora_msg all\r\n");
            }
        } else if (strncmp(cmd, "lora_test", 9) == 0) {
            Lora_Connection_Test();
        } else {
            printf("\r\nUnknown command. Type 'help' for available commands.\r\n");
        }

        // Show help menu after each command (optional, as per original)
        Show_Help_Menu();
    }
}

/**
 * @file bq76907.h
 * @brief Zephyr driver implementation for BQ76907
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <math.h>

/* Register Addresses */
#define SAFETY_ALERT_A	        0x02
#define SAFETY_STATUS_A         0x03
#define SAFETY_ALERT_B	        0x04
#define SAFETY_STATUS_B         0x05
#define BATTERY_STATUS	        0x12
#define CELL_1_VOLTAGE	        0x14
#define CELL_2_VOLTAGE	        0x16
#define CELL_3_VOLTAGE	        0x18
#define CELL_4_VOLTAGE	        0x1A
#define CELL_5_VOLTAGE	        0x1C
#define CELL_6_VOLTAGE	        0x1E
#define TS_MEASUREMENT	        0x2A
#define ALARM_STATUS            0x62
#define ALARM_RAW_STATUS        0x64
#define ALARM_ENABLE	        0x66
#define FET_CONTROL	            0x68
#define REGOUT_CONTROL	        0x69

#define BQ76907_MAX_CELLS       4  /* the number is 7 */

/* Subcommand register base */

#define SUBCMD_LB        0x3E
#define SUBCMD_UB        0x3F
#define PASSQ_START      0x40
#define PASSQ_CSUM       0x60
#define PASSQ_LEN        0x61

/* data */
struct bq76907_data {
	/* Battery status */
	uint16_t status;

	/* Cell voltages in mV */
	uint16_t voltage[BQ76907_MAX_CELLS];

	/* Temperature in °C */
	uint8_t temperature;

	/* Alarm status register */
	uint16_t alarm_status;

	/* Accumulated charge (userA-seconds) */
	int64_t accumulated_charge;

	/* Accumulated time (250ms units) */
	uint32_t accumulated_time;

	/* Charging state */
	bool is_charging;

    /* ALERT IRQ callback */
	struct gpio_callback alert_cb;

	/* Deferred work */
	struct k_work alert_work;
    /* Deferred work */
    const struct device *dev;
};

/* data to configure the devices*/
struct bq76907_config {
	struct i2c_dt_spec i2c;

	/* ALERT GPIO pin */
	struct gpio_dt_spec alrt_gpio;

	/* Protection thresholds (mV / mA) */
	uint16_t cov_threshold;
	uint16_t cuv_threshold;
	uint16_t occ_threshold;
	uint16_t ocd1_threshold;
	uint16_t ocd2_threshold;

	/* Number of cells */
	uint8_t number_cell;

	/* Shunt resistor (mΩ) */
	uint16_t rsense_mohms;
};

/**
 * @brief BQ76907 driver error codes
 *
 * This enumeration defines all possible return codes for the BQ76907 driver,
 * including communication errors and battery protection fault conditions.
 */
typedef enum {
    BQ76907_ERROR_UNDERTEMPERATURE_CHARGE = 1,
    BQ76907_ERROR_UNDERTEMPERATURE_DISCHARGE ,
    BQ76907_ERROR_INT_OVERTEMPERATURE ,
    BQ76907_ERROR_OVERTEMPERATURE_CHARGE ,
    BQ76907_ERROR_OVERTEMPERATURE_DISCHARGE ,
    BQ76907_ERROR_OVERCURRENT_CHARGE ,
    BQ76907_ERROR_OVERCURRENT_DISCHARGE_2 ,
    BQ76907_ERROR_OVERCURRENT_DISCHARGE_1 ,
    BQ76907_ERROR_SHORT_CIRCUIT_DISCHARGE ,
    BQ76907_ERROR_CELL_UNDERVOLTAGE,
    BQ76907_ERROR_CELL_OVERVOLTAGE 
} bq76907_error_code;

/**
 * @brief Mapping between a safety/alarm bit and a driver error code.
 */
struct alarm_map {
    /** @brief Bit mask from the device register (single alarm flag) */
    uint8_t bit;
    /** @brief Corresponding driver error code (BQ76907_ErrorCode_t) */
    bq76907_error_code error;
};

/**
 * @brief BQ76907 sensor channels (custom Zephyr sensor channels)
 */
enum bq76907_channel {
	BQ76907_CHAN_STATUS = SENSOR_CHAN_PRIV_START,
    BQ76907_CHAN_SECURITY_KEYS,
    BQ76907_CHAN_ACCUMULATED_CHARGE,
    BQ76907_CHAN_ACCUMULATED_TIME,
	BQ76907_CHAN_IS_CHARGING,
};

int bq76907_manage_cell_balancing(const struct device *dev);
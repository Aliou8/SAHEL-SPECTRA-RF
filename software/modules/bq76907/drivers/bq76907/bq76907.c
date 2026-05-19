/**
 * @file bq76907.c
 * @brief BQ76907 battery management system driver implementation
 * @details Implements Zephyr sensor API and custom battery management API
 *          including SoC estimation, cell balancing, and protection handling.
 */

#include "bq76907.h"

#define DT_DRV_COMPAT ti_bq76907

LOG_MODULE_REGISTER(bq76907, CONFIG_SENSOR_LOG_LEVEL);


/**
 * @brief Current/voltage safety alarm mapping (SAFETY_ALERT_A / SAFETY_STATUS_A)
 */
static const struct alarm_map alarm_current_voltage_map[] = {
    {0x04, BQ76907_ERROR_OVERCURRENT_CHARGE},
    {0x08, BQ76907_ERROR_OVERCURRENT_DISCHARGE_2},
    {0x10, BQ76907_ERROR_OVERCURRENT_DISCHARGE_1},
    {0x20, BQ76907_ERROR_SHORT_CIRCUIT_DISCHARGE},
    {0x40, BQ76907_ERROR_CELL_UNDERVOLTAGE},
    {0x80, BQ76907_ERROR_CELL_OVERVOLTAGE},
};

/**
 * @brief Temperature safety alarm mapping (SAFETY_ALERT_B / SAFETY_STATUS_B)
 */
static const struct alarm_map alarm_temp_map[] = {
    {0x08, BQ76907_ERROR_INT_OVERTEMPERATURE},
    {0x10, BQ76907_ERROR_UNDERTEMPERATURE_CHARGE},
    {0x20, BQ76907_ERROR_UNDERTEMPERATURE_DISCHARGE},
    {0x40, BQ76907_ERROR_OVERTEMPERATURE_CHARGE},
    {0x80, BQ76907_ERROR_OVERTEMPERATURE_DISCHARGE},
};



/**
 * @brief Read bytes from a register
 * @param dev device structure
 * @param reg Register address
 * @param data Output buffer
 * @param len Number of bytes
 * @return 0 on success, negative error code
 */
 static int bq76907_reg_read(const struct device *dev,
							uint8_t reg,
							uint8_t *data,
							uint8_t len)
{
	const struct bq76907_config *cfg = dev->config;
	return i2c_burst_read_dt(&cfg->i2c, reg, data, len);
}

/**
 * @brief Write bytes to a register
 * @brief Read bytes from a register
 * @param dev device structure
 * @param reg Register address
 * @param data Output buffer
 * @param len Number of bytes
 * @return 0 on success, negative error code
 */
static int bq76907_reg_write(const struct device *dev,
							 uint8_t reg,
							 const uint8_t *data,
							 uint8_t len)
{
	const struct bq76907_config *cfg = dev->config;
	return i2c_burst_write_dt(&cfg->i2c, reg, data, len);
}

/**
 * @brief Write a pass q commande
 * @param dev device structure
 * @param lsb The data LSB
 * @param msb The data MSB
 * @return 0 on success, negative error code
 */

static int bq76907_passq_write(const struct device *dev,
							   uint8_t lsb,
							   uint8_t msb,
							   const uint8_t *data,
							   size_t len)
{
	uint8_t checksum;
	uint8_t length;
	int rc;

	rc = bq76907_reg_write(dev, SUBCMD_LB, &lsb, 1);
	if (rc)
		return rc;
	rc = bq76907_reg_write(dev, SUBCMD_UB, &msb, 1);
	if (rc)
		return rc;

	rc = bq76907_reg_write(dev, PASSQ_START, data, len);
	if (rc)
		return rc;

	checksum = lsb + msb;
	for (size_t i = 0; i < len; i++)
	{
		checksum += data[i];
	}
	checksum = ~checksum;
	rc = bq76907_reg_write(dev, PASSQ_CSUM, &checksum, 1);
	if (rc)
		return rc;

	length = 2 + 2 + len;

	rc = bq76907_reg_write(dev, PASSQ_LEN, &length, 1);
	if (rc)
		return rc;

	return 0;
}

/**
 * @brief Read the battery status
 * @param dev device structure
 * @param status Pointer to store the battery status
 * @return 0 on success, negative error code
 */
static inline int bq76907_read_status(const struct device *dev, uint16_t *status)
{
	return bq76907_reg_read(dev, BATTERY_STATUS, (uint8_t *)status, sizeof(uint16_t));
}

/**
 * @brief Convert the ADC code to temperature
 * @param adcCode ADC code
 * @return Temperature in Celsius
 */
static inline int8_t tsad_to_temperature(uint16_t adcCode)
{
	float vTs = adcCode * 1.8f * 5.0f / 3.0f / 32768.0f; // ≈ 92uV per LSB
	float vRef = 1.8f;
	float rPullup = 20000.0f; // 20kΩ
	float rNTC = rPullup * vTs / (vRef - vTs);

	// Define thermistor parameters (adjust as needed for your NTC)
	float B = 4300.0f;	 // Beta value of thermistor (typical: 3435 or 3950)
	float R0 = 10000.0f; // Resistance at 25°C (10kΩ)
	float T0 = 298.15f;	 // 25°C in Kelvin

	float tempK = 1.0f / ((1.0f / B) * logf(rNTC / R0) + (1.0f / T0));
	float tempC = tempK - 273.15f;
	return (int16_t)roundf(tempC);
}

/**
 * @brief Read the battery temperature
 * @param dev Pointer to the device structure
 * @param temperature Pointer to store the temperature
 * @return 0 on success, negative error code
 */
static inline int bq76907_read_temperature(const struct device *dev, uint8_t *temperature)
{
	uint16_t adc_tmp;
	int rc = bq76907_reg_read(dev, TS_MEASUREMENT, (uint8_t *)&adc_tmp, sizeof(uint16_t));
	if (rc != 0)
		return rc;
	*temperature = tsad_to_temperature(adc_tmp);

	return rc;
}

/**
 * @brief Read all cell voltages
 *
 * @param dev Pointer to the device structure
 * @param cell_voltages Pointer to array of cell voltages
 * @return 0 on success, negative error code
 */
static inline int bq76907_read_cell_voltages(const struct device *dev,
											 uint16_t *cell_voltages)
{
	const struct bq76907_config *cfg = dev->config;
	uint16_t data[cfg->number_cell];
	return bq76907_reg_read(dev,
							CELL_1_VOLTAGE,
							(uint8_t *)data,
							cfg->number_cell * sizeof(uint16_t));
}

/**
 * @brief Read the security keys from the BQ76907
 * @param dev Pointer to the device structure
 * @param key Pointer to store the key
 * @return 0 on success, negative error code
 */
__maybe_unused static int bq76907_read_security_keys(const struct device *dev, uint32_t *key)
{
	uint8_t subcmd[2] = {0x35, 0x00};
	uint8_t keys[4];
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc != 0)
		return rc;
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc != 0)
		return rc;
	rc = bq76907_reg_read(dev, PASSQ_START, keys, 4);
	if (rc != 0)
		return rc;
	*key = (uint32_t)((uint16_t)keys[0] | ((uint16_t)keys[1] << 8) | (uint16_t)keys[2] | ((uint16_t)keys[3] << 8));
	return rc;
}

/**
 * @brief Read PASSQ accumulated data block
 *
 * @param dev Pointer to device
 * @param subcmd Subcommand (LSB/MSB)
 * @param data Pointer to output buffer (12 bytes)
 *
 * @return 0 on success
 */

static int bq76907_read_passq_block(const struct device *dev,
									uint8_t lsb,
									uint8_t msb,
									uint8_t *data)
{

	int rc = bq76907_reg_write(dev, SUBCMD_LB, &lsb, 1);
	if (rc < 0)
	{
		return rc;
	}

	rc = bq76907_reg_write(dev, SUBCMD_UB, &msb, 1);
	if (rc < 0)
	{
		return rc;
	}

	rc = bq76907_reg_read(dev, PASSQ_START, data, 12);
	if (rc < 0)
	{
		return rc;
	}

	return 0;
}

/**
 * @brief Read accumulated charge
 *
 * @param dev Pointer to device
 * @param charge Pointer to store accumulated charge
 *
 * @return 0 on success
 */

static int bq76907_read_accumulated_charge(const struct device *dev,
										   int64_t *charge)
{
	uint8_t data[12];
	int rc;

	rc = bq76907_read_passq_block(dev, 0x04, 0x00, data);
	if (rc < 0)
	{
		return rc;
	}

	uint32_t low =
		(uint32_t)data[0] |
		((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) |
		((uint32_t)data[3] << 24);

	int32_t high =
		(int32_t)((int16_t)(data[4] | (data[5] << 8)));

	*charge = ((int64_t)high << 32) | low;

	return 0;
}
/**
 * @brief Read accumulated time
 *
 * @param dev Pointer to device
 * @param time Pointer to store accumulated time
 *
 * @return 0 on success
 */

static int bq76907_read_accumulated_time(const struct device *dev,
										 uint32_t *time)
{
	uint8_t data[12];
	int rc;

	rc = bq76907_read_passq_block(dev, 0x04, 0x00, data);
	if (rc < 0)
	{
		return rc;
	}

	*time =
		(uint32_t)data[8] |
		((uint32_t)data[9] << 8) |
		((uint32_t)data[10] << 16) |
		((uint32_t)data[11] << 24);

	return 0;
}

/**
 * @brief Check if the battery is charging
 * @param dev Pointer to the device structure
 * @param is_charging Pointer to store the charging status (true if charging, false if discharging)
 * @return 0 on success, negative error code
 */
static int bq76907_is_charging(const struct device *dev, bool *is_charging)
{
	int64_t charge;
	uint32_t time;
	int rc = bq76907_read_accumulated_charge(dev, &charge);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_read_accumulated_time(dev, &time);
	if (rc < 0)
	{
		return rc;
	}
	float timeSeconds = (float)time * 0.25f;
	if (timeSeconds > 0.0f)
	{
		// Current = charge / time (in amperes)
		float current = (float)charge / timeSeconds;

		// Positive current means charging, negative means discharging
		*is_charging = (current > 0.0f);
	}
	else
	{
		// If no time has passed, assume not charging
		*is_charging = false;
	}

	return rc;
}


/**
 * @brief Seal the BQ76907 (return to protected mode)
 * @param dev Pointer to the device structure
 * @return 0 on success, negative error code
 */
__maybe_unused  static int bq76907_seal(const struct device *dev)
{
	uint8_t subcmd[2] = {0x30, 0x00};
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc < 0)
	{
		return rc;
	}

	return 0;
}

/**
 * @brief Unseal the BQ76907 (return to protected mode)
 * @param dev Pointer to the device structure
 * @param key1 16-bit key1
 * @param key2 16-bit key2
 * @return 0 on success, negative error code
 */
static int bq76907_unseal(const struct device *dev, uint16_t key1, uint16_t key2)
{
	uint8_t keyBytes[2];
	keyBytes[0] = key1 & 0xFF;		  // LSB
	keyBytes[1] = (key1 >> 8) & 0xFF; // MSB
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &keyBytes[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &keyBytes[1], 1);
	if (rc < 0)
	{
		return rc;
	}
	keyBytes[0] = key2 & 0xFF;		  // LSB
	keyBytes[1] = (key2 >> 8) & 0xFF; // MSB

	rc = bq76907_reg_write(dev, SUBCMD_LB, &keyBytes[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &keyBytes[1], 1);
	if (rc < 0)
	{
		return rc;
	}

	return 0;
}
/**
 * @brief Reset the BQ76907
 * @param dev Pointer to the device structure
 * @return 0 on success, negative error code
 */

static int bq76907_reset(const struct device *dev)
{
	uint8_t subcmd[2] = {0x12, 0x00};
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc < 0)
	{
		return rc;
	}
	return 0;
}

/**
 * @brief Set the BQ76907 to allow configuration updates
 * @param dev Pointer to the device structure
 * @return 0 on success, negative error code
 */

static int bq76907_allow_config_update(const struct device *dev)
{
	uint8_t subcmd[2] = {0x90, 0x00};
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc < 0)
	{
		return rc;
	}
	while (1)
	{
		uint8_t status[2];
		bq76907_reg_write(dev, BATTERY_STATUS, status, 2);
		if (status[0] & (1 << 5))
			break;
	}

	return 0;
}

/**
 * @brief Exit configuration update mode
 * @param dev Pointer to the device structure
 * @return 0 on success, negative error code
 */

static int bq76907_exit_config_update(const struct device *dev)
{
	uint8_t subcmd[2] = {0x92, 0x00};
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc < 0)
	{
		return rc;
	}
	while (1)
	{
		uint8_t status[2];
		bq76907_reg_write(dev, BATTERY_STATUS, status, 2);
		if (status[0] & (1 << 5))
			break;
	}

	return 0;
}

/**
 * @brief Configure power settings of BQ76907
 *
 * @param dev Pointer to device
 * @return 0 on success, negative error code
 */
static int bq76907_configure_power(const struct device *dev)
{
	uint8_t data[1] = {0x30};

	return bq76907_passq_write(dev, 0x14, 0x90, data, 1);
}

/**
 * @brief Configure FET behavior (charge/discharge control)
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_configure_fet_options(const struct device *dev)
{
	uint8_t data[1] = {0x0C};

	return bq76907_passq_write(dev, 0x1E, 0x90, data, 1);
}

/**
 * @brief Configure number of active cells (VCELL mode)
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_configure_vcell_mode(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;
	uint8_t data[1] = {cfg->number_cell};

	return bq76907_passq_write(dev, 0x1B, 0x90, data, 1);
}

/**
 * @brief Set overcurrent charge threshold
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_set_occ_threshold(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;

	uint8_t reg =
		(uint8_t)(((cfg->rsense_mohms * cfg->occ_threshold) + 1000) / 2000);

	return bq76907_passq_write(dev, 0x36, 0x90, &reg, 1);
}

/**
 * @brief Set overcurrent discharge threshold 1
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_set_ocd1_threshold(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;

	uint8_t reg =
		(uint8_t)(((cfg->rsense_mohms * cfg->ocd1_threshold) + 1000) / 2000);

	return bq76907_passq_write(dev, 0x38, 0x90, &reg, 1);
}

/**
 * @brief Set overcurrent discharge threshold 2
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_set_ocd2_threshold(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;

	uint8_t reg =
		(uint8_t)(((cfg->rsense_mohms * cfg->ocd2_threshold) + 1000) / 2000);

	return bq76907_passq_write(dev, 0x3A, 0x90, &reg, 1);
}

/**
 * @brief Set cell undervoltage protection threshold
 *
 * @param dev Pointer to device
 * @return 0 on success, negative error code
 */
static int bq76907_set_cuv_threshold(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;

	uint16_t value = cfg->cuv_threshold;

	uint8_t data[2] = {
		value & 0xFF,
		(value >> 8) & 0xFF};

	return bq76907_passq_write(dev, 0x2E, 0x90, data, 2);
}
/**
 * @brief Set CUV protection recovery hysteresis
 *
 * @param dev Pointer to device
 * @return 0 on success, negative error code
 */
static int bq76907_set_cuv_prot_rec_hysteresis(const struct device *dev)
{
	uint8_t data[1] = {0x01};

	return bq76907_passq_write(dev, 0x31, 0x90, data, 1);
}

/**
 * @brief Set cell overvoltage protection threshold (COV)
 *
 * @param dev Pointer to device
 * @return 0 on success, negative error code
 */
static int bq76907_set_cov_threshold(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;

	uint16_t value = cfg->cov_threshold;

	uint8_t data[2] = {
		value & 0xFF,
		(value >> 8) & 0xFF};

	return bq76907_passq_write(dev, 0x32, 0x90, data, 2);
}

/**
 * @brief Set COV protection recovery hysteresis
 *
 * @param dev Pointer to device
 * @return 0 on success, negative error code
 */
static int bq76907_set_cov_prot_rec_hysteresis(const struct device *dev)
{
	uint8_t data[1] = {0x01};

	return bq76907_passq_write(dev, 0x35, 0x90, data, 1);
}

/**
 * @brief Enable thermistor pull-up and configure REGOUT_CONTROL
 *
 * @details Enables the internal thermistor pull-up by writing REGOUT_CONTROL and set Vldo=3.3V.
 *
 * @param dev Pointer to the device structure
 * @return 0 on success, negative error code
 */
static inline int bq76907_enable_thermistor_pullup(const struct device *dev)
{
	uint8_t value = 0x1E;

	return bq76907_reg_write(dev, REGOUT_CONTROL, &value, 1);
}

/**
 * @brief Enable alarms
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_enable_alarms(const struct device *dev)
{
	uint16_t data = 0xF000;

	return bq76907_reg_write(dev, ALARM_ENABLE, (uint8_t *)&data, sizeof(data));
}

/**
 * @brief Reset passed charge
 *
 * @param dev Pointer to device
 * @return 0 on success
 */

static int bq76907_reset_passed_charge(const struct device *dev)
{
	uint8_t subcmd[2] = {0x05, 0x00};

	int rc;

	rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc)
		return rc;

	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc)
		return rc;

	return 0;
}

/**
 * @brief Enable protections A
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_enable_protections_a(const struct device *dev)
{
	uint8_t lsb = 0x24;
	uint8_t msb = 0x90;
	uint8_t data[1] = {0xFC};

	return bq76907_passq_write(dev, lsb, msb, data, sizeof(data));
}

/**
 * @brief Enable protections B
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
static int bq76907_enable_protections_b(const struct device *dev)
{
	uint8_t lsb = 0x25;
	uint8_t msb = 0x90;
	uint8_t data[1] = {0x3E};

	return bq76907_passq_write(dev, lsb, msb, data, sizeof(data));
}

/**
 * @brief Read the alarm status
 * @param dev Pointer to the device structure
 * @param alarm_status status Pointer to array to store the status
 * @return 0 on success, negative error code
 */
static inline int bq76907_read_alarm_status(const struct device *dev, uint16_t *alarm_status)
{
	return bq76907_reg_read(dev,
							  ALARM_STATUS,
							  (uint8_t *)alarm_status,
							  sizeof(uint16_t));
}

/**
 * @brief Clear alarm status
 *
 * @param dev Pointer to device
 * @param status Value to clear
 * @return 0 on success
 */
__maybe_unused static int bq76907_clear_alarm_status(const struct device *dev, uint16_t status)
{
	return bq76907_reg_write(dev, ALARM_STATUS, (uint8_t *)&status, sizeof(uint16_t));
}

/**
 * @brief Read safety alert A
 *
 * @param dev Pointer to device
 * @param status Output byte
 * @return 0 on success
 */
__maybe_unused static int bq76907_read_safety_alert_a(const struct device *dev, uint8_t *status)
{
	return bq76907_reg_read(dev, SAFETY_ALERT_A, status, 1);
}

/**
 * @brief Read safety alert B
 *
 * @param dev Pointer to device
 * @param status Output byte
 * @return 0 on success
 */
__maybe_unused static int bq76907_read_safety_alert_b(const struct device *dev, uint8_t *status)
{
	return bq76907_reg_read(dev, SAFETY_ALERT_B, status, 1);
}

/**
 * @brief Read safety Status A
 *
 * @param dev Pointer to device
 * @param status Output byte
 * @return 0 on success
 */

__maybe_unused static int bq76907_read_safety_status_a(const struct device *dev, uint8_t *status)
{
	return bq76907_reg_read(dev, SAFETY_STATUS_A, status, 1);
}

/**
 * @brief Read safety Status A
 *
 * @param dev Pointer to device
 * @param status Output byte
 * @return 0 on success
 */

__maybe_unused static int bq76907_read_safety_status_b(const struct device *dev, uint8_t *status)
{
	return bq76907_reg_read(dev, SAFETY_STATUS_B, status, 1);
}

/**
 * @brief Enable protection recovery
 *
 * @param dev Pointer to device
 * @return 0 on success
 */
__maybe_unused static int bq76907_protection_recovery(const struct device *dev)
{
	uint8_t lsb = 0x9B;
	uint8_t msb = 0x00;
	uint8_t data[1] = {0x82};

	return bq76907_passq_write(dev, lsb, msb, data, 1);
}

/**
 * @brief Decode a safety/alarm register and return a mapped error code
 * @param dev Pointer to device
 * @param reg       Safety register address to read
 * @param map       Pointer to alarm mapping table (bit → error)
 * @param map_len   Number of entries in the mapping table
 *
 * @return 0 on success
 */
static int bq76907_map_alarm(const struct device *dev, uint8_t reg,
							 const struct alarm_map *map,
							 size_t map_len)
{
	uint8_t data;
	int rc;

	rc = bq76907_reg_read(dev, reg, &data, 1);
	if (rc != 0)
	{
		return rc;
	}

	for (size_t i = 0; i < map_len; i++)
	{
		if (data & map[i].bit)
		{
			return map[i].error;
		}
	}

	return 0;
}

/**
 * @brief Handle and decode all active BQ76907 alarm sources
 *
 * @param dev Pointer to device
 * @return bq76907_error_code
 *         - Specific BQ76907 error if an alarm is active
 *         - 0 if no alarms are active
 *         - Negative error code if a register read fails
 */

bq76907_error_code bq76907_handle_alarms(const struct device *dev)
{
	uint16_t alarms;
	int rc;

	rc = bq76907_read_alarm_status(dev, &alarms);
	if (rc != 0)
	{
		return rc;
	}

	if (alarms & 0x1000)
	{
		rc = bq76907_map_alarm(dev,
							   SAFETY_ALERT_B,
							   alarm_temp_map,
							   ARRAY_SIZE(alarm_temp_map));
		if (rc != 0)
			return rc;
	}

	if (alarms & 0x2000)
	{
		rc = bq76907_map_alarm(dev,
							   SAFETY_ALERT_A,
							   alarm_current_voltage_map,
							   ARRAY_SIZE(alarm_current_voltage_map));
		if (rc != 0)
			return rc;
	}

	if (alarms & 0x4000)
	{
		rc = bq76907_map_alarm(dev,
							   SAFETY_STATUS_B,
							   alarm_temp_map,
							   ARRAY_SIZE(alarm_temp_map));
		if (rc != 0)
			return rc;
	}

	if (alarms & 0x8000)
	{
		rc = bq76907_map_alarm(dev,
							   SAFETY_STATUS_A,
							   alarm_current_voltage_map,
							   ARRAY_SIZE(alarm_current_voltage_map));
		if (rc != 0)
			return rc;
	}

	return 0;
}

/**
 * @brief Enable cell balancing for specified cells
 *
 * @param dev Pointer to device
 * @param cell_mask Bit mask of cells to balance
 *
 * @return 0 on success
 */

static int bq76907_enable_cell_balancing(const struct device *dev, uint8_t cell_mask)
{
	uint8_t lsb = 0x83;
	uint8_t msb = 0x00;
	uint8_t data[1] = {cell_mask};

	return bq76907_passq_write(dev, lsb, msb, data, sizeof(data));
}

/**
 * @brief Get the current cell balancing status
 *
 * @param dev Pointer to device
 * @param active_cells active_cells Pointer to store the bit mask of actively balanced cells
 *
 * @return 0 on success
 */
static int bq76907_get_cell_balancing_status(const struct device *dev, uint8_t *active_cells)
{
	uint8_t subcmd[2] = {0x83, 0x00};
	int rc = bq76907_reg_write(dev, SUBCMD_LB, &subcmd[0], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_write(dev, SUBCMD_UB, &subcmd[1], 1);
	if (rc < 0)
	{
		return rc;
	}
	rc = bq76907_reg_read(dev, SUBCMD_UB, active_cells, 1);
	if (rc < 0)
	{
		return rc;
	}

	return 0;
}


/**
 * @brief Disable cell balancing for specified cells
 *
 * @param dev Pointer to device
 *
 * @return 0 on success
 */

static int bq76907_disable_cell_balancing(const struct device *dev,
                                           uint8_t cell_mask)
{
	uint8_t lsb = 0x83;
	uint8_t msb = 0x00;
	uint8_t active_cells = 0;
	int rc;

	rc = bq76907_get_cell_balancing_status(dev, &active_cells);
	if (rc < 0) {
		return rc;
	}

	/* retirer les cellules demandées */
	uint8_t new_state = active_cells & ~cell_mask;

	uint8_t data[1];
	data[0] = new_state;

	return bq76907_passq_write(dev, lsb, msb, data, sizeof(data));
}

/**
 * @brief Manages the cell balancing process based on battery state.
 * @details This function should be called periodically (e.g., every second). It manages
 *          the balancing of the cells to equalize the pack. BQ76907 only allows 2 cells
 *          to be balanced at once. Balances the 2 highest voltage cells until there's
 *          a 20mV difference between min and max.
 * @param dev Pointer to device
 * @return 0 on success
 */
/**
 * @brief Manages the cell balancing process based on battery state.
 *
 * @details This function should be called periodically (e.g., every second).
 *          It finds the highest voltage cells and enables balancing on up to
 *          2 cells when the voltage difference exceeds 20mV.
 *          If the pack is already balanced, it disables balancing.
 *
 * @param dev Pointer to device
 *
 * @return 0 on success
 */

int bq76907_manage_cell_balancing(const struct device *dev)
{
	const struct bq76907_config *cfg = dev->config;
	uint16_t cellVoltages[cfg->number_cell] ;
	for (int i = 0; i < cfg->number_cell; i++) {
    cellVoltages[i] = 0;
}

	uint16_t minV = 0xFFFF;
	uint16_t maxV = 0x0000;

	int8_t top[2] = {-1, -1};
	uint8_t mask = 0;

	int rc = bq76907_read_cell_voltages(dev, cellVoltages);
	if (rc < 0)
	{
		return rc;
	}

	for (uint8_t i = 0; i < cfg->number_cell; i++)
	{
		if (cellVoltages[i] < minV)
		{
			minV = cellVoltages[i];
		}
		if (cellVoltages[i] > maxV)
		{
			maxV = cellVoltages[i];
		}
	}

	if ((maxV - minV) <= 20)
	{
		return bq76907_disable_cell_balancing(dev,0xFF);
	}

	for (uint8_t i = 0; i < cfg->number_cell; i++)
	{

		if (top[0] == -1 || cellVoltages[i] > cellVoltages[top[0]])
		{
			top[1] = top[0];
			top[0] = i;
		}
		else if (top[1] == -1 || cellVoltages[i] > cellVoltages[top[1]])
		{
			if (i != top[0])
			{
				top[1] = i;
			}
		}
	}

	if (top[1] == -1)
	{
		top[1] = top[0];
	}

	for (uint8_t i = 0; i < 2; i++)
	{

		uint8_t bit = top[i] + 1;

		if (bit != 0)
		{
			mask |= (1 << bit);
		}
	}

	return bq76907_enable_cell_balancing(dev, mask);
}

/* =========================================================
 * 2. Work handler
 * ========================================================= */

static void bq76907_alert_work_handler(struct k_work *work)
{
	struct bq76907_data *data =
		CONTAINER_OF(work, struct bq76907_data, alert_work);

	const struct device *dev = data->dev;

	bq76907_handle_alarms(dev);

}

/* =========================================================
 * 3. GPIO IRQ callback
 * ========================================================= */

static void bq76907_alert_handler(const struct device *port,
				  struct gpio_callback *cb,
				  gpio_port_pins_t pins)
{
	struct bq76907_data *data =
		CONTAINER_OF(cb, struct bq76907_data, alert_cb);

	k_work_submit(&data->alert_work);
}


/**
 * @brief Initialize the BQ76907 device
 *
 * @param dev Pointer to device structure
 *
 * @return 0 on success, negative error code
 */
static int bq76907_init(const struct device *dev)
{
	int rc;
	const struct bq76907_config *cfg = dev->config;
	struct bq76907_data *data = dev->data;
	data->dev = dev;

	/* -------------------------------------------------
	 * 1. Check I2C bus readiness
	 * ------------------------------------------------- */
	if (!device_is_ready(cfg->i2c.bus)) {
		return -ENODEV;
	}

	/* -------------------------------------------------
	 * 2. Configure ALERT GPIO
	 * ------------------------------------------------- */
	if (!device_is_ready(cfg->alrt_gpio.port)) {
		return -ENODEV;
	}

	rc = gpio_pin_configure_dt(&cfg->alrt_gpio, GPIO_INPUT);
	if (rc < 0) {
		return rc;
	}

	/* Init workqueue item */
	k_work_init(&data->alert_work,
		    bq76907_alert_work_handler);

	/* Init GPIO callback */
	gpio_init_callback(&data->alert_cb,
			   bq76907_alert_handler,
			   BIT(cfg->alrt_gpio.pin));
	
	rc = gpio_add_callback(cfg->alrt_gpio.port,
			       &data->alert_cb);
	if (rc < 0) {
		return rc;
	}

	rc = gpio_pin_interrupt_configure_dt(&cfg->alrt_gpio,
					     GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		return rc;
	}

	/* -------------------------------------------------
	 * 3. Reset / wake device (optional depending HW)
	 * ------------------------------------------------- */
	rc = bq76907_reset(dev);
	if (rc < 0) return rc;

	/* -------------------------------------------------
	 * 4. Unseal + CFG update mode (required for writes)
	 * ------------------------------------------------- */
	rc = bq76907_unseal(dev, 0x0414, 0x3672);
	if (rc < 0) {
		return rc;
	}

	rc =  bq76907_allow_config_update(dev);
	if (rc < 0) {
		return rc;
	}

	/* -------------------------------------------------
	 * 5. Apply configuration registers
	 * ------------------------------------------------- */
	rc = bq76907_configure_power(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_configure_fet_options(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_configure_vcell_mode(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_set_occ_threshold(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_set_ocd1_threshold(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_set_ocd2_threshold(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_set_cuv_threshold(dev);
	if (rc < 0) {
		return rc;
	}
	rc = bq76907_set_cuv_prot_rec_hysteresis(dev);
	if (rc < 0) {
		return rc;
	}
	rc = bq76907_set_cov_threshold(dev);
	if (rc < 0) {
		return rc;
	} 
	rc = bq76907_set_cov_prot_rec_hysteresis(dev);
	if (rc < 0) {
		return rc;
	}
	rc = bq76907_enable_thermistor_pullup(dev);
	if (rc < 0) {
		return rc;
	}

	/* -------------------------------------------------
	 * 6. Enable protections
	 * ------------------------------------------------- */
	rc = bq76907_enable_protections_a(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_enable_protections_b(dev);
	if (rc < 0) {
		return rc;
	}

	rc = bq76907_exit_config_update(dev);
	if (rc < 0) {
		return rc;
	}

	/* -------------------------------------------------
	 * 7. Enable alarms
	 * ------------------------------------------------- */
	rc = bq76907_enable_alarms(dev);
	if (rc < 0) {
		return rc;
	}

	bq76907_reset_passed_charge(dev);
	if (rc < 0) {
		return rc;
	}

	/* -------------------------------------------------
	 * 8. Initialize runtime data
	 * ------------------------------------------------- */
	data->status = 0;
	data->alarm_status = 0;
	data->accumulated_charge = 0;
	data->accumulated_time = 0;
	data->is_charging = false;

	for (int i = 0; i < BQ76907_MAX_CELLS; i++) {
		data->voltage[i] = 0;
	}

	return 0;
}


/* =========================================================
 * API structures
 * ========================================================= */

/**
 * @brief Get a sensor channel value from the BQ76907 cache
 *
 * @param dev Pointer to the device structure
 * @param chan Sensor channel to get
 * @param val Pointer to store the sensor value
 * @return 0 on success, negative error code
 */
static int bq76907_channel_get(const struct device *dev,
							   enum sensor_channel chan,
							   struct sensor_value *val)
{
	const struct bq76907_config *cfg = dev->config;
	struct bq76907_data *data = dev->data;
	uint32_t mv;

	switch ((int)chan)
	{

	case SENSOR_CHAN_VOLTAGE:
		/*
		 * Return total pack voltage
		 * by summing all cell voltages.
		 */
		mv = 0;

		for (int i = 0; i < cfg->number_cell; i++)
		{
			mv += data->voltage[i];
		}

		val->val1 = mv / 1000;
		val->val2 = (mv % 1000) * 1000;
		break;

	case SENSOR_CHAN_DIE_TEMP:
		/*
		 * Temperature is stored in °C
		 */
		val->val1 = data->temperature;
		val->val2 = 0;
		break;

	case BQ76907_CHAN_ACCUMULATED_CHARGE:
		/*
		 * Conversion from accumulated charge
		 * to mAh.
		 */
		val->val1 = data->accumulated_charge / 3600;
		val->val2 = 0;
		break;

	case BQ76907_CHAN_ACCUMULATED_TIME:
		/*
		 * accumulated_time stored in 250 ms units
		 */
		val->val1 = data->accumulated_time / 240;
		val->val2 = 0;
		break;

	case BQ76907_CHAN_IS_CHARGING:
		val->val1 = data->is_charging;
		val->val2 = 0;
		break;
	case BQ76907_CHAN_STATUS:
		val->val1 = data->status;
		val->val2 = 0;
		break;
	default:
		LOG_ERR("Unsupported channel!");
		return -ENOTSUP;
	}
	return 0;
}

/**
 * @brief Fetch sensor data from the BQ76907 device
 *
 * Reads all or selected raw measurements from the hardware and updates
 * the driver's internal data structure.
 *
 * @param dev Pointer to the device structure
 * @param chan Sensor channel request (use SENSOR_CHAN_ALL for full update)
 * @return 0 on success, negative error code
 */
static int bq76907_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct bq76907_data *data = dev->data;

	switch ((int)chan)
	{
	case SENSOR_CHAN_VOLTAGE:
		bq76907_read_cell_voltages(dev, data->voltage);
		break;
	case SENSOR_CHAN_DIE_TEMP:
		bq76907_read_temperature(dev, &data->temperature);
		break;
	case BQ76907_CHAN_ACCUMULATED_CHARGE:
		bq76907_read_accumulated_charge(dev, &data->accumulated_charge);
		break;
	case BQ76907_CHAN_ACCUMULATED_TIME:
		bq76907_read_accumulated_time(dev, &data->accumulated_time);
		break;
	case BQ76907_CHAN_IS_CHARGING:
		bq76907_is_charging(dev, &data->is_charging);
		break;
	case BQ76907_CHAN_STATUS:
		bq76907_read_status(dev, &data->status);
		break;
	default:
		LOG_ERR("Unsupported channel!");
		return -ENOTSUP;
	}
	return 0;
}


/** Zephyr sensor API */
static const struct sensor_driver_api bq76907_api = {
	.sample_fetch = bq76907_sample_fetch,
	.channel_get = bq76907_channel_get,
};

static struct bq76907_data bq76907_data;

static const struct bq76907_config bq76907_config = {
	.i2c = I2C_DT_SPEC_INST_GET(0),
	.alrt_gpio = GPIO_DT_SPEC_INST_GET(0, alert_gpios),
	.cov_threshold = DT_INST_PROP(0, overvoltage_threshold),
	.cuv_threshold = DT_INST_PROP(0, undervoltage_threshold),
	.occ_threshold = DT_INST_PROP(0, overcurrent_in_charge_threshold),
	.ocd1_threshold = DT_INST_PROP(0, overcurrent_in_discharge1_threshold),
	.ocd2_threshold = DT_INST_PROP(0, overcurrent_in_discharge2_threshold),
	.number_cell = DT_INST_PROP(0, number_cell),
	.rsense_mohms = DT_INST_PROP(0, rsense_mohms),
};

DEVICE_DT_INST_DEFINE(0,
		      bq76907_init,
		      NULL,
		      &bq76907_data,
		      &bq76907_config,
		      POST_KERNEL,
		      CONFIG_SENSOR_INIT_PRIORITY,
		      &bq76907_api);
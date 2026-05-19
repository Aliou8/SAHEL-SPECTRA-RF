#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#include "bq76907.h"

#define BQ76907_NODE DT_INST(0, ti_bq76907)

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(BQ76907_NODE);

    if (!device_is_ready(dev)) {
        printk("BQ76907 not ready\n");
        return -1;
    }

    struct sensor_value val;

    while (1) {

        sensor_sample_fetch(dev);

        sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &val);
        printk("Pack voltage: %d.%06d V\n", val.val1, val.val2);

        sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &val);
        printk("Temp: %d C\n", val.val1);

        sensor_channel_get(dev, BQ76907_CHAN_IS_CHARGING, &val);
        printk("Charging: %d\n", val.val1);

        k_sleep(K_SECONDS(2));
    }
}
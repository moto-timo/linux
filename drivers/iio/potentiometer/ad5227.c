// SPDX-License-Identifier: GPL-2.0
/*
 * Analog Devices AD5227 digital potentiometer driver
 * Copyright (C) 2026 Tim Orling <tim.orling@konsulko.com>
 *
 * Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/AD5227.pdf
 *
 * DEVID    #Wipers  #Positions  Resistance (kOhm)
 * ad5227   1        64          10
 */

#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#define AD5227_MAX_POS	63	/* 6-bit: 0-63 */
#define AD5227_KOHMS	10	/* 10 kΩ */

struct ad5227_data {
	struct spi_device *spi;
	struct mutex lock;
	unsigned int value;	/* Cached wiper value */

	u8 buf __aligned(IIO_DMA_MINALIGN);
};

/* Single resistance channel definition */
static const struct iio_chan_spec ad5227_channel = {
	.type = IIO_RESISTANCE,
	.indexed = 1,
	.output = 1,
	.channel = 0,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
};

/* Read returns cached value (hardware is write-only) */
static int ad5227_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct ad5227_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = data->value;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		/* Scale: (kohms * 1000) / max_pos */
		*val = 1000 * AD5227_KOHMS;
		*val2 = AD5227_MAX_POS;
		return IIO_VAL_FRACTIONAL;
	}

	return -EINVAL;
}

/* Write wiper position via simple 8-bit SPI transfer */
static int ad5227_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct ad5227_data *data = iio_priv(indio_dev);
	int ret;

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	if (val < 0 || val > AD5227_MAX_POS || val2)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->buf = val;
	ret = spi_write(data->spi, &data->buf, 1);
	if (!ret)
		data->value = val;
	mutex_unlock(&data->lock);

	return ret;
}

static const struct iio_info ad5227_info = {
	.read_raw = ad5227_read_raw,
	.write_raw = ad5227_write_raw,
};

static int ad5227_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct ad5227_data *data;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->spi = spi;
	mutex_init(&data->lock);
	data->value = 0;	/* Initialize to 0 */

	indio_dev->info = &ad5227_info;
	indio_dev->channels = &ad5227_channel;
	indio_dev->num_channels = 1;
	indio_dev->name = "ad5227";
	indio_dev->modes = INDIO_DIRECT_MODE;

	return devm_iio_device_register(&spi->dev, indio_dev);
}

static const struct of_device_id ad5227_match[] = {
	{ .compatible = "adi,ad5227" },
	{ }
};
MODULE_DEVICE_TABLE(of, ad5227_match);

static const struct spi_device_id ad5227_id[] = {
	{ "ad5227", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, ad5227_id);

static struct spi_driver ad5227_driver = {
	.driver = {
		.name = "ad5227",
		.of_match_table = ad5227_match,
	},
	.probe = ad5227_probe,
	.id_table = ad5227_id,
};

module_spi_driver(ad5227_driver);

MODULE_AUTHOR("Tim Orling <tim.orling@konsulko.com>");
MODULE_DESCRIPTION("Analog Devices AD5227 digital potentiometer");
MODULE_LICENSE("GPL");

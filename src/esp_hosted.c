#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/sdhc.h>
#include <zephyr/logging/log.h>
#include <stm32_ll_bus.h>
#include <zephyr/cache.h>
#include <stm32_ll_gpio.h>
#include <inttypes.h>
#include <strings.h>
#include <errno.h>
#include <endian.h>
#include "sdio_io.h"

LOG_MODULE_REGISTER(esp_hosted, CONFIG_SDHC_LOG_LEVEL);

int hosted_sdio_set_blocksize(const struct device *card, uint8_t fn, uint16_t value)
{
	size_t offset = SD_IO_FBR_START * fn;
	const uint8_t *bs_u8 = (const uint8_t *)&value;
	uint16_t bs_read = 0;
	uint8_t *bs_read_u8 = (uint8_t *)&bs_read;

	// Set and read back block size
	ERROR_CHECK(sdio_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, bs_u8[0],
				       NULL));
	ERROR_CHECK(sdio_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, bs_u8[1],
				       NULL));
	ERROR_CHECK(
		sdio_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, &bs_read_u8[0]));
	ERROR_CHECK(
		sdio_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, &bs_read_u8[1]));
	LOG_INF("Function %d Blocksize: %d", fn, (unsigned int)bs_read);

	if (bs_read == value) {
		return 0;
	} else {
		return -1;
	}
}

int hosted_sdio_card_fn_init(const struct device *card)
{
	uint8_t ioe = 0;
	uint8_t ior = 0;
	uint8_t ie = 0;
	uint8_t bus_width = 0;
	uint16_t bs = 0;
	int i = 0;

	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));
	LOG_INF("IOE: 0x%02x", ioe);

	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
	LOG_INF("IOR: 0x%02x", ior);

	// enable function 1
	ioe |= FUNC1_EN_MASK;
	ERROR_CHECK(sdio_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, ioe, &ioe));
	LOG_INF("IOE: 0x%02x", ioe);

	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));
	LOG_INF("IOE: 0x%02x", ioe);

	// wait for the card to become ready
	ior = 0;
	for (i = 0; i < SDIO_INIT_MAX_RETRY; i++) {
		ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
		LOG_INF("IOR: 0x%02x", ior);
		if (ior & FUNC1_EN_MASK) {
			break;
		} else {
			k_usleep(10 * 1000);
		}
	}
	if (i >= SDIO_INIT_MAX_RETRY) {
		// card failed to become ready
		return -1;
	}

	// get interrupt status
	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	LOG_INF("IE: 0x%02x", ie);

	// enable interrupts for function 1 and master enable
	ie |= BIT(0) | FUNC1_EN_MASK;
	ERROR_CHECK(sdio_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, ie, NULL));

	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	LOG_INF("IE: 0x%02x", ie);

	// get bus width register
	ERROR_CHECK(sdio_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_BUS_WIDTH, &bus_width));
	LOG_INF("BUS_WIDTH: 0x%02x", bus_width);

	// skip enable of continous SPI interrupts

	// set FN0 block size to 512
	bs = 512;
	ERROR_CHECK(hosted_sdio_set_blocksize(card, SDIO_FUNC_0, bs));

	// set FN1 block size to 512
	bs = 512;
	ERROR_CHECK(hosted_sdio_set_blocksize(card, SDIO_FUNC_1, bs));

	return 0;
}

int hosted_sdio_read_block(const struct device *card, uint32_t reg, uint8_t *data, uint16_t size,
			   bool lock_required)
{
	int res = 0;

	if (size <= 1) {
		res = sdio_io_read_byte(card, SDIO_FUNC_1, reg, data);
	} else {
		res = sdio_read_fromio(card, SDIO_FUNC_1, reg, data,
				       H_SDIO_RX_LEN_TO_TRANSFER(size));
	}
	return res;
}

int hosted_sdio_write_block(const struct device *card, uint32_t reg, uint8_t *data, uint16_t size,
			    bool lock_required)
{
	int res = 0;

	if (size <= 1) {
		res = sdio_io_write_byte(card, SDIO_FUNC_1, reg, *data, NULL);
	} else {
		res = sdio_write_toio(card, SDIO_FUNC_1, reg, data,
				      H_SDIO_TX_LEN_TO_TRANSFER(size));
	}
	return res;
}

static uint8_t sdio_tx_buffer[1536] __aligned(32) __attribute__((section(".nocache")));

int esp_hosted_tx(const struct device *card, uint8_t iface_type, uint8_t iface_num,
		  uint8_t *wbuffer, uint16_t wlen)
{
	uint8_t *sendbuf = NULL;
	uint8_t *payload = NULL;
	struct esp_payload_header *header = NULL;

	memset(sdio_tx_buffer, 0, wlen + sizeof(struct esp_payload_header));
	header = (struct esp_payload_header *)sdio_tx_buffer;
	payload = sdio_tx_buffer + sizeof(struct esp_payload_header);

	header->len = htole16(wlen);
	header->offset = htole16(sizeof(struct esp_payload_header));
	header->if_type = iface_type;
	header->if_num = iface_num;
	header->seq_num = 0;

	memcpy(payload, wbuffer, wlen);

	uint32_t data_left = wlen + sizeof(struct esp_payload_header);

	uint32_t block_send_len =
		((data_left + ESP_BLOCK_SIZE - 1) / ESP_BLOCK_SIZE) * ESP_BLOCK_SIZE;

	int ret = sdio_io_write_blocks(card, SDIO_FUNC_1, ESP_SLAVE_CMD53_END_ADDR - data_left, sdio_tx_buffer,
			     block_send_len);
	k_free(sendbuf);

	return ret;
}

int send_slave_config(const struct device *card, uint8_t host_cap, uint8_t firmware_chip_id,
		      uint8_t raw_tp_direction, uint8_t low_thr_thesh, uint8_t high_thr_thesh)
{
#define LENGTH_1_BYTE 1
	struct esp_priv_event *event = NULL;
	uint8_t *pos = NULL;
	uint16_t len = 0;
	uint8_t *sendbuf = NULL;

	sendbuf = k_malloc(512);

	/* Populate event data */
	// event = (struct esp_priv_event *) (sendbuf + sizeof(struct esp_payload_header));
	// //ZeroCopy
	event = (struct esp_priv_event *)(sendbuf);

	event->event_type = ESP_PRIV_EVENT_INIT;

	/* Populate TLVs for event */
	pos = event->event_data;

	/* TLVs start */

	/* TLV - Board type */
	LOG_INF("send_slave_config: Slave chip Id[%x]", ESP_PRIV_FIRMWARE_CHIP_ID);
	*pos = HOST_CAPABILITIES;
	pos++;
	len++;
	*pos = LENGTH_1_BYTE;
	pos++;
	len++;
	*pos = host_cap;
	pos++;
	len++;

	/* TLV - Capability */
	*pos = RCVD_ESP_FIRMWARE_CHIP_ID;
	pos++;
	len++;
	*pos = LENGTH_1_BYTE;
	pos++;
	len++;
	*pos = firmware_chip_id;
	pos++;
	len++;

	*pos = SLV_CONFIG_TEST_RAW_TP;
	pos++;
	len++;
	*pos = LENGTH_1_BYTE;
	pos++;
	len++;
	*pos = raw_tp_direction;
	pos++;
	len++;

	*pos = SLV_CONFIG_THROTTLE_HIGH_THRESHOLD;
	pos++;
	len++;
	*pos = LENGTH_1_BYTE;
	pos++;
	len++;
	*pos = high_thr_thesh;
	pos++;
	len++;

	*pos = SLV_CONFIG_THROTTLE_LOW_THRESHOLD;
	pos++;
	len++;
	*pos = LENGTH_1_BYTE;
	pos++;
	len++;
	*pos = low_thr_thesh;
	pos++;
	len++;

	/* TLVs end */

	event->event_len = len;

	/* payload len = Event len + sizeof(event type) + sizeof(event len) */
	len += 2;

	return esp_hosted_tx(card, ESP_PRIV_IF, 0, sendbuf, len);
}
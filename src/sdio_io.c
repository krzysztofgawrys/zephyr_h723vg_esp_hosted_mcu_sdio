/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

LOG_MODULE_REGISTER(sdio_io, CONFIG_SDHC_LOG_LEVEL);

static char chip_type = ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED;

void log_sdio_command(struct sdhc_command *cmd, int8_t ret)
{
	// Logowanie podstawowych parametrów komendy
	LOG_INF("cmd op=%d arg=%08x timeout=%d", cmd->opcode, cmd->arg, cmd->timeout_ms);

	LOG_INF("cmd response %08x %08x %08x %08x err=0x%x", cmd->response[0], cmd->response[1],
		cmd->response[2], cmd->response[3], ret);

	// Wyświetlanie odpowiedzi w zależności od jej typu
	// R2 ma 128 bitów (wszystkie 4 pola), reszta ma 32 bity (tylko response[0])
	if (cmd->response_type & SD_RSP_TYPE_R2) {
		LOG_INF("  Resp [R2]: 0x%08x %08x %08x %08x", cmd->response[0], cmd->response[1],
			cmd->response[2], cmd->response[3]);
	} else if (cmd->response_type != SD_RSP_TYPE_NONE) {
		LOG_INF("  Resp: 0x%08x", cmd->response[0]);

		// Dodatkowe dekodowanie dla najczęstszych komend SDIO
		if (cmd->opcode == 5) {
			LOG_INF("    [R4] Ready: %d, Functions: %d, OCR: 0x%06x",
				(cmd->response[0] >> 31) & 0x1, (cmd->response[0] >> 28) & 0x7,
				cmd->response[0] & 0xFFFFFF);
		} else if (cmd->opcode == 3) {
			LOG_INF("    [R6] RCA: 0x%04x, Status: 0x%04x",
				(cmd->response[0] >> 16) & 0xFFFF, cmd->response[0] & 0xFFFF);
		} else if (cmd->opcode == 52 || cmd->opcode == 53) {
			LOG_INF("    [R5] Flags: 0x%02x, Data: 0x%02x",
				(cmd->response[0] >> 8) & 0xFF, cmd->response[0] & 0xFF);
		}
	}
}

int sdio_io_read_byte(const struct device *dev, uint32_t function, uint32_t addr, uint8_t *out_byte)
{
	int8_t ret = sdio_io_rw_direct(dev, function, addr, SD_ARG_CMD52_READ, out_byte);
	if (unlikely(ret != 0)) {
		LOG_INF("sdio_io_read_byte: sdio_io_rw_direct (read 0x%" PRIx32 ") returned 0x%x",
			addr, ret);
	}
	return ret;
}

static uint8_t sdio_buffer[1536] __aligned(32) __attribute__((section(".nocache")));

int sdio_io_rw_extended(const struct device *dev, uint32_t func, uint32_t reg, uint32_t arg,
			void *datap, size_t datalen)
{
	int8_t err;

	struct sdhc_command cmd = {
		.opcode = SD_IO_RW_EXTENDED,
		.arg = arg,
		.response_type = SD_RSP_TYPE_R5,
		.timeout_ms = 1000,
	};

	uint32_t num_blocks = (datalen + BLOCKSIZE - 1) / BLOCKSIZE;
	uint32_t count;

	struct sdhc_data data_cfg = {
		.block_size = BLOCKSIZE,
		.blocks = num_blocks,
		.data = sdio_buffer,
		.timeout_ms = 1000,
	};

	memset(sdio_buffer, 0xcc, BLOCKSIZE);
	if (arg & SD_ARG_CMD53_WRITE) {
		memcpy(sdio_buffer, datap, datalen);
	}

	if (arg & SD_ARG_CMD53_BLOCK_MODE) {
		if (data_cfg.block_size % BLOCKSIZE != 0) {
			return -1;
		}
		count = data_cfg.block_size / BLOCKSIZE;
	} else {
		if (datalen > BLOCKSIZE) {
			return -1;
		}
		if (datalen == BLOCKSIZE) {
			count = 0;
		} else {
			count = datalen;
		}
		data_cfg.block_size = datalen;
	}

	arg |= (func & SD_ARG_CMD53_FUNC_MASK) << SD_ARG_CMD53_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD53_REG_MASK) << SD_ARG_CMD53_REG_SHIFT;
	arg |= (count & SD_ARG_CMD53_LENGTH_MASK) << SD_ARG_CMD53_LENGTH_SHIFT;
	cmd.arg = arg;

	LOG_INF("======== write ===========");
	LOG_HEXDUMP_INF(sdio_buffer, datalen, "DATA to write:");
	LOG_INF("=========================");

	err = sdhc_request(dev, &cmd, &data_cfg);
	log_sdio_command(&cmd, err);

	// Cannot do a normal bitmask check (arg & SD_ARG_CMD53_READ)
	// since SD_ARG_CMD53_READ (0<<31) is 0
	if (!(arg & SD_ARG_CMD53_WRITE) && datalen > 0) {
		memcpy(datap, sdio_buffer, datalen);
		LOG_INF("sdio_io_rw_extended: copied %zu bytes from aligned buffer", datalen);
		LOG_INF("======== read ===========");
		LOG_HEXDUMP_INF(sdio_buffer, datalen, "DATA read:");
		LOG_INF("=========================");
	}

	if (err != 0) {
		LOG_INF("sdio_io_rw_extended: sdhc_request returned 0x%x", err);
		return err;
	}

	return 0;
}

int sdio_io_read_bytes(const struct device *dev, uint32_t function, uint32_t addr, void *dst,
		       size_t size)
{
	uint32_t arg = SD_ARG_CMD53_READ;
	bool incr_addr = true;
	// Extract and unset the bit used to indicate the OP Code
	if (addr & SDMMC_IO_FIXED_ADDR) {
		addr &= ~SDMMC_IO_FIXED_ADDR;
		incr_addr = false;
	}
	if (incr_addr) {
		arg |= SD_ARG_CMD53_INCREMENT;
	}

	/* host quirk: SDIO transfer with length not divisible by 4 bytes
	 * has to be split into two transfers: one with aligned length,
	 * the other one for the remaining 1-3 bytes.
	 */
	uint8_t *pc_dst = dst;
	while (size > 0) {
		size_t size_aligned = size & (~3);
		size_t will_transfer = size_aligned > 0 ? size_aligned : size;

		// Note: sdmmc_io_rw_extended has an internal timeout,
		//  typically SDMMC_DEFAULT_CMD_TIMEOUT_MS
		int8_t err = sdio_io_rw_extended(dev, function, addr, arg, pc_dst, will_transfer);
		if (unlikely(err != 0)) {
			return err;
		}
		pc_dst += will_transfer;
		size -= will_transfer;
		if (incr_addr) {
			addr += will_transfer;
		}
	}
	return 0;
}

int sdio_io_write_bytes(const struct device *dev, uint32_t function, uint32_t addr, void *src,
			size_t size)
{
	uint32_t arg = SD_ARG_CMD53_WRITE;
	bool incr_addr = true;
	// Extract and unset the bit used to indicate the OP Code
	if (addr & SDMMC_IO_FIXED_ADDR) {
		addr &= ~SDMMC_IO_FIXED_ADDR;
		incr_addr = false;
	}
	if (incr_addr) {
		arg |= SD_ARG_CMD53_INCREMENT;
	}

	/* host quirk: SDIO transfer with length not divisible by 4 bytes
	 * has to be split into two transfers: one with aligned length,
	 * the other one for the remaining 1-3 bytes.
	 */
	uint8_t *pc_dst = src;
	while (size > 0) {
		size_t size_aligned = size & (~3);
		size_t will_transfer = size_aligned > 0 ? size_aligned : size;

		// Note: sdmmc_io_rw_extended has an internal timeout,
		//  typically SDMMC_DEFAULT_CMD_TIMEOUT_MS
		int8_t err = sdio_io_rw_extended(dev, function, addr, arg, pc_dst, will_transfer);
		if (unlikely(err != 0)) {
			return err;
		}
		pc_dst += will_transfer;
		size -= will_transfer;
		if (incr_addr) {
			addr += will_transfer;
		}
	}
	return 0;
}

int sdio_io_write_byte(const struct device *dev, uint32_t function, uint32_t addr, uint8_t in_byte,
		       uint8_t *out_byte)
{
	int8_t ret = sdio_io_rw_direct(dev, function, addr,
				       SD_ARG_CMD52_WRITE | SD_ARG_CMD52_EXCHANGE, &in_byte);
	if (unlikely(ret != 0)) {
		LOG_INF("sdio_io_write_byte: sdio_io_rw_direct (write 0x%" PRIx32 ") returned 0x%x",
			addr, ret);
	}
	if (out_byte != NULL) {
		*out_byte = in_byte;
	}
	return ret;
}

int sdio_io_rw_direct(const struct device *dev, int func, uint32_t reg, uint32_t arg, void *byte)
{
	int8_t ret;
	struct sdhc_command cmd = {.arg = 0, .opcode = SD_IO_RW_DIRECT, .timeout_ms = 1000};

	arg |= (func & SD_ARG_CMD52_FUNC_MASK) << SD_ARG_CMD52_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD52_REG_MASK) << SD_ARG_CMD52_REG_SHIFT;
	arg |= (*(uint8_t *)byte & SD_ARG_CMD52_DATA_MASK) << SD_ARG_CMD52_DATA_SHIFT;
	cmd.arg = arg;

	ret = sdhc_request(dev, &cmd, NULL);
	log_sdio_command(&cmd, ret);
	if (ret < 0) {
		LOG_INF("sdhc_request returned 0x%x", ret);
		return ret;
	}

	*(uint8_t *)byte = SD_R5_DATA(cmd.response);

	return 0;
}

int sdio_io_reset(const struct device *dev)
{
	uint8_t sdio_reset = CCCR_CTL_RES;

	int8_t ret = sdio_io_rw_direct(dev, 0, SD_IO_CCCR_CTL, SD_ARG_CMD52_WRITE, &sdio_reset);

	if (ret == -ETIMEDOUT) {
	} else if (ret == -ENOTSUP) {
		LOG_INF("sdio_io_reset: card not present");
		return ret;
	} else if (ret != 0) {
		LOG_INF("sdio_io_reset: unexpected return: 0x%x", ret);
		return ret;
	}
	return 0;
}

int sdio_send_cmd_go_idle_state(const struct device *dev)
{
	struct sdhc_command cmd = {
		.opcode = MMC_GO_IDLE_STATE, .arg = 0, .timeout_ms = 1000, .retries = 3};

	int8_t ret = sdhc_request(dev, &cmd, NULL);
	log_sdio_command(&cmd, ret);
	if (ret < 0) {
		LOG_INF("sdio_send_cmd_go_idle_state: CMD0 failed: %d", ret);
		return ret;
	}

	k_msleep(SDMMC_GO_IDLE_DELAY_MS);

	return 0;
}

int sdio_io_send_op_cond(const struct device *dev, uint32_t ocr, uint32_t *ocrp)
{
	int8_t ret = 0;
	struct sdhc_command cmd = {
		.arg = ocr, .opcode = SD_IO_SEND_OP_COND, .timeout_ms = 1000, .retries = 3};
	for (size_t i = 0; i < 100; i++) {
		ret = sdhc_request(dev, &cmd, NULL);
		log_sdio_command(&cmd, ret);
		if (ret < 0) {
			break;
		}
		if ((MMC_R4(cmd.response) & SD_IO_OCR_MEM_READY) || ocr == 0) {
			break;
		}
		ret = -ETIMEDOUT;
		k_msleep(SDMMC_IO_SEND_OP_COND_DELAY_MS);
	}
	if (ret == 0 && ocrp != NULL) {
		*ocrp = MMC_R4(cmd.response);
	}

	return ret;
}

int sdio_init_io(const struct device *dev)
{
	uint32_t ocrp = 0;
	int8_t err = 0;
	err = sdio_io_send_op_cond(dev, 0, &ocrp);
	if (err < 0) {
		LOG_INF("io_send_op_cond (1) returned 0x%x; not IO card", err);
	} else {
		if (ocrp & SD_IO_OCR_MEM_PRESENT) {
			LOG_INF("Combination card");
		} else {
			LOG_INF("IO-only card");
		}
		int num_io_functions = SD_IO_OCR_NUM_FUNCTIONS(ocrp);
		LOG_INF("number of IO functions: %d", num_io_functions);
		if (num_io_functions == 0) {
			LOG_INF("no IO functions");
		}
		err = sdio_io_send_op_cond(dev, SD_OCR_VOL_MASK, &ocrp);
		if (err < 0) {
			LOG_INF("sdio_io_send_op_cond (1) returned 0x%x", err);
			return err;
		}
	}
	return 0;
}

int sdio_send_cmd_set_relative_addr(const struct device *dev, uint16_t *out_rca)
{
	struct sdhc_command cmd = {.opcode = SD_SEND_RELATIVE_ADDR,
				   .response_type = SD_RSP_TYPE_R6,
				   .timeout_ms = 1000,
				   .retries = 3};

	int8_t err = sdhc_request(dev, &cmd, NULL);
	log_sdio_command(&cmd, err);
	if (err != 0) {
		return err;
	}

	uint16_t response_rca = SD_R6_RCA(cmd.response);
	if (response_rca == 0) {
		err = sdhc_request(dev, &cmd, NULL);
		log_sdio_command(&cmd, err);
		if (err != 0) {
			return err;
		}
		response_rca = SD_R6_RCA(cmd.response);
	}
	*out_rca = response_rca;
	return 0;
}

int sdio_send_cmd_select_card(const struct device *dev, uint32_t rca)
{
	struct sdhc_command cmd = {.opcode = MMC_SELECT_CARD,
				   .arg = MMC_ARG_RCA(rca),
				   .timeout_ms = 1000,
				   .retries = 3};
	int8_t ret = sdhc_request(dev, &cmd, NULL);
	log_sdio_command(&cmd, ret);
	if (ret != 0) {
		LOG_INF("sdio_send_cmd_select_card: sdhc_request returned 0x%x", ret);
	}
	return ret;
}

int sdio_init_select_card(const struct device *dev, uint32_t rca)
{
	int8_t err = sdio_send_cmd_select_card(dev, rca);
	if (err != 0) {
		LOG_INF("sdio_init_select_card: select_card returned 0x%x", err);
		return err;
	}
	return 0;
}

int sdio_io_init_read_card_cap(const struct device *dev, uint8_t *card_cap)
{
	int8_t err = 0;

	err = sdio_io_rw_direct(dev, 0, SD_IO_CCCR_CARD_CAP, SD_ARG_CMD52_READ, card_cap);
	if (err != 0) {
		LOG_INF("sdio_io_init_read_card_cap: sdio_io_rw_direct (read SD_IO_CCCR_CARD_CAP) "
			"returned 0x%x",
			err);
		return err;
	}

	return 0;
}

int sdio_read_reg(const struct device *dev, uint32_t reg, void *data, uint16_t size)
{
	int8_t res = 0;

	reg &= ESP_ADDRESS_MASK;

	if (size <= 1) {
		res = sdio_io_read_byte(dev, SDIO_FUNC_1, reg, data);
	} else {
		res = sdio_io_read_bytes(dev, SDIO_FUNC_1, reg, data, size);
	}
	if (res != 0) {
		LOG_INF("sdio_read_reg: sdio_io_read_byte (read 0x%" PRIx32 ") returned 0x%x", reg,
			res);
	}
	return res;
}

int sdio_write_reg(const struct device *dev, uint32_t reg, void *data, uint16_t size)
{
	int8_t res = 0;

	reg &= ESP_ADDRESS_MASK;

	if (size <= 1) {
		res = sdio_io_write_byte(dev, SDIO_FUNC_1, reg, *(uint8_t *)data, NULL);
	} else {
		res = sdio_io_write_bytes(dev, SDIO_FUNC_1, reg, data, size);
	}
	if (res != 0) {
		LOG_INF("sdio_write_reg: sdio_io_write_byte (write 0x%" PRIx32 ") returned 0x%x",
			reg, res);
	}
	return res;
}

static uint32_t sdio_rx_byte_count = 0;

int sdio_get_len_from_slave(uint32_t *rx_size, uint32_t reg_val)
{
	uint32_t len = reg_val;
	uint32_t temp;

	if (!rx_size) {
		return -1;
	}
	*rx_size = 0;

	len &= ESP_SLAVE_LEN_MASK;

	if (len >= sdio_rx_byte_count) {
		len = (len + ESP_RX_BYTE_MAX - sdio_rx_byte_count) % ESP_RX_BYTE_MAX;
	} else {
		/* Handle a case of roll over */
		temp = ESP_RX_BYTE_MAX - sdio_rx_byte_count;
		len = temp + len;
	}

#if H_SDIO_HOST_RX_MODE != H_SDIO_HOST_STREAMING_MODE
	if (len > ESP_RX_BUFFER_SIZE) {
		LOG_INF("sdio_get_len_from_slave: Len from slave[%ld] exceeds max [%d]", __func__,
			len, ESP_RX_BUFFER_SIZE);
		return -1;
	}
#endif

	*rx_size = len;

	return 0;
}

int sdio_generate_slave_intr(const struct device *card, uint8_t intr_no)
{
	uint8_t intr_mask = BIT(intr_no + ESP_SDIO_CONF_OFFSET);

	if (intr_no >= BIT(ESP_MAX_HOST_INTERRUPT)) {
		LOG_INF("sdio_generate_slave_intr: Invalid slave interrupt number");
		return -1;
	}

	uint32_t reg = HOST_TO_SLAVE_INTR;
	reg &= ESP_ADDRESS_MASK;
	int8_t ret = sdio_io_write_byte(card, SDIO_FUNC_1, reg, intr_mask, NULL);
	if (ret < 0) {
		LOG_INF("Failed to generate slave interrupt: %d", ret);
		return ret;
	}

	return 0;
}

int sdio_read_fromio(const struct device *card, uint32_t function, uint32_t addr, void *data,
		     uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	int8_t res;
	uint32_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_RX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdio_io_read_blocks(card, function, addr, ptr, size);
		if (res) {
			return res;
		}

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdio_io_read_bytes(card, function, addr, ptr, size);
		if (res) {
			return res;
		}

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return 0;
}

int sdio_write_toio(const struct device *card, uint32_t function, uint32_t addr, void *data,
		    uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	int8_t res;
	uint32_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_TX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdio_io_write_blocks(card, function, addr, ptr, size);
		if (res) {
			return res;
		}

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdio_io_write_bytes(card, function, addr, ptr, size);
		if (res) {
			return res;
		}

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return 0;
}

int sdio_io_read_blocks(const struct device *card, uint32_t function, uint32_t addr, void *dst,
			size_t size)
{
	uint32_t arg = SD_ARG_CMD53_READ | SD_ARG_CMD53_INCREMENT | SD_ARG_CMD53_BLOCK_MODE;
	// Extract and unset the bit used to indicate the OP Code (inverted logic)
	if (addr & SDMMC_IO_FIXED_ADDR) {
		arg &= ~SD_ARG_CMD53_INCREMENT;
		addr &= ~SDMMC_IO_FIXED_ADDR;
	}

	int8_t ret = sdio_io_rw_extended(card, function, addr, arg, dst, size);
	if (ret != 0) {
		LOG_INF("sdio_io_read_blocks: sdio_io_rw_extended returned 0x%x", ret);
	}
	return ret;
}

int sdio_io_write_blocks(const struct device *card, uint32_t function, uint32_t addr,
			 const void *src, size_t size)
{
	uint32_t arg = SD_ARG_CMD53_WRITE | SD_ARG_CMD53_INCREMENT | SD_ARG_CMD53_BLOCK_MODE;
	// Extract and unset the bit used to indicate the OP Code (inverted logic)
	if (addr & SDMMC_IO_FIXED_ADDR) {
		arg &= ~SD_ARG_CMD53_INCREMENT;
		addr &= ~SDMMC_IO_FIXED_ADDR;
	}

	int8_t ret = sdio_io_rw_extended(card, function, addr, arg, (void *)src, size);
	if (ret != 0) {
		LOG_INF("sdio_io_write_blocks: sdio_io_rw_extended returned 0x%x", ret);
	}
	return ret;
}

static double_buf_t double_buf;

uint8_t *sdio_rx_get_buffer(uint32_t len)
{
	len = ((len + ESP_BLOCK_SIZE - 1) / ESP_BLOCK_SIZE) * ESP_BLOCK_SIZE;
	int index = double_buf.write_index;
	uint8_t **buf = &double_buf.buffer[index].buf;

	if (len > double_buf.buffer[index].buf_size) {
		if (*buf) {
			// free already allocated memory
			k_free(*buf);
		}
		*buf = k_malloc(len);
		double_buf.buffer[index].buf_size = len;
		LOG_INF("sdio_rx_get_buffer: allocated buf %d size: %d", index,
			double_buf.buffer[index].buf_size);
	}
	return *buf;
}

void process_capabilities(uint8_t cap)
{
	LOG_INF("capabilities: 0x%x", cap);
}

void print_capabilities(uint32_t cap)
{
	LOG_INF("Features supported are:");
	if (cap & ESP_WLAN_SDIO_SUPPORT) {
		LOG_INF("\t * WLAN");
	}
	if (cap & ESP_BT_UART_SUPPORT) {
		LOG_INF("\t   - HCI over UART");
	}
	if (cap & ESP_BT_SDIO_SUPPORT) {
		LOG_INF("\t   - HCI over SDIO");
	}
	if (cap & ESP_BT_SPI_SUPPORT) {
		LOG_INF("\t   - HCI over SPI");
	}
	if ((cap & ESP_BLE_ONLY_SUPPORT) && (cap & ESP_BR_EDR_ONLY_SUPPORT)) {
		LOG_INF("\t   - BT/BLE dual mode");
	} else if (cap & ESP_BLE_ONLY_SUPPORT) {
		LOG_INF("\t   - BLE only");
	} else if (cap & ESP_BR_EDR_ONLY_SUPPORT) {
		LOG_INF("\t   - BR EDR only");
	}
}

static uint32_t process_ext_capabilities(uint8_t *ptr)
{
	// ptr address may be not be 32-bit aligned
	uint32_t cap;

	cap = (uint32_t)ptr[0] + ((uint32_t)ptr[1] << 8) + ((uint32_t)ptr[2] << 16) +
	      ((uint32_t)ptr[3] << 24);
	LOG_INF("extended capabilities: 0x%" PRIx32, cap);

	return cap;
}

static void print_ext_capabilities(uint8_t *ptr)
{
	// ptr address may be not be 32-bit aligned
	uint32_t cap;

	cap = (uint32_t)ptr[0] + ((uint32_t)ptr[1] << 8) + ((uint32_t)ptr[2] << 16) +
	      ((uint32_t)ptr[3] << 24);

	LOG_INF("Extended Features supported:");
	LOG_INF("\t No extended features. capabilities[%" PRIu32 "]", cap);
}

static int get_chip_str_from_id(int chip_id, char *chip_str)
{
	int ret = 0;

	switch (chip_id) {
	case ESP_PRIV_FIRMWARE_CHIP_ESP32:
		strcpy(chip_str, "esp32");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C2:
		strcpy(chip_str, "esp32c2");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C3:
		strcpy(chip_str, "esp32c3");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C6:
		strcpy(chip_str, "esp32c6");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32S2:
		strcpy(chip_str, "esp32s2");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32S3:
		strcpy(chip_str, "esp32s3");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C5:
		strcpy(chip_str, "esp32c5");
		break;
	default:
		LOG_INF("Unsupported chip id: %u", chip_id);
		strcpy(chip_str, "unsupported");
		ret = -1;
		break;
	}
	return ret;
}

static void verify_host_config_for_slave(uint8_t chip_type)
{
	uint8_t exp_chip_id = 0xff;

#if H_SLAVE_TARGET_ESP32
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32;
#elif H_SLAVE_TARGET_ESP32C2
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C2;
#elif H_SLAVE_TARGET_ESP32C3
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C3;
#elif H_SLAVE_TARGET_ESP32C6
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C6;
#elif H_SLAVE_TARGET_ESP32S2
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32S2;
#elif H_SLAVE_TARGET_ESP32S3
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32S3;
#elif H_SLAVE_TARGET_ESP32C5
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C5;
#else
	LOG_INF("Incorrect host config for ESP slave chipset[%x]", chip_type);
#endif
	if (chip_type != exp_chip_id) {
		char slave_str[20], exp_str[20];

		memset(slave_str, '\0', 20);
		memset(exp_str, '\0', 20);

		get_chip_str_from_id(chip_type, slave_str);
		get_chip_str_from_id(exp_chip_id, exp_str);
		LOG_INF("Identified slave [%s] != Expected [%s]\n\t\trun 'idf.py menuconfig' at "
			"host to reselect the slave?\n\t\tAborting.. ",
			slave_str, exp_str);
		k_msleep(10);
	}
}

int process_init_event(const struct device *card, uint8_t *evt_buf, uint16_t len)
{
	uint8_t len_left = len, tag_len;
	uint8_t *pos;
	uint32_t ext_cap = 0;
	uint32_t slave_fw_version = 0;
	uint8_t raw_tp_config = H_TEST_RAW_TP_DIR;

	if (!evt_buf) {
		return -1;
	}

	pos = evt_buf;
	LOG_INF("Init event length: %u", len);
	if (len > 64) {
		LOG_INF("Init event length: %u", len);
	}

	while (len_left) {
		tag_len = *(pos + 1);
		LOG_INF("EVENT: %2x", *pos);

		if (*pos == ESP_PRIV_CAPABILITY) {
			process_capabilities(*(pos + 2));
			print_capabilities(*(pos + 2));
		} else if (*pos == ESP_PRIV_CAP_EXT) {
			ext_cap = process_ext_capabilities(pos + 2);
			print_ext_capabilities(pos + 2);
		} else if (*pos == ESP_PRIV_FIRMWARE_CHIP_ID) {
			chip_type = *(pos + 2);
			verify_host_config_for_slave(chip_type);
		} else if (*pos == ESP_PRIV_TEST_RAW_TP) {
			if (*(pos + 2)) {
				LOG_INF("Slave enabled Raw Throughput Testing, but not enabled on "
					"Host");
			}
		} else if (*pos == ESP_PRIV_RX_Q_SIZE) {
			LOG_INF("slave rx queue size: %u", *(pos + 2));
		} else if (*pos == ESP_PRIV_TX_Q_SIZE) {
			LOG_INF("slave tx queue size: %u", *(pos + 2));
		} else if (*pos == ESP_PRIV_FIRMWARE_VERSION) {
			slave_fw_version = *(pos + 2) | (*(pos + 3) << 8) | (*(pos + 4) << 16) |
					   (*(pos + 5) << 24);
			LOG_INF("slave fw version: 0x%08" PRIx32, slave_fw_version);
		} else {
			LOG_INF("Unsupported EVENT: %2x", *pos);
		}
		pos += (tag_len + 2);
		len_left -= (tag_len + 2);
	}

	if ((chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S2) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S3) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C2) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C3) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C6) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C5)) {
		LOG_INF("ESP board type is not mentioned, ignoring [%d]\n\r", chip_type);
		chip_type = ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED;
		return -1;
	} else {
		LOG_INF("ESP board type is : %d \n\r", chip_type);
	}

	send_slave_config(card, 0, chip_type, raw_tp_config, H_WIFI_TX_DATA_THROTTLE_LOW_THRESHOLD,
			  H_WIFI_TX_DATA_THROTTLE_HIGH_THRESHOLD);

	return 0;
}

void process_event(const struct device *card, uint8_t *evt_buf, uint16_t len)
{
	int ret = 0;
	struct esp_priv_event *event;

	if (!evt_buf || !len) {
		return;
	}

	event = (struct esp_priv_event *)evt_buf;

	if (event->event_type == ESP_PRIV_EVENT_INIT) {

		LOG_INF("process_event: Received INIT event from ESP32 peripheral");
		LOG_HEXDUMP_INF(event->event_data, event->event_len, "Slave_init_evt");

		ret = process_init_event(card, event->event_data, event->event_len);
		if (ret) {
			LOG_INF("process_event: failed to init event");
		}
	} else {
		LOG_INF("process_event: Drop unknown event");
	}
}

void process_priv_communication(const struct device *card, interface_buffer_handle_t *buf_handle)
{
	if (!buf_handle || !buf_handle->payload || !buf_handle->payload_len) {
		return;
	}

	process_event(card, buf_handle->payload, buf_handle->payload_len);
}

int is_valid_sdio_rx_packet(uint8_t *rxbuff_a, uint16_t *len_a, uint16_t *offset_a)
{
	struct esp_payload_header *h = (struct esp_payload_header *)rxbuff_a;
	uint16_t len = 0, offset = 0;

	if (!h || !len_a || !offset_a) {
		return 0;
	}

	/* Fetch length and offset from payload header */
	len = le16toh(h->len);
	offset = le16toh(h->offset);

	if ((!len) || (len > MAX_PAYLOAD_SIZE) || (offset != sizeof(struct esp_payload_header))) {

		/* Free up buffer, as one of following -
		 * 1. no payload to process
		 * 2. input packet size > driver capacity
		 * 3. payload header size mismatch,
		 * wrong header/bit packing?
		 * */
		return 0;
	}

	*len_a = len;
	*offset_a = offset;

	return 1;
}

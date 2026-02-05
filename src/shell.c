/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/sdhc.h>
#include <zephyr/logging/log.h>
#include <stm32_ll_bus.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <esp_hosted_rpc.pb.h>
#include <zephyr/cache.h>
#include <stm32_ll_gpio.h>
#include <endian.h>
#include "sdio_io.h"

LOG_MODULE_REGISTER(sdio_shell, LOG_LEVEL_DBG);

static const struct device *get_sdhc_dev(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(sdmmc1));
	if (!device_is_ready(dev)) {
		return NULL;
	}
	return dev;
}

static int cmd_sdio_reset(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	int ret = sdio_io_reset(dev);
	if (ret < 0) {
		LOG_INF("Failed to reset SDIO card: %d", ret);
		return ret;
	}
	LOG_INF("SDIO card reset successfully");
	return 0;
}

static int cmd_go_idle_state(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	int ret = sdio_send_cmd_go_idle_state(dev);
	if (ret < 0) {
		LOG_INF("Failed to send CMD0 to SDIO card: %d", ret);
		return ret;
	}
	LOG_INF("CMD0 sent successfully to SDIO card");
	return 0;
}

static int cmd_init_io(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	int ret = sdio_init_io(dev);
	if (ret < 0) {
		LOG_INF("Failed to init SDIO IO functions: %d", ret);
		return ret;
	}
	LOG_INF("SDIO IO functions initialized successfully");
	return 0;
}

static int cmd_init_select_card(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	uint16_t rca = 0;
	int ret = sdio_send_cmd_set_relative_addr(dev, &rca);
	if (ret < 0) {
		LOG_INF("Failed to get SDIO RCA: %d", ret);
		return ret;
	}
	ret = sdio_init_select_card(dev, rca);
	if (ret < 0) {
		LOG_INF("Failed to select SDIO card: %d", ret);
		return ret;
	}
	LOG_INF("SDIO card selected successfully");
	return 0;
}

static int cmd_read_card_cap(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	uint8_t card_cap = 0;
	int ret = sdio_io_init_read_card_cap(dev, &card_cap);
	if (ret < 0) {
		LOG_INF("Failed to read SDIO card capabilities: %d", ret);
		return ret;
	}
	LOG_INF("SDIO card capabilities read successfully: 0x%02x", card_cap);
	return 0;
}

static int cmd_fn_init(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	int ret = hosted_sdio_card_fn_init(dev);
	if (ret < 0) {
		LOG_INF("Failed to initialize SDIO card functions: %d", ret);
		return ret;
	}
	LOG_INF("SDIO card functions initialized successfully");
	return 0;
}

static void esp_sdio_irq_cb(const struct device *dev, int reason, const void *user_data)
{
    // struct esp_hosted *ctx = user_data;
    // k_sem_give(&ctx->irq_sem);
}

static int cmd_slave_intr(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}

	uint8_t f1_ready = 0;
	for (int i = 0; i < 100; i++) {
		// Odczyt rejestru 0x03 w Funkcji 0
		sdio_io_read_byte(dev, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &f1_ready);
		if (f1_ready & 0x02) {
			LOG_INF("ESP32-C6 Function 1 is READY");
			break;
		}
		k_msleep(10);
	}

	if (!(f1_ready & 0x02)) {
		LOG_ERR("ESP32-C6 Function 1 TIMEOUT - nie wysyłaj przerwania!");
		return -ETIMEDOUT;
	}

	void *ctx;

	sdhc_enable_interrupt(dev, esp_sdio_irq_cb, SDMMC_STA_SDIOIT, &ctx);

	int ret = sdio_generate_slave_intr(dev, ESP_OPEN_DATA_PATH);

    // k_sem_take(&ctx->irq_sem, K_FOREVER);

	if (ret < 0) {
		LOG_INF("Failed to generate slave interrupt %d: %d", ESP_OPEN_DATA_PATH, ret);
		return ret;
	}
	LOG_INF("Slave interrupt %d generated successfully", ESP_OPEN_DATA_PATH);
	return 0;
}

static int cmd_sdio_read_reg(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 2) {
		shell_error(sh, "Usage: sdio read_reg <reg>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	uint32_t data = 0;
	int ret = sdio_read_reg(dev, reg, (char *)&data, 4);
	if (ret < 0) {
		LOG_INF("Failed to read SDIO register 0x%08x: %d", reg, ret);
		return ret;
	}
	LOG_INF("SDIO register 0x%08x read successfully: 0x%08x", reg, data);
	return 0;
}

static int cmd_sdio_write_reg(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 3) {
		shell_error(sh, "Usage: sdio write_reg <reg> <data>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	uint8_t data = strtoul(argv[2], NULL, 0);
	int ret = sdio_write_reg(dev, reg, &data, 1);
	if (ret < 0) {
		LOG_INF("Failed to write SDIO register 0x%08x: %d", reg, ret);
		return ret;
	}
	LOG_INF("SDIO register 0x%08x written successfully: 0x%02x", reg, data);
	return 0;
}

static int cmd_sdio_read_ext(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 3) {
		shell_error(sh, "Usage: sdio read_ext <reg> <length>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	size_t length = strtoul(argv[2], NULL, 0);
	uint8_t data[512];
	// if (!data) {
	// 	LOG_INF("Failed to allocate memory for SDIO read_ext");
	// 	return -ENOMEM;
	// }
	int ret = sdio_io_rw_extended(dev, SDIO_FUNC_1, reg,
				      SD_ARG_CMD53_READ | SD_ARG_CMD53_INCREMENT, data, length);
	if (ret < 0) {
		LOG_INF("Failed to read SDIO extended 0x%08x: %d", reg, ret);
		k_free(data);
		return ret;
	}
	LOG_INF("SDIO extended 0x%08x read successfully:", reg);
	LOG_HEXDUMP_INF(data, length, NULL);
	k_free(data);
	return 0;
}

static int cmd_sdio_write_ext(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 4) {
		shell_error(sh, "Usage: sdio write_ext <reg> <length> <data_byte>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	size_t length = strtoul(argv[2], NULL, 0);
	uint8_t data_byte = strtoul(argv[3], NULL, 0);
	uint8_t *data = k_malloc(length);
	if (!data) {
		LOG_INF("Failed to allocate memory for SDIO write_ext");
		return -ENOMEM;
	}
	memset(data, data_byte, length);
	int ret = sdio_io_rw_extended(dev, SDIO_FUNC_1, reg,
				      SD_ARG_CMD53_WRITE | SD_ARG_CMD53_INCREMENT, data, length);
	if (ret < 0) {
		LOG_INF("Failed to write SDIO extended 0x%08x: %d", reg, ret);
		k_free(data);
		return ret;
	}
	LOG_INF("SDIO extended 0x%08x written successfully:", reg);
	LOG_HEXDUMP_INF(data, length, NULL);
	k_free(data);
	return 0;
}

static int cmd_get_intr_status(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	uint8_t intr_status[REG_BUF_LEN] = {0};
	int ret = sdio_read_reg(dev, ESP_SLAVE_INT_RAW_REG, (char *)intr_status, REG_BUF_LEN);
	if (ret < 0) {
		LOG_INF("Failed to get SDIO interrupt status: %d", ret);
		return ret;
	}
	LOG_INF("SDIO interrupt status read successfully:");
	LOG_HEXDUMP_INF(intr_status, REG_BUF_LEN, "Registers:");

	uint32_t *intr_index = (uint32_t *)&intr_status[0];
	uint32_t *read_len_index = (uint32_t *)&intr_status[PACKET_LEN_INDEX];

	uint32_t interrupts = *intr_index;

	LOG_INF("Intr: %08" PRIX32, interrupts);

	uint32_t len_from_slave;

	ret = sdio_get_len_from_slave(&len_from_slave, *read_len_index);
	if (ret < 0) {
		LOG_INF("Failed to get length from slave: %d", ret);
		return ret;
	}
	LOG_INF("Length from slave: %u", len_from_slave);

	ret = sdio_write_reg(dev, ESP_SLAVE_INT_CLR_REG, &interrupts, sizeof(interrupts));
	if (ret < 0) {
		LOG_INF("Failed to clear interrupt status: %d", ret);
		return ret;
	}
	LOG_INF("Interrupt status cleared successfully");

	uint8_t *data = sdio_rx_get_buffer(len_from_slave);
	if (!data) {
		LOG_INF("Failed to allocate memory for SDIO read_blocks");
		return -ENOMEM;
	}

	uint32_t block_read_len =
		((len_from_slave + ESP_BLOCK_SIZE - 1) / ESP_BLOCK_SIZE) * ESP_BLOCK_SIZE;
	ret = hosted_sdio_read_block(dev, ESP_SLAVE_CMD53_END_ADDR - len_from_slave, data,
				     block_read_len, false);
	if (ret < 0) {
		LOG_INF("Failed to read SDIO blocks 0x%08x: %d",
			ESP_SLAVE_CMD53_END_ADDR - len_from_slave, ret);
		k_free(data);
		return ret;
	}
	LOG_INF("SDIO blocks 0x%08x read successfully:", ESP_SLAVE_CMD53_END_ADDR - len_from_slave);
	LOG_HEXDUMP_INF(data, len_from_slave, NULL);

	struct esp_payload_header *h = NULL;
	interface_buffer_handle_t *buf_handle = NULL;

	h = (struct esp_payload_header *)data;

	buf_handle = k_malloc(sizeof(interface_buffer_handle_t));
	if (!buf_handle) {
		LOG_INF("Failed to allocate memory for interface_buffer_handle_t");
		k_free(data);
		return -ENOMEM;
	}

	uint16_t len = 0;
	uint16_t offset = 0;

	if (!is_valid_sdio_rx_packet(data, &len, &offset)) {
		LOG_INF("Dropping packet(s) from stream");
		k_free(buf_handle);
		k_free(data);
		return -1;
	}

	buf_handle->priv_buffer_handle = data;
	buf_handle->payload_len = len;
	buf_handle->if_type = h->if_type;
	buf_handle->if_num = h->if_num;
	buf_handle->payload = data + offset;
	buf_handle->seq_num = le16toh(h->seq_num);
	buf_handle->flag = h->flags;

	process_priv_communication(dev, buf_handle);
	k_free(data);
	k_free(buf_handle);

	return 0;
}

static int cmd_sdio_read_blocks(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 3) {
		shell_error(sh, "Usage: sdio read_blocks <reg> <length>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	size_t length = strtoul(argv[2], NULL, 0);
	uint8_t *data = k_malloc(length);
	if (!data) {
		LOG_INF("Failed to allocate memory for SDIO read_blocks");
		return -ENOMEM;
	}
	uint32_t block_read_len = ((length + ESP_BLOCK_SIZE - 1) / ESP_BLOCK_SIZE) * ESP_BLOCK_SIZE;
	int ret = hosted_sdio_read_block(dev, reg, data, block_read_len, false);
	if (ret < 0) {
		LOG_INF("Failed to read SDIO blocks 0x%08x: %d", reg, ret);
		k_free(data);
		return ret;
	}
	LOG_INF("SDIO blocks 0x%08x read successfully:", reg);
	LOG_HEXDUMP_INF(data, length, NULL);
	k_free(data);
	return 0;
}

static int cmd_sdio_write_blocks(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	if (argc < 4) {
		shell_error(sh, "Usage: sdio write_blocks <reg> <length> <data_byte>");
		return -EINVAL;
	}
	uint32_t reg = strtoul(argv[1], NULL, 0);
	size_t length = strtoul(argv[2], NULL, 0);
	uint8_t data_byte = strtoul(argv[3], NULL, 0);
	uint8_t *data = k_malloc(length);
	if (!data) {
		LOG_INF("Failed to allocate memory for SDIO write_blocks");
		return -ENOMEM;
	}
	memset(data, data_byte, length);
	int ret = sdio_io_write_blocks(dev, SDIO_FUNC_1, reg, data, length);
	if (ret < 0) {
		LOG_INF("Failed to write SDIO blocks 0x%08x: %d", reg, ret);
		k_free(data);
		return ret;
	}
	LOG_INF("SDIO blocks 0x%08x written successfully:", reg);
	LOG_HEXDUMP_INF(data, length, NULL);
	k_free(data);
	return 0;
}

int cmd_sdio_all(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}
	int ret;

	ret = cmd_sdio_reset(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_sdio_reset failed, with err: %d", ret);
	}

	ret = cmd_go_idle_state(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_go_idle_state failed, with err: %d", ret);
	}

	ret = cmd_init_io(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_init_io failed, with err: %d", ret);
	}

	ret = cmd_init_select_card(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_init_select_card failed, with err: %d", ret);
	}

	ret = cmd_read_card_cap(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_read_card_cap failed, with err: %d", ret);
	}

	ret = cmd_fn_init(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_fn_init failed, with err: %d", ret);
	}

	ret = cmd_slave_intr(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_slave_intr failed, with err: %d", ret);
	}

	ret = cmd_get_intr_status(sh, 0, NULL);
	if (ret < 0) {
		LOG_INF("cmd_get_intr_status failed, with err: %d", ret);
	}

	return 0;
}

int cmd_sdio_scan(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}

	Rpc msg = Rpc_init_default;
	msg.msg_type = RpcType_Req;
	msg.msg_id = RpcId_Req_GetMACAddress;
	msg.uid = RpcId_Req_GetMACAddress;
	msg.which_payload = Rpc_req_get_mac_address_tag;
	msg.payload.req_get_mac_address.mode = 0; // Scan WiFi networks

	uint8_t payload_buf[128];
	memset(payload_buf, 0, sizeof(payload_buf));

	// 2. Nagłówek TLV (identyczny jak w działającym wzorcu)
	payload_buf[0] = 0x01; // Type: EP
	payload_buf[1] = 0x06; // Len: 6
	payload_buf[2] = 0x00; // Padding
	memcpy(&payload_buf[3], "RPCRsp", 6);

	payload_buf[9] = 0x02; // Type: DATA
	// payload_buf[10] zostanie ustawione po zakodowaniu
	payload_buf[11] = 0x00; // Padding

	// 3. TUTAJ ląduje ctrl_msg!
	// Tworzymy stream Nanopb wskazujący na 12-ty bajt naszego bufora.
	pb_ostream_t stream = pb_ostream_from_buffer(&payload_buf[12], sizeof(payload_buf) - 12);

	// Kodujemy naszą strukturę bezpośrednio do payload_buf[12...]
	if (!pb_encode(&stream, Rpc_fields, &msg)) {
		LOG_INF("Nanopb encode failed: %s", PB_GET_ERROR(&stream));
		return -1;
	}

	// 4. Pobieramy realny rozmiar zakodowanego Protobufa
	size_t pb_size = stream.bytes_written;
	payload_buf[10] = (uint8_t)pb_size; // Wpisujemy go do TLV Header

	// 5. Wysyłamy całość (12 bajtów TLV + rozmiar Protobufa)
	uint32_t total_len = 12 + pb_size;

	uint8_t *sendbuf = k_malloc(total_len);
	memset(sendbuf, 0, total_len);
	if (!sendbuf) {
		LOG_INF("cmd_sdio_scan: Failed to allocate memory for sendbuf");
		k_free(sendbuf);
		return -ENOMEM;
	}

	memcpy(sendbuf, &payload_buf, total_len);

	int ret = esp_hosted_tx(dev, ESP_SERIAL_IF, 0, sendbuf, total_len);
	if (ret < 0) {
		LOG_INF("cmd_sdio_scan: Failed to send esp_hosted_tx: %d", ret);
		k_free(sendbuf);
		return ret;
	}

	k_free(sendbuf);
	LOG_INF("WIFI scanned successfully");
	return 0;
}

int cmd_wifi_sta(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = get_sdhc_dev();
	if (!dev) {
		LOG_INF("SDHC dev not ready");
		return -ENODEV;
	}

	wifi_init_config cfg = wifi_init_config_init_default;

	cfg.magic = 0x1F2F3F4F;
	cfg.static_rx_buf_num = 10;  // Zazwyczaj min. 10
	cfg.dynamic_rx_buf_num = 32; // Zazwyczaj 32
	cfg.static_tx_buf_num = 0;   // Może być 0 jeśli tx_buf_type to dynamic
	cfg.dynamic_tx_buf_num = 32;
	cfg.tx_buf_type = 1; // 1 = WIFI_TX_BUF_DYNAMIC
	cfg.cache_tx_buf_num = 0;
	cfg.ampdu_rx_enable = 1;
	cfg.ampdu_tx_enable = 1;
	cfg.nvs_enable = 0; // Na początku lepiej wyłączyć NVS, żeby nie komplikować
	cfg.nano_enable = 0;
	cfg.rx_ba_win = 6; // Standardowa wartość okna
	cfg.wifi_task_core_id = 0;
	cfg.beacon_max_len = 752;
	cfg.mgmt_sbuf_num = 32;
	cfg.feature_caps = 121;

	Rpc msg = Rpc_init_default;
	msg.msg_type = RpcType_Req;
	msg.msg_id = RpcId_Req_WifiInit;
	msg.uid = RpcId_Req_WifiInit;
	msg.which_payload = Rpc_req_wifi_init_tag;
	msg.payload.req_wifi_init.has_cfg = true;
	msg.payload.req_wifi_init.cfg = cfg;

	uint8_t payload_buf[128];
	memset(payload_buf, 0, sizeof(payload_buf));

	// 2. Nagłówek TLV (identyczny jak w działającym wzorcu)
	payload_buf[0] = 0x01; // Type: EP
	payload_buf[1] = 0x06; // Len: 6
	payload_buf[2] = 0x00; // Padding
	memcpy(&payload_buf[3], "RPCRsp", 6);

	payload_buf[9] = 0x02; // Type: DATA
	// payload_buf[10] zostanie ustawione po zakodowaniu
	payload_buf[11] = 0x00; // Padding

	// 3. TUTAJ ląduje ctrl_msg!
	// Tworzymy stream Nanopb wskazujący na 12-ty bajt naszego bufora.
	pb_ostream_t stream = pb_ostream_from_buffer(&payload_buf[12], sizeof(payload_buf) - 12);

	// Kodujemy naszą strukturę bezpośrednio do payload_buf[12...]
	if (!pb_encode(&stream, Rpc_fields, &msg)) {
		LOG_INF("Nanopb encode failed: %s", PB_GET_ERROR(&stream));
		return -1;
	}

	// 4. Pobieramy realny rozmiar zakodowanego Protobufa
	size_t pb_size = stream.bytes_written;
	payload_buf[10] = (uint8_t)pb_size; // Wpisujemy go do TLV Header

	// 5. Wysyłamy całość (12 bajtów TLV + rozmiar Protobufa)
	uint32_t total_len = 12 + pb_size;

	uint8_t *sendbuf = k_malloc(total_len);
	memset(sendbuf, 0, total_len);
	if (!sendbuf) {
		LOG_INF("cmd_sdio_scan: Failed to allocate memory for sendbuf");
		k_free(sendbuf);
		return -ENOMEM;
	}

	memcpy(sendbuf, &payload_buf, total_len);

	int ret = esp_hosted_tx(dev, ESP_SERIAL_IF, 0, sendbuf, total_len);
	if (ret < 0) {
		LOG_INF("cmd_wifi_sta: Failed to set STA mode: %d", ret);
		k_free(sendbuf);
		return ret;
	}

	k_free(sendbuf);
	LOG_INF("Set WIFI STA MODE successfully");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_sdio, SHELL_CMD_ARG(reset, NULL, "Reset SDIO Card", cmd_sdio_reset, 1, 0),
	SHELL_CMD_ARG(idle, NULL, "Send idle command", cmd_go_idle_state, 1, 0),
	SHELL_CMD_ARG(init_io, NULL, "Initialize SDIO IO functions", cmd_init_io, 1, 0),
	SHELL_CMD_ARG(select, NULL, "Select SDIO card", cmd_init_select_card, 1, 0),
	SHELL_CMD_ARG(read_card_cap, NULL, "Read SDIO card capabilities", cmd_read_card_cap, 1, 0),
	SHELL_CMD_ARG(fn_init, NULL, "Initialize SDIO card functions", cmd_fn_init, 1, 0),
	SHELL_CMD_ARG(slave_intr, NULL, "Generate slave interrupt", cmd_slave_intr, 1, 0),
	SHELL_CMD_ARG(read_reg, NULL, "Read SDIO register", cmd_sdio_read_reg, 2, 0),
	SHELL_CMD_ARG(write_reg, NULL, "Write SDIO register", cmd_sdio_write_reg, 3, 0),
	SHELL_CMD_ARG(read_ext, NULL, "Read SDIO extended", cmd_sdio_read_ext, 3, 0),
	SHELL_CMD_ARG(write_ext, NULL, "Write SDIO extended", cmd_sdio_write_ext, 4, 0),
	SHELL_CMD_ARG(get_intr_status, NULL, "Get interrupt status", cmd_get_intr_status, 1, 0),
	SHELL_CMD_ARG(read_blocks, NULL, "Read SDIO blocks", cmd_sdio_read_blocks, 3, 0),
	SHELL_CMD_ARG(write_blocks, NULL, "Write SDIO blocks", cmd_sdio_write_blocks, 4, 0),
	SHELL_CMD_ARG(all, NULL, "Write SDIO blocks", cmd_sdio_all, 1, 0),
	SHELL_CMD_ARG(scan, NULL, "Scan SDIO bus for cards", cmd_sdio_scan, 1, 0),
	SHELL_CMD_ARG(sta, NULL, "Scan SDIO bus for cards", cmd_wifi_sta, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(sd, &sub_sdio, "SDIO debug commands", NULL);

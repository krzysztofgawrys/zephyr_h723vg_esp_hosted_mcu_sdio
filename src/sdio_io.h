#include <zephyr/logging/log.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <esp_hosted_rpc.pb.h>

#ifndef SDIO_IO_H
#define SDIO_IO_H

#define SCF_CMD_AC 0x0000

#define SCF_ITSDONE     0x0001 /*!< command is complete */
#define SCF_CMD(flags)  ((flags) & 0x00f0)
#define SCF_CMD_AC      0x0000
#define SCF_CMD_ADTC    0x0010
#define SCF_CMD_BC      0x0020
#define SCF_CMD_BCR     0x0030
#define SCF_CMD_READ    0x0040 /*!< read command (data expected) */
#define SCF_RSP_BSY     0x0100
#define SCF_RSP_136     0x0200
#define SCF_RSP_CRC     0x0400
#define SCF_RSP_IDX     0x0800
#define SCF_RSP_PRESENT 0x1000
/* response types */
#define SCF_RSP_R0      0 /*!< none */
#define SCF_RSP_R1      (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX)
#define SCF_RSP_R1B     (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX | SCF_RSP_BSY)
#define SCF_RSP_R2      (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_136)
#define SCF_RSP_R3      (SCF_RSP_PRESENT)
#define SCF_RSP_R4      (SCF_RSP_PRESENT)
#define SCF_RSP_R5      (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX)
#define SCF_RSP_R5B     (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX | SCF_RSP_BSY)
#define SCF_RSP_R6      (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX)
#define SCF_RSP_R7      (SCF_RSP_PRESENT | SCF_RSP_CRC | SCF_RSP_IDX)

#define CCCR_CTL_RES   (1 << 3)
#define SD_IO_CCCR_CTL 0x06

/* SD R4 response (IO OCR) */
#define SD_IO_OCR_MEM_READY          (1 << 31)
#define SD_IO_OCR_NUM_FUNCTIONS(ocr) (((ocr) >> 28) & 0x7)
#define SD_IO_OCR_MEM_PRESENT        (1 << 27)
#define SD_IO_OCR_MASK               0x00fffff0

/* CMD52 arguments */
#define SD_ARG_CMD52_READ       (0 << 31)
#define SD_ARG_CMD52_WRITE      (1 << 31)
#define SD_ARG_CMD52_FUNC_SHIFT 28
#define SD_ARG_CMD52_FUNC_MASK  0x7
#define SD_ARG_CMD52_EXCHANGE   (1 << 27)
#define SD_ARG_CMD52_REG_SHIFT  9
#define SD_ARG_CMD52_REG_MASK   0x1ffff
#define SD_ARG_CMD52_DATA_SHIFT 0
#define SD_ARG_CMD52_DATA_MASK  0xff
#define SD_R5_DATA(resp)        ((resp)[0] & 0xff)

/* CMD53 arguments */
#define SD_ARG_CMD53_READ         (0 << 31)
#define SD_ARG_CMD53_WRITE        (1 << 31)
#define SD_ARG_CMD53_FUNC_SHIFT   28
#define SD_ARG_CMD53_FUNC_MASK    0x7
#define SD_ARG_CMD53_BLOCK_MODE   (1 << 27)
#define SD_ARG_CMD53_INCREMENT    (1 << 26)
#define SD_ARG_CMD53_REG_SHIFT    9
#define SD_ARG_CMD53_REG_MASK     0x1ffff
#define SD_ARG_CMD53_LENGTH_SHIFT 0
#define SD_ARG_CMD53_LENGTH_MASK  0x1ff
#define SD_ARG_CMD53_LENGTH_MAX   512

#define SDMMC_IO_FIXED_ADDR BIT(31)

/* MMC commands */                  /* response type */
#define MMC_GO_IDLE_STATE        0  /* R0 */
#define MMC_SEND_OP_COND         1  /* R3 */
#define MMC_ALL_SEND_CID         2  /* R2 */
#define MMC_SET_RELATIVE_ADDR    3  /* R1 */
#define MMC_SWITCH               6  /* R1B */
#define MMC_SELECT_CARD          7  /* R1 */
#define MMC_SEND_EXT_CSD         8  /* R1 */
#define MMC_SEND_CSD             9  /* R2 */
#define MMC_SEND_CID             10 /* R1 */
#define MMC_READ_DAT_UNTIL_STOP  11 /* R1 */
#define MMC_STOP_TRANSMISSION    12 /* R1B */
#define MMC_SEND_STATUS          13 /* R1 */
#define MMC_SET_BLOCKLEN         16 /* R1 */
#define MMC_READ_BLOCK_SINGLE    17 /* R1 */
#define MMC_READ_BLOCK_MULTIPLE  18 /* R1 */
#define MMC_SEND_TUNING_BLOCK    19 /* R1 */
#define MMC_WRITE_DAT_UNTIL_STOP 20 /* R1 */
#define MMC_SET_BLOCK_COUNT      23 /* R1 */
#define MMC_WRITE_BLOCK_SINGLE   24 /* R1 */
#define MMC_WRITE_BLOCK_MULTIPLE 25 /* R1 */
#define MMC_ERASE_GROUP_START    35 /* R1 */
#define MMC_ERASE_GROUP_END      36 /* R1 */
#define MMC_ERASE                38 /* R1B */
#define MMC_APP_CMD              55 /* R1 */

/* SD commands */                /* response type */
#define SD_SEND_RELATIVE_ADDR 3  /* R6 */
#define SD_SEND_SWITCH_FUNC   6  /* R1 */
#define SD_SEND_IF_COND       8  /* R7 */
#define SD_SWITCH_VOLTAGE     11 /* R1 */
#define SD_ERASE_GROUP_START  32 /* R1 */
#define SD_ERASE_GROUP_END    33 /* R1 */
#define SD_READ_OCR           58 /* R3 */
#define SD_CRC_ON_OFF         59 /* R1 */

/* SD application commands */        /* response type */
#define SD_APP_SET_BUS_WIDTH      6  /* R1 */
#define SD_APP_SD_STATUS          13 /* R2 */
#define SD_APP_SEND_NUM_WR_BLOCKS 22 /* R1 */
#define SD_APP_OP_COND            41 /* R3 */
#define SD_APP_SEND_SCR           51 /* R1 */

/* SD IO commands */
#define SD_IO_SEND_OP_COND 5  /* R4 */
#define SD_IO_RW_DIRECT    52 /* R5 */
#define SD_IO_RW_EXTENDED  53 /* R5 */

/* 48-bit response decoding (32 bits w/o CRC) */
#define MMC_R1(resp)               ((resp)[0])
#define MMC_R3(resp)               ((resp)[0])
#define MMC_R4(resp)               ((resp)[0])
#define MMC_R5(resp)               ((resp)[0])
#define SD_R6(resp)                ((resp)[0])
#define MMC_R1_CURRENT_STATE(resp) (((resp)[0] >> 9) & 0xf)

#define SDMMC_GO_IDLE_DELAY_MS         20
#define SDMMC_IO_SEND_OP_COND_DELAY_MS 10

#define SDMMC_SEND_OP_COND_MAX_RETRIES 300
#define SDMMC_SEND_OP_COND_MAX_ERRORS  3

/* OCR bits */
#define MMC_OCR_MEM_READY        (1 << 31)  /* memory power-up status bit */
#define MMC_OCR_ACCESS_MODE_MASK 0x60000000 /* bits 30:29 */
#define MMC_OCR_SECTOR_MODE      (1 << 30)
#define MMC_OCR_BYTE_MODE        (1 << 29)
#define MMC_OCR_3_5V_3_6V        (1 << 23)
#define MMC_OCR_3_4V_3_5V        (1 << 22)
#define MMC_OCR_3_3V_3_4V        (1 << 21)
#define MMC_OCR_3_2V_3_3V        (1 << 20)
#define MMC_OCR_3_1V_3_2V        (1 << 19)
#define MMC_OCR_3_0V_3_1V        (1 << 18)
#define MMC_OCR_2_9V_3_0V        (1 << 17)
#define MMC_OCR_2_8V_2_9V        (1 << 16)
#define MMC_OCR_2_7V_2_8V        (1 << 15)
#define MMC_OCR_2_6V_2_7V        (1 << 14)
#define MMC_OCR_2_5V_2_6V        (1 << 13)
#define MMC_OCR_2_4V_2_5V        (1 << 12)
#define MMC_OCR_2_3V_2_4V        (1 << 11)
#define MMC_OCR_2_2V_2_3V        (1 << 10)
#define MMC_OCR_2_1V_2_2V        (1 << 9)
#define MMC_OCR_2_0V_2_1V        (1 << 8)
#define MMC_OCR_1_65V_1_95V      (1 << 7)

#define SD_OCR_CARD_READY MMC_OCR_MEM_READY /* bit-31: power-up status */
#define SD_OCR_SDHC_CAP   (1 << 30)         /* HCS bit */
#define SD_OCR_XPC        (1 << 28)         /* SDXC Power Control (bit 28) */
#define SD_OCR_S18_RA     (1 << 24)         /* S18R/A bit: 1.8V voltage support, UHS-I only */
#define SD_OCR_VOL_MASK   0xFF8000          /* SD OCR voltage bits 23:15 */
#define SD_OCR_3_5V_3_6V  MMC_OCR_3_5V_3_6V /* bit-23 */
#define SD_OCR_3_4V_3_5V  MMC_OCR_3_4V_3_5V /* bit-22 */
#define SD_OCR_3_3V_3_4V  MMC_OCR_3_3V_3_4V /* ...    */
#define SD_OCR_3_2V_3_3V  MMC_OCR_3_2V_3_3V
#define SD_OCR_3_1V_3_2V  MMC_OCR_3_1V_3_2V
#define SD_OCR_3_0V_3_1V  MMC_OCR_3_0V_3_1V
#define SD_OCR_2_9V_3_0V  MMC_OCR_2_9V_3_0V
#define SD_OCR_2_8V_2_9V  MMC_OCR_2_8V_2_9V /* ...    */
#define SD_OCR_2_7V_2_8V  MMC_OCR_2_7V_2_8V /* bit-15 */

/* SD mode R1 response type bits */
#define MMC_R1_READY_FOR_DATA     (1 << 8) /* ready for next transfer */
#define MMC_R1_APP_CMD            (1 << 5) /* app. commands supported */
#define MMC_R1_SWITCH_ERROR       (1 << 7) /* switch command did not succeed */
#define MMC_R1_CURRENT_STATE_POS  (9)
#define MMC_R1_CURRENT_STATE_MASK (0x1E00) /* card current state */
#define MMC_R1_CURRENT_STATE_TRAN (4)
#define MMC_R1_CURRENT_STATE_STATUS(status)                                                        \
	(((status) & MMC_R1_CURRENT_STATE_MASK) >> MMC_R1_CURRENT_STATE_POS)

/* RCA argument and response */
#define MMC_ARG_RCA(rca) ((rca) << 16)
#define SD_R6_RCA(resp)  (SD_R6((resp)) >> 16)

/* Card Common Control Registers (CCCR) */
#define SD_IO_CCCR_START       0x00000
#define SD_IO_CCCR_SIZE        0x100
#define SD_IO_CCCR_FN_ENABLE   0x02
#define SD_IO_CCCR_FN_READY    0x03
#define SD_IO_CCCR_INT_ENABLE  0x04
#define SD_IO_CCCR_INT_PENDING 0x05
#define SD_IO_CCCR_CTL         0x06
#define CCCR_CTL_RES           (1 << 3)
#define SD_IO_CCCR_BUS_WIDTH   0x07
#define CCCR_BUS_WIDTH_1       (0 << 0)
#define CCCR_BUS_WIDTH_4       (2 << 0)
#define CCCR_BUS_WIDTH_8       (3 << 0)
#define CCCR_BUS_WIDTH_ECSI    (1 << 5)
#define SD_IO_CCCR_CARD_CAP    0x08
#define CCCR_CARD_CAP_LSC      BIT(6)
#define CCCR_CARD_CAP_4BLS     BIT(7)
#define SD_IO_CCCR_CISPTR      0x09
#define SD_IO_CCCR_BLKSIZEL    0x10
#define SD_IO_CCCR_BLKSIZEH    0x11
#define SD_IO_CCCR_HIGHSPEED   0x13
#define CCCR_HIGHSPEED_SUPPORT BIT(0)
#define CCCR_HIGHSPEED_ENABLE  BIT(1)

#define SDIO_FUNC_0         (0)
#define SDIO_FUNC_1         (1)
#define FUNC1_EN_MASK       (BIT(1))
#define SDIO_INIT_MAX_RETRY 10 // max number of times we try to init SDIO FN 1

#define ESP_SDIO_CONF_OFFSET (0)
#define ESP_SDIO_SEND_OFFSET (16)

/* Function Basic Registers (FBR) */
#define SD_IO_FBR_START 0x00100
#define SD_IO_FBR_SIZE  0x00700

/* Card Information Structure (CIS) */
#define SD_IO_CIS_START 0x01000
#define SD_IO_CIS_SIZE  0x17000

#define TLV_HEADER_SIZE      (14)
#define TLV_HEADER_TYPE_EP   (1)
#define TLV_HEADER_TYPE_DATA (2)
#define TLV_HEADER_EP_RESP   "RPCRsp"
#define TLV_HEADER_EP_EVENT  "RPCEvt"

#define ESP_FRAME_SIZE           (1600)
#define ESP_FRAME_HEADER_SIZE    (12)
#define ESP_FRAME_MAX_PAYLOAD    (ESP_FRAME_SIZE - ESP_FRAME_HEADER_SIZE)
#define ESP_FRAME_FLAGS_FRAGMENT (1 << 0)

typedef uint32_t sdmmc_response_t[4];

static inline uint32_t MMC_RSP_BITS(uint32_t *src, int start, int len)
{
	uint32_t mask = (len % 32 == 0) ? UINT_MAX : UINT_MAX >> (32 - (len % 32));
	size_t word = start / 32;
	size_t shift = start % 32;
	uint32_t right = src[word] >> shift;
	uint32_t left = (len + shift <= 32) ? 0 : src[word + 1] << ((32 - shift) % 32);
	return (left | right) & mask;
}

typedef struct {
	int mfg_id;   /*!< manufacturer identification number */
	int oem_id;   /*!< OEM/product identification number */
	char name[8]; /*!< product name (MMC v1 has the longest) */
	int revision; /*!< product revision */
	int serial;   /*!< product serial number */
	int date;     /*!< manufacturing date */
} sdmmc_cid_t;


typedef struct {
	uint8_t * buf;
	uint32_t buf_size;
} buf_info_t;

typedef struct {
	buf_info_t buffer[2];
	int read_index; // -1 means not in use
	uint32_t read_data_len;
	int write_index;
} double_buf_t;

typedef struct {
	union {
		void *sdio_buf_handle;
		void *wlan_buf_handle;
		void *priv_buffer_handle;
	};
	uint8_t if_type;
	uint8_t if_num;
	uint8_t *payload;
	uint8_t flag;
	uint16_t payload_len;
	uint16_t seq_num;
	void (*free_buf_handle)(void *buf_handle);
} interface_buffer_handle_t;

struct esp_priv_event {
	uint8_t		event_type;
	uint8_t		event_len;
	uint8_t		event_data[0];
}__attribute__((packed));

struct esp_payload_header {
	uint8_t          if_type:4;
	uint8_t          if_num:4;
	uint8_t          flags;
	uint16_t         len;
	uint16_t         offset;
	uint16_t         checksum;
	uint16_t		 seq_num;
	uint8_t          reserved2;
	#if ESP_PKT_NUM_DEBUG
	uint16_t         pkt_num;
	#endif
	/* Position of union field has to always be last,
	 * this is required for hci_pkt_type */
	union {
		uint8_t      reserved3;
		uint8_t      hci_pkt_type;		/* Packet type for HCI interface */
		uint8_t      priv_pkt_type;		/* Packet type for priv interface */
	};
	/* Do no add anything here */
} __attribute__((packed));

typedef enum {
	ESP_PACKET_TYPE_EVENT = 0x33,
} ESP_PRIV_PACKET_TYPE;

typedef enum {
	ESP_PRIV_EVENT_INIT = 0x22,
} ESP_PRIV_EVENT_TYPE;

typedef enum {
	HOST_CAPABILITIES=0x44,
	RCVD_ESP_FIRMWARE_CHIP_ID,
	SLV_CONFIG_TEST_RAW_TP,
	SLV_CONFIG_THROTTLE_HIGH_THRESHOLD,
	SLV_CONFIG_THROTTLE_LOW_THRESHOLD,
} SLAVE_CONFIG_PRIV_TAG_TYPE;

typedef enum {
	ESP_HOSTED_STA_IF = 0,
	ESP_HOSTED_SAP_IF,
	ESP_HOSTED_SERIAL_IF,
	ESP_HOSTED_HCI_IF,
	ESP_HOSTED_PRIV_IF,
	ESP_HOSTED_TEST_IF,
	ESP_HOSTED_MAX_IF,
} esp_hosted_interface_t;

enum wifi_iface_state {
	/** Interface is disconnected. */
	WIFI_STATE_DISCONNECTED = 0,
	/** Interface is disabled (administratively). */
	WIFI_STATE_INTERFACE_DISABLED,
	/** No enabled networks in the configuration. */
	WIFI_STATE_INACTIVE,
	/** Interface is scanning for networks. */
	WIFI_STATE_SCANNING,
	/** Authentication with a network is in progress. */
	WIFI_STATE_AUTHENTICATING,
	/** Association with a network is in progress. */
	WIFI_STATE_ASSOCIATING,
	/** Association with a network completed. */
	WIFI_STATE_ASSOCIATED,
	/** 4-way handshake with a network is in progress. */
	WIFI_STATE_4WAY_HANDSHAKE,
	/** Group Key exchange with a network is in progress. */
	WIFI_STATE_GROUP_HANDSHAKE,
	/** All authentication completed, ready to pass data. */
	WIFI_STATE_COMPLETED,

/** @cond INTERNAL_HIDDEN */
	__WIFI_STATE_AFTER_LAST,
	WIFI_STATE_MAX = __WIFI_STATE_AFTER_LAST - 1,
	WIFI_STATE_UNKNOWN
/** @endcond */
};

typedef struct {
	uint16_t seq_num;
	uint64_t last_hb_ms;
	struct net_if *iface[2];
	uint8_t mac_addr[2][6];
	k_tid_t tid;
	struct k_sem bus_sem;
	enum wifi_iface_state state[2];
} esp_hosted_data_t;

/* SD R2 response (CID) */
#define SD_CID_MID(resp) MMC_RSP_BITS((resp), 120, 8)
#define SD_CID_OID(resp) MMC_RSP_BITS((resp), 104, 16)
#define SD_CID_PNM_CPY(resp, pnm)                                                                  \
	do {                                                                                       \
		(pnm)[0] = MMC_RSP_BITS((resp), 96, 8);                                            \
		(pnm)[1] = MMC_RSP_BITS((resp), 88, 8);                                            \
		(pnm)[2] = MMC_RSP_BITS((resp), 80, 8);                                            \
		(pnm)[3] = MMC_RSP_BITS((resp), 72, 8);                                            \
		(pnm)[4] = MMC_RSP_BITS((resp), 64, 8);                                            \
		(pnm)[5] = '\0';                                                                   \
	} while (0)
#define SD_CID_REV(resp) MMC_RSP_BITS((resp), 56, 8)
#define SD_CID_PSN(resp) MMC_RSP_BITS((resp), 24, 32)
#define SD_CID_MDT(resp) MMC_RSP_BITS((resp), 8, 12)

int sdio_io_reset(const struct device *dev);
int sdio_io_rw_direct(const struct device *dev, int func, uint32_t reg, uint32_t arg, void *byte);
int sdio_send_cmd_go_idle_state(const struct device *dev);
int sdio_init_io(const struct device *dev);
void log_sdio_command(struct sdhc_command *cmd, int8_t ret);
int sdio_init_select_card(const struct device *dev, uint32_t rca);
int sdio_send_cmd_set_relative_addr(const struct device *dev, uint16_t *out_rca);
int sdio_io_init_read_card_cap(const struct device *dev, uint8_t *card_cap);
int sdio_io_read_byte(const struct device *dev, uint32_t function, uint32_t addr,
		      uint8_t *out_byte);
int sdio_io_write_byte(const struct device *dev, uint32_t function, uint32_t addr, uint8_t in_byte,
		       uint8_t *out_byte);
int hosted_sdio_set_blocksize(const struct device *card, uint8_t fn, uint16_t value);
int hosted_sdio_card_fn_init(const struct device *dev);
int sdio_generate_slave_intr(const struct device *card, uint8_t intr_no);
int sdio_io_rw_extended(const struct device *dev, uint32_t func, uint32_t reg, uint32_t arg,
			void *datap, size_t datalen);
int sdio_read_reg(const struct device *dev, uint32_t reg, void *data, uint16_t size);
int sdio_write_reg(const struct device *dev, uint32_t reg, void *data, uint16_t size);
int sdio_io_write_bytes(const struct device *dev, uint32_t function, uint32_t addr, void *src,
			size_t size);
int sdio_get_len_from_slave(uint32_t *rx_size, uint32_t reg_val);
int sdio_read_fromio(const struct device *card, uint32_t function, uint32_t addr, void *data,
		     uint16_t size);
int sdio_write_toio(const struct device *card, uint32_t function, uint32_t addr, void *data,
		    uint16_t size);
int sdio_io_read_blocks(const struct device* card, uint32_t function,
        uint32_t addr, void* dst, size_t size);
int sdio_io_write_blocks(const struct device* card, uint32_t function, uint32_t addr, const void* src, size_t size);
int hosted_sdio_read_block(const struct device *card, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);
int hosted_sdio_write_block(const struct device *card, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);
uint8_t *sdio_rx_get_buffer(uint32_t len);
void process_priv_communication(const struct device *card, interface_buffer_handle_t *buf_handle);
void process_event(const struct device *card, uint8_t *evt_buf, uint16_t len);
int is_valid_sdio_rx_packet(uint8_t *rxbuff_a, uint16_t *len_a, uint16_t *offset_a);
int process_init_event(const struct device *card, uint8_t *evt_buf, uint16_t len);
int send_slave_config(const struct device *card, uint8_t host_cap, uint8_t firmware_chip_id, uint8_t raw_tp_direction, uint8_t low_thr_thesh, uint8_t high_thr_thesh);

int esp_hosted_tx(const struct device *card, uint8_t iface_type, uint8_t iface_num,
		  uint8_t *wbuffer, uint16_t wlen);
typedef enum {
	ESP_OPEN_DATA_PATH,
	ESP_CLOSE_DATA_PATH,
	ESP_RESET,
	ESP_MAX_HOST_INTERRUPT,
} ESP_HOST_INTERRUPT;

#define ESP_SLAVE_SLCHOST_BASE 0x3FF55000
#define ESP_ADDRESS_MASK       (0x3FF)

#define HOST_TO_SLAVE_INTR    ESP_SLAVE_SCRATCH_REG_7
/* SLAVE registers */
/* Interrupt Registers */
#define ESP_SLAVE_INT_RAW_REG (ESP_SLAVE_SLCHOST_BASE + 0x50)
#define ESP_SLAVE_INT_ST_REG  (ESP_SLAVE_SLCHOST_BASE + 0x58)
#define ESP_SLAVE_INT_CLR_REG (ESP_SLAVE_SLCHOST_BASE + 0xD4)
#define ESP_HOST_INT_ENA_REG  (ESP_SLAVE_SLCHOST_BASE + 0xDC)

/* Data path registers*/
#define ESP_SLAVE_PACKET_LEN_REG (ESP_SLAVE_SLCHOST_BASE + 0x60)
#define ESP_SLAVE_TOKEN_RDATA    (ESP_SLAVE_SLCHOST_BASE + 0x44)

/* Scratch registers*/
#define ESP_SLAVE_SCRATCH_REG_0  (ESP_SLAVE_SLCHOST_BASE + 0x6C)
#define ESP_SLAVE_SCRATCH_REG_1  (ESP_SLAVE_SLCHOST_BASE + 0x70)
#define ESP_SLAVE_SCRATCH_REG_2  (ESP_SLAVE_SLCHOST_BASE + 0x74)
#define ESP_SLAVE_SCRATCH_REG_3  (ESP_SLAVE_SLCHOST_BASE + 0x78)
#define ESP_SLAVE_SCRATCH_REG_4  (ESP_SLAVE_SLCHOST_BASE + 0x7C)
#define ESP_SLAVE_SCRATCH_REG_6  (ESP_SLAVE_SLCHOST_BASE + 0x88)
#define ESP_SLAVE_SCRATCH_REG_7  (ESP_SLAVE_SLCHOST_BASE + 0x8C)
#define ESP_SLAVE_SCRATCH_REG_8  (ESP_SLAVE_SLCHOST_BASE + 0x9C)
#define ESP_SLAVE_SCRATCH_REG_9  (ESP_SLAVE_SLCHOST_BASE + 0xA0)
#define ESP_SLAVE_SCRATCH_REG_10 (ESP_SLAVE_SLCHOST_BASE + 0xA4)
#define ESP_SLAVE_SCRATCH_REG_11 (ESP_SLAVE_SLCHOST_BASE + 0xA8)
#define ESP_SLAVE_SCRATCH_REG_12 (ESP_SLAVE_SLCHOST_BASE + 0xAC)
#define ESP_SLAVE_SCRATCH_REG_13 (ESP_SLAVE_SLCHOST_BASE + 0xB0)
#define ESP_SLAVE_SCRATCH_REG_14 (ESP_SLAVE_SLCHOST_BASE + 0xB4)
#define ESP_SLAVE_SCRATCH_REG_15 (ESP_SLAVE_SLCHOST_BASE + 0xB8)

#define REG_BUF_LEN      (ESP_SLAVE_PACKET_LEN_REG - ESP_SLAVE_INT_RAW_REG + 4)
#define PACKET_LEN_INDEX (ESP_SLAVE_PACKET_LEN_REG - ESP_SLAVE_INT_RAW_REG)

#define ESP_SLAVE_CMD53_END_ADDR       0x1F800
#define ESP_SLAVE_LEN_MASK 0xFFFFF
#define ESP_BLOCK_SIZE     512
#define ESP_RX_BYTE_MAX    0x100000
#define ESP_RX_BUFFER_SIZE 1536

#define ESP_TX_BUFFER_MASK 0xFFF
#define ESP_TX_BUFFER_MAX  0x1000
#define ESP_MAX_BUF_CNT    10

#define ERROR_CHECK(x)                                                                             \
	do {                                                                                       \
		int8_t err_rc_ = (x);                                                              \
		if (unlikely(err_rc_ != 0)) {                                                      \
			LOG_INF("error %d", err_rc_);                                              \
		}                                                                                  \
	} while (0)

#define H_SDIO_TX_LEN_TO_TRANSFER(x)    ((x + 3) & (~3))
#define H_SDIO_RX_LEN_TO_TRANSFER(x)    ((x + 3) & (~3))
#define H_SDIO_TX_BLOCKS_TO_TRANSFER(x) (x / ESP_BLOCK_SIZE)
#define H_SDIO_RX_BLOCKS_TO_TRANSFER(x) (x / ESP_BLOCK_SIZE)

typedef enum {
        ESP_WLAN_SDIO_SUPPORT = (1 << 0),
        ESP_BT_UART_SUPPORT = (1 << 1), // HCI over UART
        ESP_BT_SDIO_SUPPORT = (1 << 2),
        ESP_BLE_ONLY_SUPPORT = (1 << 3),
        ESP_BR_EDR_ONLY_SUPPORT = (1 << 4),
        ESP_WLAN_SPI_SUPPORT = (1 << 5),
        ESP_BT_SPI_SUPPORT = (1 << 6),
        ESP_CHECKSUM_ENABLED = (1 << 7),
} ESP_CAPABILITIES;

typedef enum {
        // spi hd capabilities
        ESP_SPI_HD_INTERFACE_SUPPORT_2_DATA_LINES = (1 << 0),
        ESP_SPI_HD_INTERFACE_SUPPORT_4_DATA_LINES = (1 << 1),
        // leave a gap for future expansion

        // features supported
        ESP_WLAN_SUPPORT         = (1 << 4),
        ESP_BT_INTERFACE_SUPPORT = (1 << 5), // bt supported over current interface
        // leave a gap for future expansion

        // Hosted UART interface
        ESP_WLAN_UART_SUPPORT = (1 << 8),
        ESP_BT_VHCI_UART_SUPPORT = (1 << 9), // VHCI over UART
} ESP_EXTENDED_CAPABILITIES;

typedef enum {
        ESP_TEST_RAW_TP_NONE = 0,
        ESP_TEST_RAW_TP = (1 << 0),
        ESP_TEST_RAW_TP__ESP_TO_HOST = (1 << 1),
        ESP_TEST_RAW_TP__HOST_TO_ESP = (1 << 2),
        ESP_TEST_RAW_TP__BIDIRECTIONAL = (1 << 3),
} ESP_RAW_TP_MEASUREMENT;

typedef enum {
        ESP_PRIV_CAPABILITY=0x11,
        ESP_PRIV_FIRMWARE_CHIP_ID,
        ESP_PRIV_TEST_RAW_TP,
        ESP_PRIV_RX_Q_SIZE,
        ESP_PRIV_TX_Q_SIZE,
        ESP_PRIV_CAP_EXT, // extended capability (4 bytes)
        ESP_PRIV_FIRMWARE_VERSION,
} ESP_PRIV_TAG_TYPE;

typedef struct __packed {
	uint8_t if_type: 4;
	uint8_t if_num: 4;
	uint8_t flags;
	uint16_t len;
	uint16_t offset;
	uint16_t checksum;
	uint16_t seq_num;
	uint8_t reserved2;
	union {
		uint8_t hci_pkt_type;
		uint8_t priv_pkt_type;
	};
	union {
		struct __packed {
			uint8_t event_type;
			uint8_t event_len;
			uint8_t event_data[];
		};
		struct __packed {
			uint8_t ep_type;
			uint16_t ep_length;
			uint8_t ep_value[8];
			uint8_t data_type;
			uint16_t data_length;
			uint8_t data_value[];
		};
		struct {
			/* To support fragmented frames, reserve 2x payload size. */
			uint8_t payload[ESP_FRAME_MAX_PAYLOAD * 2];
		};
	};
} esp_frame_t;

#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP_NONE)

#define ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED (0xff)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32        (0x0)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32S2      (0x2)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C3      (0x5)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32S3      (0x9)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C2      (0xC)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C6      (0xD)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C5      (0x17)

#define ESP_TRANSPORT_SDIO_MAX_BUF_SIZE   1536
#define MAX_SDIO_BUFFER_SIZE              ESP_TRANSPORT_SDIO_MAX_BUF_SIZE
#define MAX_TRANSPORT_BUFFER_SIZE        MAX_SDIO_BUFFER_SIZE

#define H_ESP_PAYLOAD_HEADER_OFFSET sizeof(struct esp_payload_header)

#define MAX_PAYLOAD_SIZE (MAX_TRANSPORT_BUFFER_SIZE-H_ESP_PAYLOAD_HEADER_OFFSET)

#define H_SLAVE_TARGET_ESP32C5 1

#define ESP_HOSTED_SCAN_TIMEOUT (10000)

#define PRIO_Q_SERIAL                             0
#define PRIO_Q_BT                                 1
#define PRIO_Q_OTHERS                             2
#define MAX_PRIORITY_QUEUES                       3
#define MAC_SIZE_BYTES                            6

typedef enum {
	ESP_INVALID_IF,
	ESP_STA_IF,
	ESP_AP_IF,
	ESP_SERIAL_IF,
	ESP_HCI_IF,
	ESP_PRIV_IF,
	ESP_TEST_IF,
	ESP_ETH_IF,
	ESP_MAX_IF,
} esp_hosted_if_type_t;

static inline uint16_t esp_hosted_frame_checksum(esp_frame_t *frame)
{
	uint16_t checksum = 0;
	uint8_t *buf = (uint8_t *)frame;

	frame->checksum = 0;
	for (size_t i = 0; i < (frame->len + ESP_FRAME_HEADER_SIZE); i++) {
		checksum += buf[i];
	}

	return checksum;
}

#define ESP_FRAME_SIZE_ROUND(frame) ((ESP_FRAME_HEADER_SIZE + frame.len + 3) & ~3U)

#define CONFIG_ESP_HOSTED_TO_WIFI_DATA_THROTTLE_HIGH_THRESHOLD 80
#define CONFIG_ESP_HOSTED_TO_WIFI_DATA_THROTTLE_LOW_THRESHOLD 60

#define H_WIFI_TX_DATA_THROTTLE_LOW_THRESHOLD        CONFIG_ESP_HOSTED_TO_WIFI_DATA_THROTTLE_LOW_THRESHOLD
#define H_WIFI_TX_DATA_THROTTLE_HIGH_THRESHOLD       CONFIG_ESP_HOSTED_TO_WIFI_DATA_THROTTLE_HIGH_THRESHOLD

#endif // SDIO_IO_H

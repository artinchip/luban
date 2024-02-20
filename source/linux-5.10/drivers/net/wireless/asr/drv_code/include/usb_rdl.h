/**
 ******************************************************************************
 *
 * @file usb_rdl.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _USB_RDL_H
#define _USB_RDL_H

/* Control messages: bRequest values */
#define DL_GETVER       0	/* returns the bootrom_id_t struct */
#define DL_START        1	/* initialize dl state */
#define DL_GETSTATE     2	/* returns the rdl_state_t struct */
#define DL_GO           3	/* execute downloaded image */
#define DL_REBOOT       4	/* reboot the device in 2 seconds */
#define DL_GO_PROTECTED 5	/* execute the downloaded code and set reset
				 * event to occur in 2 seconds.  It is the
				 * responsibility of the downloaded code to
				 * clear this event
				 */
#define DL_EXEC         6	/* jump to a supplied address */
#define DL_RESETCFG     7	/* To support single enum on dongle
				 * - Not used by bootloader
				 */

/* states */
#define DL_WAITING      0	/* waiting to rx first pkt */
#define DL_READY        1	/* hdr was good, waiting for more of the
				 * compressed image
				 */
#define DL_BAD_HDR      2	/* hdr was corrupted */
#define DL_BAD_CRC      3	/* compressed image was corrupted */
#define DL_RUNNABLE     4	/* download was successful,waiting for go cmd */
#define DL_START_FAIL   5	/* failed to initialize correctly */

struct rdl_state_le {
	__le32 state;
	__le32 bytes;
};

struct bootrom_id_le {
	__le32 chip;		/* Chip id */
	__le32 chiprev;		/* Chip rev */
	__le32 remapbase;	/* Current remap base address */
	__le32 fwver;		/* Firmware version
				 * bit 0-15 for rom code version
				 * bit 16-31 for firmware version
				 */
};

#define RDL_CHUNK    1500	/* size of each dl transfer */

#define TRX_OFFSETS_DLFWLEN_IDX     0
#define TRX_OFFSETS_JUMPTO_IDX      1
#define TRX_OFFSETS_NVM_LEN_IDX     2

#define TRX_OFFSETS_DLBASE_IDX      0

#define FW_HEADER_MAGIC             0xAABBCCDD

struct sec_header_le {
	__le32 sec_len;
	__le32 sec_crc;
	__le32 chip_addr;
};

struct fw_header_le {
	__le32 magic;
	__le32 sec_num;
	struct sec_header_le sec_hdr[];
};

#endif /* _USB_RDL_H */

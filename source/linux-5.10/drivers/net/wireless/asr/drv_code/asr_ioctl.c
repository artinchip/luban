/**
 ******************************************************************************
 *
 * @file asr_ioctl.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/wireless.h>
#include <linux/types.h>

#include "asr_defs.h"
#include "ieee802_mib.h"
#include "asr_utils.h"
#include "asr_msg_tx.h"
#include "asr_ate.h"
#include "asr_sdio.h"

#ifdef CONFIG_HW_MIB_TABLE
#include "hal_machw_mib.h"
struct machw_mib_tag asr_hw_mib_table;
#endif
typedef enum {
	BYTE_T,
	INT_T,
	SSID_STRING_T,
	BYTE_ARRAY_T,
	DEF_SSID_STRING_T,
	STRING_T,
} TYPE_T;

struct iwpriv_arg {
	char name[32];		/* mib name */
	TYPE_T type;		/* Type and number of args */
	int offset;		/* mib offset */
	int len;		/* mib byte len */
	int Default;		/* mib default value */
};

#define ioctl_copy_from_user(to,from,n)        copy_from_user(to,from,n)
#define ioctl_copy_to_user(to,from,n)        copy_to_user(to,from,n)

// Macros
#define GET_MIB(asr_hw)        (asr_hw->pmib)
#define SSID            ((GET_MIB(asr_hw))->dot11StationConfigEntry.dot11DesiredSSID)
#define SSID_LEN        ((GET_MIB(asr_hw))->dot11StationConfigEntry.dot11DesiredSSIDLen)
#define SSID2SCAN        ((GET_MIB(asr_hw))->dot11StationConfigEntry.dot11SSIDtoScan)
#define SSID2SCAN_LEN    ((GET_MIB(asr_hw))->dot11StationConfigEntry.dot11SSIDtoScanLen)

#if defined(ROCK960) || defined(CONFIG_64BIT)
#define _OFFSET(field)    ((int)(uintptr_t)(long *)&(((struct wifi_mib *)0)->field))
#else
#define _OFFSET(field)    ((int)(long *)&(((struct wifi_mib *)0)->field))
#endif
#define _SIZE(field)    sizeof(((struct wifi_mib *)0)->field)

#define CTRL_LEN_CHECK(__x__,__y__) \
     do { \
         if(__x__ < __y__) { \
             ASR_DBG("!!! error [%s][%d] len=%d \n",__FUNCTION__, __LINE__, __y__); \
         } \
     } while(0)

 /****************************************************************/
 /**** add private ioctl here      *******************************/
 /****************************************************************/
#define ASR_IOCTL_SET_MIB           (SIOCDEVPRIVATE + 0x1)	// 0x89f1
#define ASR_IOCTL_GET_MIB           (SIOCDEVPRIVATE + 0x2)	// 0x89f2
#define ASR_IOCTL_WRITE_REG         (SIOCDEVPRIVATE + 0x3)	// 0x89f3
#define ASR_IOCTL_READ_REG          (SIOCDEVPRIVATE + 0x4)	// 0x89f4
#define ASR_IOCTL_SET_TXPOWER       (SIOCDEVPRIVATE + 0x5)	// 0x89f5
#define ASR_IOCTL_GET_TXPOWER       (SIOCDEVPRIVATE + 0x6)	// 0x89f6
#define ASR_IOCTL_SET_FILTER        (SIOCDEVPRIVATE + 0x7)	// 0x89f7
#define ASR_IOCTL_GET_SDIO_INFO     (SIOCDEVPRIVATE + 0x8)	// 0x89f8
#define ASR_IOCTL_TX_TRIGER         (SIOCDEVPRIVATE + 0x9)	// 0x89f9
#ifdef CONFIG_TWT
#define ASR_IOCTL_SET_TWT           (SIOCDEVPRIVATE + 0xa)	// 0x89fa
#endif
#ifdef CONFIG_HW_MIB_TABLE
#define ASR_IOCTL_RESET_HW_MIB_TABLE   (SIOCDEVPRIVATE + 0xb)	// 0x89fb
#define ASR_IOCTL_GET_HW_MIB_TABLE     (SIOCDEVPRIVATE + 0xc)	// 0x89fc
#endif
//ATE CMD
#define ASR_IOCTL_ATE_CMD             (SIOCDEVPRIVATE + 0x1)	// 0x89f1

struct iw_priv_args privtab[] = {
	{ASR_IOCTL_SET_MIB, IW_PRIV_TYPE_CHAR | 450, 0, "set_mib"},
	{ASR_IOCTL_GET_MIB, IW_PRIV_TYPE_CHAR | 40, IW_PRIV_TYPE_BYTE | 128,
	 "get_mib"},
	{ASR_IOCTL_WRITE_REG, IW_PRIV_TYPE_CHAR | 128, 0, "write_reg"},
	{ASR_IOCTL_READ_REG, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_BYTE | 128,
	 "read_reg"},
	{ASR_IOCTL_SET_TXPOWER, IW_PRIV_TYPE_CHAR | 750, 0, "set_txpower"},
	{ASR_IOCTL_GET_TXPOWER, IW_PRIV_TYPE_CHAR | 40,
	 IW_PRIV_TYPE_BYTE | 128, "get_txpower"},
	{ASR_IOCTL_SET_FILTER, IW_PRIV_TYPE_CHAR | 128, 0, "set_filter"},
	{ASR_IOCTL_GET_SDIO_INFO, IW_PRIV_TYPE_CHAR | 128, 0, "get_sdioinfo"},
	{ASR_IOCTL_TX_TRIGER, IW_PRIV_TYPE_CHAR | 128, 0, "tx_triger"},
#ifdef CONFIG_TWT
	{ASR_IOCTL_SET_TWT, IW_PRIV_TYPE_CHAR | 128, 0, "set_twt"},
#endif
#ifdef CONFIG_HW_MIB_TABLE
	{ASR_IOCTL_RESET_HW_MIB_TABLE, IW_PRIV_TYPE_CHAR | 128, 0, "rst_hw_mib_table"},
	{ASR_IOCTL_GET_HW_MIB_TABLE, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_BYTE | 128, "get_hw_mib_table"},
#endif
};

struct iw_priv_args ate_privtab[] = {
	{ASR_IOCTL_ATE_CMD, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_BYTE | 128, "at_cmd"},
};

/* MIB table */
static struct iwpriv_arg mib_table[] = {
	// struct Dot11RFEntry
	{"channel", BYTE_T, _OFFSET(dot11RFEntry.dot11channel),
	 _SIZE(dot11RFEntry.dot11channel), 1},
	{"ch_low", INT_T, _OFFSET(dot11RFEntry.dot11ch_low),
	 _SIZE(dot11RFEntry.dot11ch_low), 0},
	{"ch_hi", INT_T, _OFFSET(dot11RFEntry.dot11ch_hi),
	 _SIZE(dot11RFEntry.dot11ch_hi), 0},
	// struct Dot11StationConfigEntry
	{"ssid", SSID_STRING_T,
	 _OFFSET(dot11StationConfigEntry.dot11DesiredSSID),
	 _SIZE(dot11StationConfigEntry.dot11DesiredSSID), 0},
	{"defssid", DEF_SSID_STRING_T,
	 _OFFSET(dot11StationConfigEntry.dot11DefaultSSID),
	 _SIZE(dot11StationConfigEntry.dot11DefaultSSID), 0},
	{"bssid2join", BYTE_ARRAY_T,
	 _OFFSET(dot11StationConfigEntry.dot11DesiredBssid),
	 _SIZE(dot11StationConfigEntry.dot11DesiredBssid), 0},
	{"autorate", INT_T, _OFFSET(dot11StationConfigEntry.autoRate),
	 _SIZE(dot11StationConfigEntry.autoRate), 1},
	{"fixrate", INT_T, _OFFSET(dot11StationConfigEntry.fixedTxRate),
	 _SIZE(dot11StationConfigEntry.fixedTxRate), 0},
	// struct Dot1180211AuthEntry
	{"authtype", INT_T, _OFFSET(dot1180211AuthEntry.dot11AuthAlgrthm),
	 _SIZE(dot1180211AuthEntry.dot11AuthAlgrthm), 0},
	{"encmode", BYTE_T, _OFFSET(dot1180211AuthEntry.dot11PrivacyAlgrthm),
	 _SIZE(dot1180211AuthEntry.dot11PrivacyAlgrthm), 0},
	{"wepdkeyid", INT_T, _OFFSET(dot1180211AuthEntry.dot11PrivacyKeyIndex),
	 _SIZE(dot1180211AuthEntry.dot11PrivacyKeyIndex), 0},
	{"psk_enable", INT_T, _OFFSET(dot1180211AuthEntry.dot11EnablePSK),
	 _SIZE(dot1180211AuthEntry.dot11EnablePSK), 0},
	{"wpa_cipher", INT_T, _OFFSET(dot1180211AuthEntry.dot11WPACipher),
	 _SIZE(dot1180211AuthEntry.dot11WPACipher), 0},
	{"wpa2_cipher", INT_T, _OFFSET(dot1180211AuthEntry.dot11WPA2Cipher),
	 _SIZE(dot1180211AuthEntry.dot11WPA2Cipher), 0},
	{"passphrase", STRING_T, _OFFSET(dot1180211AuthEntry.dot11PassPhrase),
	 _SIZE(dot1180211AuthEntry.dot11PassPhrase), 0},

	// struct Dot118021xAuthEntry
	{"802_1x", INT_T, _OFFSET(dot118021xAuthEntry.dot118021xAlgrthm),
	 _SIZE(dot118021xAuthEntry.dot118021xAlgrthm), 0},
	// struct Dot11DefaultKeysTable
	{"wepkey1", BYTE_ARRAY_T, _OFFSET(dot11DefaultKeysTable.keytype[0]),
	 _SIZE(dot11DefaultKeysTable.keytype[0]), 0},
	{"wepkey2", BYTE_ARRAY_T, _OFFSET(dot11DefaultKeysTable.keytype[1]),
	 _SIZE(dot11DefaultKeysTable.keytype[1]), 0},
	{"wepkey3", BYTE_ARRAY_T, _OFFSET(dot11DefaultKeysTable.keytype[2]),
	 _SIZE(dot11DefaultKeysTable.keytype[2]), 0},
	{"wepkey4", BYTE_ARRAY_T, _OFFSET(dot11DefaultKeysTable.keytype[3]),
	 _SIZE(dot11DefaultKeysTable.keytype[3]), 0},
	// struct Dot11OperationEntry
	{"opmode", INT_T, _OFFSET(dot11OperationEntry.opmode),
	 _SIZE(dot11OperationEntry.opmode), 0x10},
	{"hiddenAP", INT_T, _OFFSET(dot11OperationEntry.hiddenAP),
	 _SIZE(dot11OperationEntry.hiddenAP), 0},
	{"rtsthres", INT_T, _OFFSET(dot11OperationEntry.dot11RTSThreshold),
	 _SIZE(dot11OperationEntry.dot11RTSThreshold), 2347},
	// struct bss_type
	{"band", BYTE_T, _OFFSET(dot11BssType.net_work_type),
	 _SIZE(dot11BssType.net_work_type), 3},
	// struct erp_mib
	{"cts2self", INT_T, _OFFSET(dot11ErpInfo.ctsToSelf),
	 _SIZE(dot11ErpInfo.ctsToSelf), 0},
	//struct Dot11QosEntry
	{"qos_enable", INT_T, _OFFSET(dot11QosEntry.dot11QosEnable),
	 _SIZE(dot11QosEntry.dot11QosEnable), 1},
	// struct Dot11nConfigEntry
	{"supportedmcs", INT_T, _OFFSET(dot11nConfigEntry.dot11nSupportedMCS),
	 _SIZE(dot11nConfigEntry.dot11nSupportedMCS), 0xffff},
	{"basicmcs", INT_T, _OFFSET(dot11nConfigEntry.dot11nBasicMCS),
	 _SIZE(dot11nConfigEntry.dot11nBasicMCS), 0},
	{"use40M", INT_T, _OFFSET(dot11nConfigEntry.dot11nUse40M),
	 _SIZE(dot11nConfigEntry.dot11nUse40M), 1},
	{"2ndchoffset", INT_T, _OFFSET(dot11nConfigEntry.dot11n2ndChOffset),
	 _SIZE(dot11nConfigEntry.dot11n2ndChOffset), 1},
	{"shortGI20M", INT_T, _OFFSET(dot11nConfigEntry.dot11nShortGIfor20M),
	 _SIZE(dot11nConfigEntry.dot11nShortGIfor20M), 0},
	{"shortGI40M", INT_T, _OFFSET(dot11nConfigEntry.dot11nShortGIfor40M),
	 _SIZE(dot11nConfigEntry.dot11nShortGIfor40M), 0},
	{"stbc", INT_T, _OFFSET(dot11nConfigEntry.dot11nSTBC),
	 _SIZE(dot11nConfigEntry.dot11nSTBC), 1},
	{"ldpc", INT_T, _OFFSET(dot11nConfigEntry.dot11nLDPC),
	 _SIZE(dot11nConfigEntry.dot11nLDPC), 1},
	{"ampdu", INT_T, _OFFSET(dot11nConfigEntry.dot11nAMPDU),
	 _SIZE(dot11nConfigEntry.dot11nAMPDU), 0},
};

static struct iwpriv_arg *get_tbl_entry(char *pstr)
{
	int i = 0;
	int arg_num = sizeof(mib_table) / sizeof(struct iwpriv_arg);
	volatile char name[128];

	while (*pstr && *pstr != '=') {
		if (i >= sizeof(name) - 1)
			return NULL;
		name[i++] = *pstr++;
	}
	name[i] = '\0';

	for (i = 0; i < arg_num; i++) {
		if (!strcmp((char *)name, mib_table[i].name)) {
			return &mib_table[i];
		}
	}
	return NULL;
}

int _atoi(char *s, int base)
{
	int k = 0;
	int sign = 1;

	k = 0;
	if (base == 10) {
		if (*s == '-') {
			sign = -1;
			s++;
		}
		while (*s != '\0' && *s >= '0' && *s <= '9') {
			k = 10 * k + (*s - '0');
			s++;
		}
		k *= sign;
	} else {
		while (*s != '\0') {
			int v;
			if (*s >= '0' && *s <= '9')
				v = *s - '0';
			else if (*s >= 'a' && *s <= 'f')
				v = *s - 'a' + 10;
			else if (*s >= 'A' && *s <= 'F')
				v = *s - 'A' + 10;
			else {
				printk("error hex format!\n");
				return k;
			}
			k = 16 * k + v;
			s++;
		}
	}
	return k;
}

int get_array_val(unsigned char *dst, char *src, int len)
{
	char tmpbuf[4];
	int num = 0;

	while (len > 0) {
		memcpy(tmpbuf, src, 2);
		tmpbuf[2] = '\0';
		*dst++ = (unsigned char)_atoi(tmpbuf, 16);
		len -= 2;
		src += 2;
		num++;
	}
	return num;
}

void set_mib_default_tbl(struct asr_hw *asr_hw)
{
	int i;
	int arg_num = sizeof(mib_table) / sizeof(struct iwpriv_arg);

	for (i = 0; i < arg_num; i++) {
		if (mib_table[i].Default) {
			if (mib_table[i].type == BYTE_T)
				*(((unsigned char *)asr_hw->pmib) +
				  mib_table[i].offset) = (unsigned char)mib_table[i].Default;
			else if (mib_table[i].type == INT_T)
				memcpy(((unsigned char *)asr_hw->pmib) +
				       mib_table[i].offset, (unsigned char *)&mib_table[i].Default, sizeof(int));
			else {
				// We only give default value of types of BYTE_T and INT_T here.
				// Some others are gave in set_mib_default().
			}
		}
	}
}

void set_mib_default(struct asr_hw *asr_hw)
{
	asr_hw->pmib->mib_version = MIB_VERSION;
	set_mib_default_tbl(asr_hw);

#ifdef CONFIG_ASR5531
	// others that are not types of byte and int
	strncpy((char *)asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID, "ASR5531-default", 32);
	asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen("ASR5531-default");

#elif defined(CONFIG_ASR5505)
	// others that are not types of byte and int
	strncpy((char *)asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID, "ASR5505-default", 32);
	asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen("ASR5505-default");
#elif defined(CONFIG_ASR5825)
	// others that are not types of byte and int
	strncpy((char *)asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID, "ASR5825-default", 32);
	asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen("ASR5825-default");
#elif defined(CONFIG_ASR595X)
	// others that are not types of byte and int
	strncpy((char *)asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID, "ASR595X-default", 32);
	asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen("ASR595X-default");
#endif
}

int set_mib(struct asr_hw *asr_hw, unsigned char *data)
{
	struct iwpriv_arg *entry;
	int int_val, len;
	int is_hex_type = 0;
	unsigned char byte_val;
	char *arg_val;

	ASR_DBG(ASR_FN_ENTRY_STR);;

	asr_dbg(INFO, "set_mib %s\n", data);

	entry = get_tbl_entry((char *)data);
	if (entry == NULL) {
		asr_dbg(INFO, "invalid mib name [%s] !\n", data);
		return -1;
	}
	// search value
	arg_val = (char *)data;
	while (*arg_val && *arg_val != '=') {
		arg_val++;
	}

	if (!*arg_val) {
		asr_dbg(INFO, "mib value empty [%s] !\n", data);
		return -1;
	}
	//printk("[%s %d] %c \n",__FUNCTION__,__LINE__ , *arg_val);
	arg_val++;

	// skip space
	//while (*arg_val && *arg_val == 0x7f)
	//      arg_val++;

	switch (entry->type) {
	case BYTE_T:
		byte_val = (unsigned char)_atoi(arg_val, 10);
		memcpy(((unsigned char *)asr_hw->pmib) + entry->offset, &byte_val, 1);
		break;

	case INT_T:
		if (*arg_val == '0' && (*(arg_val + 1) == 'x' || *(arg_val + 1) == 'X')) {
			is_hex_type = 1;
			arg_val += 2;
			printk("[%s %d]hex format\n", __FUNCTION__, __LINE__);
		}

		if (is_hex_type)
			int_val = _atoi(arg_val, 16);
		else
			int_val = _atoi(arg_val, 10);

		memcpy(((unsigned char *)asr_hw->pmib) + entry->offset, (unsigned char *)&int_val, sizeof(int));

		break;

	case SSID_STRING_T:
		if (strlen(arg_val) > entry->len)
			arg_val[entry->len] = '\0';
		memset(asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID,
		       0, sizeof(asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID));
		memcpy(asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID, arg_val, strlen(arg_val));
		asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen(arg_val);
		if ((SSID_LEN == 3) && ((SSID[0] == 'A') || (SSID[0] == 'a'))
		    && ((SSID[1] == 'N') || (SSID[1] == 'n'))
		    && ((SSID[2] == 'Y') || (SSID[2] == 'y'))) {
			SSID2SCAN_LEN = 0;
			memset(SSID2SCAN, 0, 32);
		} else {
			SSID2SCAN_LEN = SSID_LEN;
			memcpy(SSID2SCAN, SSID, SSID_LEN);
		}
		break;

	case BYTE_ARRAY_T:
		len = strlen(arg_val);
		if (len / 2 > entry->len) {
			asr_dbg(INFO, "invalid len of BYTE_ARRAY_T mib [%s] !\n", entry->name);
			return -1;
		}
		if (len % 2) {
			asr_dbg(INFO, "invalid len of BYTE_ARRAY_T mib [%s] !\n", entry->name);
			return -1;
		}
		get_array_val(((unsigned char *)asr_hw->pmib) + entry->offset, arg_val, strlen(arg_val));
		break;

	case DEF_SSID_STRING_T:
		if (strlen(arg_val) > entry->len)
			arg_val[entry->len] = '\0';
		memset(asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSID,
		       0, sizeof(asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSID));
		memcpy(asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSID, arg_val, strlen(arg_val));
		asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSIDLen = strlen(arg_val);
		break;

	case STRING_T:
		if (strlen(arg_val) >= entry->len)
			arg_val[entry->len - 1] = '\0';
		strcpy((char *)(((unsigned char *)asr_hw->pmib) + entry->offset), arg_val);
		break;

	default:
		ASR_DBG("invalid mib type!\n");
		break;
	}

	return 0;
}

int get_mib(struct asr_hw *asr_hw, unsigned char *data)
{
	struct iwpriv_arg *entry;
	int i, copy_len;
	char tmpbuf[40];

	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_dbg(INFO, "get_mib %s\n", data);

	entry = get_tbl_entry((char *)data);
	if (entry == NULL) {
		ASR_DBG("invalid mib name [%s] !\n", data);
		return -1;
	}
	asr_dbg(INFO, "get_mib %s [%d %d %d]\n", entry->name, entry->len, entry->offset, entry->type);
	copy_len = entry->len;

	switch (entry->type) {
	case BYTE_T:
		memcpy(data, ((unsigned char *)asr_hw->pmib) + entry->offset, 1);
		asr_dbg(INFO, "byte data: %d\n", *data);
		break;

	case INT_T:
		memcpy(data, ((unsigned char *)asr_hw->pmib) + entry->offset, sizeof(int));
		asr_dbg(INFO, "int data: %d\n", *((int *)data));
		break;

	case SSID_STRING_T:
		memcpy(tmpbuf,
		       asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSID,
		       asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen);
		tmpbuf[asr_hw->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen] = '\0';
		strcpy((char *)data, tmpbuf);
		asr_dbg(INFO, "ssid: %s\n", tmpbuf);
		break;

	case BYTE_ARRAY_T:
		memcpy(data, ((unsigned char *)asr_hw->pmib) + entry->offset, entry->len);
		asr_dbg(INFO, "data (hex): ");
		for (i = 0; i < entry->len; i++)
			ASR_DBG("%02x", *((unsigned char *)((unsigned char *)
							    asr_hw->pmib) + entry->offset + i));
		asr_dbg(INFO, "\n");
		break;

	case DEF_SSID_STRING_T:
		memcpy(tmpbuf,
		       asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSID,
		       asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSIDLen);
		tmpbuf[asr_hw->pmib->dot11StationConfigEntry.dot11DefaultSSIDLen] = '\0';
		strcpy((char *)data, tmpbuf);
		asr_dbg(INFO, "defssid: %s\n", tmpbuf);
		break;

	case STRING_T:
		strcpy((char *)data, (char *)(((unsigned char *)asr_hw->pmib) + entry->offset));
		asr_dbg(INFO, "string data: %s\n", data);
		break;

	default:
		asr_dbg(INFO, "invalid mib type!\n");
		return 0;
	}

	return copy_len;
}

/*
 * Write register, command: "iwpriv wlanX write_reg addr,value"
 *     where: addr and value should be input in hex
 */
int write_reg(struct asr_hw *asr_hw, unsigned char *data)
{
	volatile char name[100];
	int i = 0;
	unsigned int val;
	unsigned int addr;
	int ret = -1;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// get addr and value
	i = 0;
	while (*data && *data != ',' && i < sizeof(name) - 1)
		name[i++] = *data++;

	name[i] = '\0';

	if (!*data++) {
		ASR_DBG("invalid value!\n");
		return -1;
	}
	addr = (unsigned int)_atoi((char *)name, 16);
	val = (unsigned int)_atoi((char *)data, 16);

	asr_dbg(INFO, "write reg  addr=%08x, val=0x%x\n", addr, val);

	ret = asr_send_dbg_mem_write_req(asr_hw, addr, val);

	return ret;
}

/*
 * Read register, command: "iwpriv wlanX read_reg addr"
 *     where: addr should be input in hex
 */
int read_reg(struct asr_hw *asr_hw, unsigned char *data)
{
	//int i=0;
	unsigned int mem_addr;
	int ret = -1;
	struct dbg_mem_read_cfm cfm = { 0 };
	unsigned char *org_ptr = data;
	unsigned int dw_val;
	int len = 4;

	ASR_DBG(ASR_FN_ENTRY_STR);;

	// get addr
	mem_addr = (unsigned int)_atoi((char *)data, 16);
	asr_dbg(INFO, "read reg  addr=%08x\n", mem_addr);

	ret = asr_send_dbg_mem_read_req(asr_hw, mem_addr, &cfm);
	if (ret < 0) {
		dev_err(asr_hw->dev, "Reading memory: [0x%08x] fail\n", cfm.memaddr);
		return ret;
	} else {
		dw_val = cfm.memdata;
		dev_info(asr_hw->dev, "Reading memory: [0x%08x] = 0x%08x / %d\n", cfm.memaddr, cfm.memdata,
			 cfm.memdata);

#if defined(__LITTLE_ENDIAN_BITFIELD)
		// To fit PSD tool endian requirement
		dw_val = ___constant_swab32(dw_val);
#endif
		memcpy(org_ptr, (char *)&dw_val, len);
	}

	return len;
}

int asr_set_tx_power(struct asr_hw *asr_hw, u8 * tmpbuf)
{
	// add set api here
	ASR_DBG(ASR_FN_ENTRY_STR);

	return 0;
}

int asr_get_txpwr(struct asr_hw *asr_hw, u8 * tmpbuf, int sizeof_tmpbuf)
{
	// add get api here
	ASR_DBG(ASR_FN_ENTRY_STR);

	return 0;
}

/*
 * set_filter, command: "iwpriv wlanX set_filter filter"
 *     where: filter should be input in hex
 */
int set_filter(struct asr_hw *asr_hw, unsigned char *data)
{
	unsigned int filter;
	int ret = -1;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// get filter
	filter = (unsigned int)_atoi((char *)data, 16);
	asr_dbg(INFO, "set_filter %08x\n", filter);

	ret = asr_send_set_filter(asr_hw, filter);
	if (ret < 0)
		dev_err(asr_hw->dev, "set_filter fail\n");

	return ret;
}

#ifdef CONFIG_TWT
// twt config
bool g_twt_on;
wifi_twt_config_t g_wifi_twt_param;
static int wifi_twt_config(int argc, char **argv, struct asr_hw *asr_hw)
{
	int setup_cmd = 1;	// suggest
	int flow_type = 0;	// announced
	int wake_interval_exponent = 0;
	int wake_interval_mantissa = 0;
	int monimal_min_wake_duration = 0;
	wifi_twt_config_t wifi_twt_param = { 0 };
	bool twt_upload_wait_trigger = false;

	switch (argc) {
	case 1:
	case 2:
	case 3:
		ASR_DBG("param num error,at leaset 3\r\n");
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		//    add [interval_exp] [interval_mantissa] [min_wake_dur] [setup_cmd] [flow_type] [twt_upload_wait_trigger]
		if (argc > 3) {
			wake_interval_exponent = _atoi(argv[1], 10);
			wake_interval_mantissa = _atoi(argv[2], 10);
			monimal_min_wake_duration = _atoi(argv[3], 10);
		}

		if (argc > 4)
			setup_cmd = _atoi(argv[4], 10);

		if (argc > 5)
			flow_type = _atoi(argv[5], 10);

		if (argc > 6)
			twt_upload_wait_trigger = (_atoi(argv[6], 10) > 0);

		if ((setup_cmd >= 0) && (setup_cmd <= 2) &&
		    (flow_type >= 0) && (flow_type <= 1) && (wake_interval_exponent >= 0)
		    && (wake_interval_mantissa >= 0)
		    && (monimal_min_wake_duration >= 0)) {
			wifi_twt_param.setup_cmd = setup_cmd;
			wifi_twt_param.flow_type = flow_type;
			wifi_twt_param.wake_interval_exponent = wake_interval_exponent;
			wifi_twt_param.wake_interval_mantissa = wake_interval_mantissa;
			wifi_twt_param.monimal_min_wake_duration = monimal_min_wake_duration;
			wifi_twt_param.twt_upload_wait_trigger = twt_upload_wait_trigger;

            // save twt info for user check.
            g_twt_on = true;
            memcpy(&g_wifi_twt_param,&wifi_twt_param,sizeof(wifi_twt_config_t));

			asr_send_itwt_config(asr_hw, &wifi_twt_param);
			return 0;
		} else {
			ASR_DBG("param not valid\n");
			break;
		}
	default:
		ASR_DBG("param num error\r\n");
		break;

	}

	return -1;
}

static int wifi_twt_del(int argc, char **argv, struct asr_hw *asr_hw)
{
	int flow_id = 0;

	//del [flow_id]
	if (argc > 1) {
		flow_id = _atoi(argv[1], 10);
		if ((flow_id < 7) && (flow_id >= 0)) {
			asr_send_itwt_del(asr_hw, flow_id);

            // clear twt info for user check.
            g_twt_on = false;
            memset(&g_wifi_twt_param,0,sizeof(wifi_twt_config_t));

			return 0;
		} else
			return -1;
	} else
		return -1;
}

#define ARGCMAXLEN  10
static int parse_cmd(char *buf, char **argv)
{
	int argc = 0;
	//buf point parse;seperate by ','
	while ((argc < ARGCMAXLEN) && (*buf != '\0')) {
		argv[argc] = buf;
		argc++;
		buf++;
		//space and NULL character
		while ((*buf != ',') && (*buf != '\0'))
			buf++;
		//one or more space characters
		while (*buf == ',') {
			*buf = '\0';
			buf++;
		}
	}
	return argc;
}

/*
 * Write register, command: "iwpriv wlanX set_twt add,[interval_exp],[interval_mantissa],[min_wake_dur],[setup_cmd],[flow_type]"
                             iwpriv wlanX set_twt del,[flow_id]
 *
*/
int set_twt(struct asr_hw *asr_hw, unsigned char *data)
{
	int ret = -1;
	char *argv[ARGCMAXLEN];
	int argc = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	pr_info("[%s] data is (%s)", __func__, data);

	if ((argc = parse_cmd((char *)data, argv)) > 0) {
		if (strcmp(argv[0], "add") == 0) {
			ret = wifi_twt_config(argc, argv, asr_hw);
		} else if (strcmp(argv[0], "del") == 0)
			ret = wifi_twt_del(argc, argv, asr_hw);
		else {
			ASR_DBG("invalid value!\n");
			return -1;
		}
	}

	if (ret < 0)
		dev_err(asr_hw->dev, "set_twt fail\n");

	return ret;
}
#endif
#ifdef CONFIG_MIB_TABLE
int reset_hw_mib_table(struct asr_hw *asr_hw)
{
	int ret = -1;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// reset hw mib table without parameters
	asr_dbg(INFO, "reset hw mib table");

	ret = asr_send_reset_hw_mib_table_req(asr_hw);

	return ret;
}
int get_hw_mib_table(struct asr_hw *asr_hw, struct machw_mib_tag *hw_mib_table)
{
	int ret = -1;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// reset hw mib table without parameters
	asr_dbg(INFO, "get hw mib table");

	ret = asr_send_get_hw_mib_table_req(asr_hw, hw_mib_table);

	printf("nx_rx_phy_error_count:%d\r\n", hw_mib_table->nx_rx_phy_error_count);

	return ret;
}
#endif
int asr_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{

	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	//unsigned long flags;
	struct iwreq *wrq = (struct iwreq *)ifr;
	int i = 0, ret = -1, sizeof_tmpbuf;
	//u8 tmpbuf1[2048];
	u8 *tmpbuf;
#ifdef CONFIG_ASR_SDIO
	u16 rd_bitmap = 0, wr_bitmap = 0;
	u16 rd_bitmap_last = 0, wr_bitmap_last = 0;
	u8 sdio_regs[SDIO_REG_READ_LENGTH] = { 0 };
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);;

	sizeof_tmpbuf = 2048;
	tmpbuf = kmalloc(sizeof(u8) * 2048, GFP_KERNEL);
	if (!tmpbuf)
		return -ENOMEM;
	memset(tmpbuf, '\0', sizeof_tmpbuf);

	if (asr_hw->driver_mode == DRIVER_MODE_NORMAL) {

		switch (cmd) {
		case ASR_IOCTL_SET_MIB:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = set_mib(asr_hw, tmpbuf);
			break;

		case ASR_IOCTL_GET_MIB:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			i = get_mib(asr_hw, tmpbuf);
			if (i >= 0) {
				if ((i > 0)
				    && ioctl_copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i))
					break;
				wrq->u.data.length = i;
				ret = 0;
			}
			break;

		case SIOCGIWPRIV:	//-- get private ioctls for iwpriv --//
			if (wrq->u.data.pointer) {
				ret =
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 1)
				    access_ok(
#else
				    access_ok(VERIFY_WRITE,
#endif
					      (const void *)wrq->u.data.pointer, sizeof(privtab));

				if (!ret) {
					ret = -EFAULT;
					printk("user space valid check error!\n");
					break;
				}
				if ((sizeof(privtab) / sizeof(privtab[0])) <= wrq->u.data.length) {
					wrq->u.data.length = sizeof(privtab) / sizeof(privtab[0]);
					if (ioctl_copy_to_user((void *)wrq->u.data.pointer, privtab, sizeof(privtab)))
						ret = -EFAULT;
				} else {
					ret = -E2BIG;
				}
			}
			break;

		case ASR_IOCTL_WRITE_REG:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = write_reg(asr_hw, tmpbuf);
			break;

		case ASR_IOCTL_READ_REG:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			i = read_reg(asr_hw, tmpbuf);
			if (i >= 0) {
				if ((i > 0)
				    && ioctl_copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i))
					break;
				wrq->u.data.length = i;
				ret = 0;
			}
			break;

		case ASR_IOCTL_SET_TXPOWER:
			CTRL_LEN_CHECK(sizeof_tmpbuf, wrq->u.data.length);
			if (ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = asr_set_tx_power(asr_hw, tmpbuf);
			break;

		case ASR_IOCTL_GET_TXPOWER:
			CTRL_LEN_CHECK(sizeof_tmpbuf, wrq->u.data.length);
			if (ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;

			//spin_lock_irqsave(&asr_hw->tx_agg_env_lock, flags);
			i = asr_get_txpwr(asr_hw, tmpbuf, sizeof_tmpbuf);
			//spin_unlock_irqrestore(&asr_hw->tx_agg_env_lock, flags);

			if (i > 0) {
				if (ioctl_copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i))
					break;
				wrq->u.data.length = i;
				ret = 0;
			}
			break;
		case ASR_IOCTL_SET_FILTER:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = set_filter(asr_hw, tmpbuf);
			break;
		case ASR_IOCTL_GET_SDIO_INFO:
#ifdef CONFIG_ASR_SDIO
			sdio_claim_host(asr_hw->plat->func);
			ret = sdio_readsb(asr_hw->plat->func, sdio_regs, 0x4, SDIO_REG_READ_LENGTH);
			sdio_release_host(asr_hw->plat->func);
			if (ret) {
				dev_err(asr_hw->dev, "%s isr read sdio_reg fail!!! \n", __func__);
			}

			rd_bitmap = asr_hw->sdio_reg[RD_BITMAP_L];
			rd_bitmap |= asr_hw->sdio_reg[RD_BITMAP_U] << 8;
			wr_bitmap = asr_hw->sdio_reg[WR_BITMAP_L];
			wr_bitmap |= asr_hw->sdio_reg[WR_BITMAP_L] << 8;

			rd_bitmap_last = asr_hw->last_sdio_regs[RD_BITMAP_L];
			rd_bitmap_last |= asr_hw->last_sdio_regs[RD_BITMAP_U] << 8;
			wr_bitmap_last = asr_hw->last_sdio_regs[WR_BITMAP_L];
			wr_bitmap_last |= asr_hw->last_sdio_regs[WR_BITMAP_L] << 8;

			dev_info(asr_hw->dev,
				 "sdiodebug:(0x%x,0x%x)(0x%x,0x%x,0x%x)(0x%x,0x%x,0x%x)(%d,%d,%u,0x%x,0x%x)(%d)!\n",
				 sdio_regs[0] | sdio_regs[1] << 8, sdio_regs[2] | sdio_regs[3] << 8,
				 asr_hw->sdio_reg[HOST_INT_STATUS], rd_bitmap, wr_bitmap,
				 asr_hw->last_sdio_regs[HOST_INT_STATUS], rd_bitmap_last, wr_bitmap_last,
				 asr_hw->rx_data_cur_idx, asr_hw->tx_data_cur_idx, asr_hw->tx_agg_env.aggr_buf_cnt,
				 asr_hw->sdio_ireg, asr_hw->tx_use_bitmap, netif_queue_stopped(asr_vif->ndev));
#endif
			break;
		case ASR_IOCTL_TX_TRIGER:
#ifdef CONFIG_ASR_SDIO
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			asr_hw->restart_flag = true;
			dev_info(asr_hw->dev, "%s:aggr_buf_cnt=%u,last_tx=%lu.\n", __func__,
				 asr_hw->tx_agg_env.aggr_buf_cnt, asr_hw->stats.last_tx);

			set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
			wake_up_interruptible(&asr_hw->waitq_main_task_thead);
			ret = 0;
#endif
			break;
#ifdef CONFIG_TWT
		case ASR_IOCTL_SET_TWT:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = set_twt(asr_hw, tmpbuf);
			break;
#endif
#ifdef CONFIG_HW_MIB_TABLE
		case ASR_IOCTL_RESET_HW_MIB_TABLE:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			ret = reset_hw_mib_table(asr_hw);
			break;
		case ASR_IOCTL_GET_HW_MIB_TABLE:
			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
				break;
			i = get_hw_mib_table(asr_hw, &asr_hw_mib_table);
			if (i >= 0) {
				if ((i > 0)
				    && ioctl_copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i))
					break;
				wrq->u.data.length = i;
				ret = 0;
			}
			break;
#endif
		default:
			dev_err(asr_hw->dev, " %s : unknown cmd 0x%x \n", __func__, cmd);
			break;
		}

	} else if (asr_hw->driver_mode == DRIVER_MODE_ATE) {

		if (cmd == SIOCGIWPRIV) {	//-- get private ioctls for iwpriv --//
			if (wrq->u.data.pointer) {

				ret =
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 1)
				    access_ok(
#else
				    access_ok(VERIFY_WRITE,
#endif
					      (const void *)wrq->u.data.pointer, sizeof(ate_privtab));

				if (!ret) {
					ret = -EFAULT;
					printk("user space valid check error!\n");
					kfree(tmpbuf);
					return ret;
				}
				if ((sizeof(ate_privtab) / sizeof(ate_privtab[0])) <= wrq->u.data.length) {
					wrq->u.data.length = sizeof(ate_privtab) / sizeof(ate_privtab[0]);
					if (ioctl_copy_to_user
					    ((void *)wrq->u.data.pointer, ate_privtab, sizeof(ate_privtab)))
						ret = -EFAULT;
				} else {
					ret = -E2BIG;
				}

			}
		} else {

			if ((wrq->u.data.length >= sizeof_tmpbuf) ||
			    ioctl_copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length)) {
				kfree(tmpbuf);
				return ret;
			}

			i = -1;

			switch (cmd) {
			case ASR_IOCTL_ATE_CMD:
				i = ate_data_direct_tx_rx(asr_hw, tmpbuf);
				break;

			default:
				dev_err(asr_hw->dev, " %s : unknown ate cmd 0x%x \n", __func__, cmd);
				break;
			}

			if (i >= 0) {
				if ((i > 0)
				    && ioctl_copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i)) {
					kfree(tmpbuf);
					return ret;
				}

				wrq->u.data.length = i;
				ret = 0;
			}
		}
	}

	kfree(tmpbuf);
	return ret;
}

/**
 ******************************************************************************
 *
 * @file ieee802_mib.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _IEEE802_MIB_H_
#define _IEEE802_MIB_H_

#define MIB_VERSION                24
#define MAX_2G_CHANNEL_NUM        14
#define MAX_5G_CHANNEL_NUM        196
#define MACADDRLEN                6

#define SDIOTYPELEN             5
//-------------------------------------------------------------
// Support add or remove ACL list at run time
//-------------------------------------------------------------
#define D_ACL

#ifdef D_ACL
#define NUM_ACL                    128
#else
#define NUM_ACL                    32
#endif

#if !defined(_LITTLE_ENDIAN_) && !defined(_BIG_ENDIAN_)
#define _BIG_ENDIAN_
#endif

#define MESH_ID_LEN                32

struct Dot11StationConfigEntry {
	unsigned char dot11Bssid[MACADDRLEN];
	unsigned char dot11DesiredSSID[32];
	unsigned int dot11DesiredSSIDLen;
	unsigned char dot11DefaultSSID[32];
	unsigned int dot11DefaultSSIDLen;
	unsigned char dot11SSIDtoScan[32];
	unsigned int dot11SSIDtoScanLen;
	unsigned char dot11DesiredBssid[6];
	unsigned char dot11OperationalRateSet[32];
	unsigned int dot11OperationalRateSetLen;

	unsigned int dot11BeaconPeriod;
	unsigned int dot11DTIMPeriod;
	unsigned int dot11swcrypto;
	unsigned int dot11AclMode;	// 1: positive check 2: negative check
	unsigned char dot11AclAddr[NUM_ACL][MACADDRLEN];
	unsigned int dot11AclNum;	// acl entry number, this field should be followed to dot11AclAddr
	unsigned int dot11SupportedRates;	// bit mask value. bit0-bit11 as 1,2,5.5,11,6,9,12,18,24,36,48,54
	unsigned int dot11BasicRates;	// bit mask value. bit0-bit11 as 1,2,5.5,11,6,9,12,18,24,36,48,54
	unsigned int dot11RegDomain;	// reguration domain
	int txpwr_lmt_index;	// TX Power Limit Index
	unsigned int autoRate;	// enable/disable auto rate
	unsigned int fixedTxRate;	// fix tx rate
	int swTkipMic;
	int protectionDisabled;	// force disable protection
	int olbcDetectDisabled;	// david, force disable olbc dection
	int nmlscDetectDisabled;	// hf, force disable no member legacy station condition detection
	int legacySTADeny;	// deny association from legacy (11B) STA

	//unsigned int    w52_passive_scan;
	int fastRoaming;	// 1: enable fast-roaming, 0: disable
	unsigned int lowestMlcstRate;	// 1: use lowest basic rate to send multicast
	unsigned int supportedStaNum;	// limit supported station number
	unsigned int probe_info_enable;	// proc probe_info
	unsigned int probe_info_timeout;	// for proc probe_info

	unsigned int sc_enabled;	//0 is disable, 1 is enable
	int sc_duration_time;	//-1 is always parse, 0 stop parse, >0 parse all packets.
	int sc_get_sync_time;	//unit is second
	int sc_get_profile_time;	//unit is second
	int sc_vxd_rescan_time;	//unit is second
	int sc_connect_timeout;	//unit is second
	int sc_pin_enabled;	// 1, MUST have PIN for SIMPLE CONFIG
	int sc_status;		//0 is not running, -1 is timeout, 1<=x<10 runing, >10 finish
	int sc_debug;
	unsigned char sc_pin[65];
	unsigned char sc_default_pin[65];
	unsigned char sc_passwd[65];
	unsigned char sc_device_name[64];
	unsigned short sc_device_type;
	int sc_ack_round;
	int sc_check_link_time;
	int sc_sync_vxd_to_root;
	unsigned int sc_control_ip;
	int sc_check_level;	//default value is 2. 0, don't check packet length; 1, check the first profile packet length; 2, check all profile packet length
	int sc_ignore_overlap;	//0, Simple Config will fail when more than one Smart Phone send config packet. 1, ignore overlap device packet.
	int sc_reset_beacon_psk;	//0, close/open interface when receive profile and try to connect remote AP; 1. reset psk and beacon only when try to connect remote AP
	int sc_security_type;
	int sc_fix_channel;	//0, don't fix channel; others, the remote AP's channel

	unsigned char wnmtest;	// CONFIG_IEEE80211V

	unsigned char deauth_mac[MACADDRLEN];	// CONFIG_IEEE80211W
	unsigned char sa_req_mac[MACADDRLEN];	// CONFIG_IEEE80211W_CLI
	unsigned char pmf_cli_test;	// CONFIG_IEEE80211W_CLI
	unsigned char pmftest;	// CONFIG_IEEE80211W
	unsigned int channel_utili_beaconIntval;

	/* below is for 802.11k radio measurement */
	unsigned char dot11RadioMeasurementActivated;
	unsigned char dot11RMLinkMeasurementActivated;
	unsigned char dot11RMNeighborReportActivated;
	unsigned char dot11RMBeaconPassiveMeasurementActivated;
	unsigned char dot11RMBeaconActiveMeasurementActivated;
	unsigned char dot11RMBeaconTableMeasurementActivated;
	unsigned char dot11RMAPChannelReportActivated;
	unsigned int dot11RMNeighborReportExpireTime;
};

/* add for 802.11d */
struct Dot1180211CountryCodeEntry {
	unsigned int dot11CountryCodeSwitch;	// 1=enabled; 0=disabled
	unsigned char dot11CountryString[3];
	unsigned int dot11CountryCodeToRegDomain;
};

struct Dot1180211AuthEntry {
	unsigned int dot11AuthAlgrthm;	// 802.11 auth, could be open, shared, auto
	unsigned char dot11PrivacyAlgrthm;	// encryption algorithm, could be none, wep40, TKIP, CCMP, wep104
	unsigned int dot11PrivacyKeyIndex;	// this is only valid for legendary wep, 0~3 for key id.
	unsigned int dot11PrivacyKeyLen;	// this could be 40 or 104
	int dot11EnablePSK;	// 0: disable, bit0: WPA, bit1: WPA2
	int dot11WPACipher;	// bit0-wep64, bit1-tkip, bit2-wrap,bit3-ccmp, bit4-wep128
	int dot11WPA2Cipher;	// bit0-wep64, bit1-tkip, bit2-wrap,bit3-ccmp, bit4-wep128
	unsigned char dot11PassPhrase[65];	// passphrase
	unsigned char dot11PassPhraseGuest[65];	// passphrase of guest
	unsigned long dot11GKRekeyTime;	// group key rekey time, 0 - disable
	unsigned long dot11UKRekeyTime;	// unicast key rekey time, 0 - disable
	unsigned char dot11IEEE80211W;	// 0: disabled, 1: capable, 2:required
	unsigned char dot11EnableSHA256;	// 0: disabled, 1: enabled
};

struct Dot118021xAuthEntry {
	unsigned int dot118021xAlgrthm;	// could be null, 802.1x/PSK
	unsigned int dot118021xDefaultPort;	// used as AP mode for default ieee8021x control port
	unsigned int dot118021xcontrolport;
	unsigned int acct_enabled;
	unsigned long acct_timeout_period;
	unsigned int acct_timeout_throughput;
};

union Keytype {
	unsigned char skey[16];
	unsigned int lkey[4];
};

struct Dot11DefaultKeysTable {
	union Keytype keytype[4];
};

union PN48 {
	unsigned long long val48;

#if defined _LITTLE_ENDIAN_
	struct {
		unsigned char TSC0;
		unsigned char TSC1;
		unsigned char TSC2;
		unsigned char TSC3;
		unsigned char TSC4;
		unsigned char TSC5;
		unsigned char TSC6;
		unsigned char TSC7;
	} _byte_;

#elif defined _BIG_ENDIAN_
	struct {
		unsigned char TSC7;
		unsigned char TSC6;
		unsigned char TSC5;
		unsigned char TSC4;
		unsigned char TSC3;
		unsigned char TSC2;
		unsigned char TSC1;
		unsigned char TSC0;
	} _byte_;

#endif
};

struct Dot11EncryptKey {
	unsigned int dot11TTKeyLen;
	unsigned int dot11TMicKeyLen;
	union Keytype dot11TTKey;
	union Keytype dot11TMicKey1;
	union Keytype dot11TMicKey2;
	union PN48 dot11TXPN48;
	union PN48 dot11RXPN48;
};

struct Dot11KeyMappingsEntry {
	unsigned int dot11Privacy;
	unsigned int keyInCam;	// Is my key in CAM?
	unsigned int keyid;
	struct Dot11EncryptKey dot11EncryptKey;
};

struct Dot11RsnIE {
	unsigned char rsnie[128];
	unsigned char rsnielen;
};

struct Dot11OperationEntry {
	unsigned char hwaddr[MACADDRLEN];
	unsigned int opmode;
	unsigned int hiddenAP;
	unsigned int dot11RTSThreshold;
	unsigned int dot11FragmentationThreshold;
	unsigned int dot11ShortRetryLimit;
	unsigned int dot11LongRetryLimit;
	unsigned int expiretime;
	unsigned int ledtype;
	unsigned int ledroute;
	unsigned int iapp_enable;
	unsigned int block_relay;
	unsigned int deny_any;
	unsigned int crc_log;
	unsigned int wifi_specific;
	unsigned int disable_txsc;
	unsigned int disable_rxsc;
	unsigned int disable_brsc;
	int keep_rsnie;
	int guest_access;
	unsigned int tdls_prohibited;
	unsigned int tdls_cs_prohibited;
};

struct Dot11RFEntry {
	unsigned int dot11RFType;
	unsigned char dot11channel;
	unsigned char band5GSelected;	// bit0: Band1, bit1: Band2, bit2: Band3, bit3: Band4
	unsigned int dot11ch_low;
	unsigned int dot11ch_hi;
	unsigned char pwrlevelCCK_A[MAX_2G_CHANNEL_NUM];
	unsigned char pwrlevelCCK_B[MAX_2G_CHANNEL_NUM];
	unsigned char pwrlevelHT40_1S_A[MAX_2G_CHANNEL_NUM];
	unsigned char pwrlevelHT40_1S_B[MAX_2G_CHANNEL_NUM];
	unsigned char pwrdiffHT40_2S[MAX_2G_CHANNEL_NUM];
	unsigned char pwrdiffHT20[MAX_2G_CHANNEL_NUM];
	unsigned char pwrdiffOFDM[MAX_2G_CHANNEL_NUM];
	unsigned char power_percent;

};

struct ibss_priv {
	unsigned short atim_win;
};

struct bss_desc {
	unsigned char bssid[MACADDRLEN];
	unsigned char ssid[32];
	unsigned char *ssidptr;	// unused, for backward compatible
	unsigned short ssidlen;
	unsigned char meshid[MESH_ID_LEN];
	unsigned char *meshidptr;	// unused, for backward compatible
	unsigned short meshidlen;
	unsigned int bsstype;
	unsigned short beacon_prd;
	unsigned char dtim_prd;
	unsigned int t_stamp[3];
	struct ibss_priv ibss_par;
	unsigned short capability;
	unsigned char channel;
	unsigned int basicrate;
	unsigned int supportrate;
	unsigned char bdsa[MACADDRLEN];
	unsigned char rssi;
	unsigned char sq;
	unsigned char network;
	/*add for P2P_SUPPORT ; for sync; it exist no matter p2p enabled or not */
	unsigned char p2pdevname[33];
	unsigned char p2prole;
	unsigned short p2pwscconfig;
	unsigned char p2paddress[MACADDRLEN];

};

struct bss_type {
	unsigned char net_work_type;
};

struct erp_mib {
	int protection;		// protection mechanism flag
	int nonErpStaNum;	// none ERP client assoication num
	int olbcDetected;	// OLBC detected
	int olbcExpired;	// expired time of OLBC state
	int shortSlot;		// short slot time flag
	int ctsToSelf;		// CTStoSelf flag
	int longPreambleStaNum;	// number of assocated STA using long preamble
};

struct Dot11DFSEntry {
	unsigned int disable_DFS;	// 1 or 0
	unsigned int disable_tx;	// 1 or 0
	unsigned int DFS_timeout;	// set to 10 ms
	unsigned int DFS_detected;	// 1 or 0
	unsigned int NOP_timeout;	// set to 30 mins
	unsigned int DFS_TXPAUSE_timeout;
	unsigned int CAC_enable;	// 1 or 0
	unsigned int CAC_ss_counter;
	unsigned int reserved1;
	unsigned int reserved2;
	unsigned int reserved3;
	unsigned int reserved4;
};

struct Dot11hTPCEntry {
	unsigned char tpc_enable;	// 1 or 0
	unsigned char tpc_tx_power;
	unsigned char tpc_link_margin;
	unsigned char min_tx_power;
	unsigned char max_tx_power;
};

struct ParaRecord {
	unsigned int ACM;
	unsigned int AIFSN;
	unsigned int ECWmin;
	unsigned int ECWmax;
	unsigned int TXOPlimit;
};

struct Dot11QosEntry {
	unsigned int dot11QosEnable;	// 0=disable, 1=enable
	unsigned int dot11QosAPSD;	// 0=disable, 1=enable
	unsigned int EDCAparaUpdateCount;	// default=0, increases if any STA_AC_XX_paraRecord updated
	unsigned int EDCA_STA_config;	// WMM STA, default=0, will be set when assoc AP's EDCA para have been set
	unsigned char WMM_IE[7];	// WMM STA, WMM IE
	unsigned char WMM_PARA_IE[24];	// WMM EDCA Parameter IE
	unsigned int UAPSD_AC_BE;
	unsigned int UAPSD_AC_BK;
	unsigned int UAPSD_AC_VI;
	unsigned int UAPSD_AC_VO;

	struct ParaRecord STA_AC_BE_paraRecord;
	struct ParaRecord STA_AC_BK_paraRecord;
	struct ParaRecord STA_AC_VI_paraRecord;
	struct ParaRecord STA_AC_VO_paraRecord;

	unsigned int ManualEDCA;	// 0=disable, 1=enable
	struct ParaRecord AP_manualEDCA[4];
	struct ParaRecord STA_manualEDCA[4];
	unsigned char TID_mapping[8];	// 1: BK, 2: BE, 3: VI, 4: VO
};

struct Dot11nConfigEntry {
	unsigned int dot11nSupportedMCS;
	unsigned int dot11nBasicMCS;
	unsigned int dot11nUse40M;	// 0: 20M, 1: 40M
	unsigned int dot11n2ndChOffset;	// 0: don't care, 1: below the primary, 2: above the primary
	unsigned int dot11nShortGIfor20M;
	unsigned int dot11nShortGIfor40M;
	unsigned int dot11nShortGIfor80M;
	unsigned int dot11nSTBC;
	unsigned int dot11nLDPC;
	unsigned int dot11nAMPDU;
	unsigned int dot11nAMSDU;
	unsigned int dot11nAMPDUSendSz;	// 8: 8K, 16: 16K, 32: 32K, 64: 64K, other: auto
	unsigned int dot11nAMPDURevSz;
	unsigned int dot11nAMSDURecvMax;	// 0: 4K, 1: 8K
	unsigned int dot11nAMSDUSendTimeout;	// timeout value to queue AMSDU packets
	unsigned int dot11nAMSDUSendNum;	// max aggregation packet number
	unsigned int dot11nLgyEncRstrct;	// bit0: Wep, bit1: TKIP, bit2: restrict Realtek client, bit3: forbid  N mode for legacy enc
	unsigned int dot11nCoexist;
	unsigned int dot11nCoexist_ch_chk;	// coexist channel chaek
	unsigned int dot11nBGAPRssiChkTh;
	unsigned int dot11nTxNoAck;
	unsigned int dot11nAddBAreject;	//add for support sigma test
};

struct Dot11acConfigEntry {
	unsigned int dot11SupportedVHT;	// b[1:0]: NSS1, b[3:2]: NSS2
	unsigned int dot11VHT_TxMap;	// b[19:10]: NSS2 MCS9~0, b[9:0]:     NSS1 MCS9~0
};

struct ReorderControlEntry {
	unsigned int ReorderCtrlEnable;
	unsigned int reserved;
	unsigned int ReorderCtrlWinSz;
	unsigned int ReorderCtrlTimeout;
};

struct EfuseEntry {
	unsigned int enable_efuse;
};

// driver mib
struct wifi_mib {
	unsigned int mib_version;
	struct Dot11StationConfigEntry dot11StationConfigEntry;
	struct Dot1180211AuthEntry dot1180211AuthEntry;
	struct Dot118021xAuthEntry dot118021xAuthEntry;
	struct Dot11DefaultKeysTable dot11DefaultKeysTable;
	struct Dot11KeyMappingsEntry dot11GroupKeysTable;
	struct Dot11KeyMappingsEntry dot11IGTKTable;	//CONFIG_IEEE80211W
	struct Dot11RsnIE dot11RsnIE;
	struct Dot11OperationEntry dot11OperationEntry;
	struct Dot11RFEntry dot11RFEntry;
	struct bss_desc dot11Bss;
	struct bss_type dot11BssType;
	struct erp_mib dot11ErpInfo;
	struct Dot11DFSEntry dot11DFSEntry;
	struct Dot11hTPCEntry dot11hTPCEntry;
	struct Dot11QosEntry dot11QosEntry;
	struct Dot11nConfigEntry dot11nConfigEntry;
	struct ReorderControlEntry reorderCtrlEntry;
	struct Dot11KeyMappingsEntry dot11sKeysTable;
	struct Dot1180211CountryCodeEntry dot11dCountry;
	struct EfuseEntry efuseEntry;
};

#endif // _IEEE802_MIB_H_

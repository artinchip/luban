#ifndef __TP2825B_H__
#define __TP2825B_H__

//#include <linux/ioctl.h>

#define TP2825_VERSION_CODE KERNEL_VERSION(0, 0, 8)

enum {
	TP2825B = 0x2825,
	TP2850  = 0x2850,
	TP2860  = 0x2860,
};
enum {
	TP2802_1080P25      = 0x03,
	TP2802_1080P30      = 0x02,
	TP2802_720P25       = 0x05,
	TP2802_720P30       = 0x04,
	TP2802_720P50       = 0x01,
	TP2802_720P60       = 0x00,
	TP2802_SD           = 0x06,
	INVALID_FORMAT      = 0x07,
	TP2802_720P25V2     = 0x0D,
	TP2802_720P30V2     = 0x0C,
	TP2802_PAL          = 0x08,
	TP2802_NTSC         = 0x09,
	TP2802_3M18         = 0x20,   //2048x1536@18.75 for TVI
	TP2802_5M12         = 0x21,   //2592x1944@12.5 for TVI
	TP2802_4M15         = 0x22,   //2688x1520@15 for TVI
	TP2802_3M20         = 0x23,   //2048x1536@20 for TVI
	TP2802_4M12         = 0x24,   //2688x1520@12.5 for TVI
	TP2802_6M10         = 0x25,   //3200x1800@10 for TVI
	TP2802_QHD30        = 0x26,   //2560x1440@30 for TVI/HDA/HDC
	TP2802_QHD25        = 0x27,   //2560x1440@25 for TVI/HDA/HDC
	TP2802_QHD15        = 0x28,   //2560x1440@15 for HDA
	TP2802_QXGA18       = 0x29,   //2048x1536@18 for HDA/TVI
	TP2802_QXGA30       = 0x2A,   //2048x1536@30 for HDA
	TP2802_QXGA25       = 0x2B,   //2048x1536@25 for HDA
	TP2802_4M30         = 0x2C,   //2688x1520@30 for TVI(for future)
	TP2802_4M25         = 0x2D,   //2688x1520@25 for TVI(for future)
	TP2802_5M20         = 0x2E,   //2592x1944@20 for TVI/HDA
	TP2802_8M15         = 0x2f,   //3840x2160@15 for TVI
	TP2802_8M12         = 0x30,   //3840x2160@12.5 for TVI
	TP2802_1080P15      = 0x31,   //1920x1080@15 for TVI
	TP2802_1080P60      = 0x32,   //1920x1080@60 for TVI
	TP2802_960P30       = 0x33,   //1280x960@30 for TVI
	TP2802_1080P20      = 0x34,   //1920x1080@20 for TVI
	TP2802_1080P50      = 0x35,   //1920x1080@50 for TVI
	TP2802_720P14       = 0x36,   //1280x720@14 for TVI
	TP2802_720P30HDR    = 0x37,   //1280x720@30 for TVI
	TP2802_6M20         = 0x38,   //2960x1920@20 for CVI
	TP2802_8M15V2       = 0x39,   //3264x2448@15 for TVI
	TP2802_5M20V2       = 0x3a,   //2960x1664@20 for TVI
	TP2802_8M7          = 0x3b,   //3720x2160@7.5 for AHD
	TP2802_3M20V2       = 0x3c,   //2304x1296@20 for TVI
	TP2802_1080P2750    = 0x3d,   //1920x1080@27.5 for TVI
	TP2802_1080P2700    = 0x3e,   //1920x1080@27.5 for TVI
	TP2802_1080P12      = 0x41,   //1920x1080@12.5
	TP2802_5M12V2       = 0x42,   //2960x1664@12.5 for TVI
	TP2802_960P25       = 0x43,   //1280x960@25 for AHD
};
enum {
	VIDEO_UNPLUG,
	VIDEO_IN,
	VIDEO_LOCKED,
	VIDEO_UNLOCK
};
enum {
	MUX656_8BIT,    //Y/C-mux 4:2:2 8-bit with embedded sync
	SEP656_8BIT,    //Y/C-mux 4:2:2 8-bit with separate sync
	EMB422_16BIT,   //only TP2825B YCbCr 4:2:2 16-bit with embedded sync
	SEP422_16BIT,   //only TP2825B YCbCr 4:2:2 16-bit with separate sync
	MIPI_2LANES,    //only TP2850/TP2860
	MIPI_1LANES,    //only TP2860
};
enum {
	VIDEO_PAGE = 0, //
	MIPI_PAGE = 8
};
enum {
	SCAN_DISABLE = 0,
	SCAN_AUTO,
	SCAN_TVI,
	SCAN_HDA,
	SCAN_HDC,
	SCAN_MANUAL,
	SCAN_TEST
};
enum {
	STD_TVI,
	STD_HDA,
	STD_HDC,
	STD_HDA_DEFAULT,
	STD_HDC_DEFAULT
};
enum {
	PTZ_TVI = 0,
	PTZ_HDA_1080P = 1,
	PTZ_HDA_720P = 2,
	PTZ_HDA_CVBS = 3,
	PTZ_HDC = 4,
	PTZ_HDA_3M18 = 5, //HDA QXGA18
	PTZ_HDA_3M25 = 6, //HDA QXGA25,QXGA30
	PTZ_HDA_4M25 = 7, //HDA QHD25,QHD30,5M20
	PTZ_HDA_4M15 = 8, //HDA QHD15,5M12.5
	PTZ_HDC_QHD = 9, //HDC QHD25,QHD30
	PTZ_HDC_FIFO = 10, //HDC 1080p,720p FIFO
	PTZ_HDC_8M12 = 11, //HDC 8M12.5
	PTZ_HDC_8M15 = 12, //HDC 8M15
	PTZ_HDC_6M20 = 13 //HDC 6M20
};
enum {
	PTZ_RX_TVI_CMD,
	PTZ_RX_TVI_BURST,
	PTZ_RX_ACP1,
	PTZ_RX_ACP2,
	PTZ_RX_ACP3,
	PTZ_RX_TEST,
	PTZ_RX_HDC1,
	PTZ_RX_HDC2
};
enum {
	PTZ_PIN_TXD = 0, //only for TP2825B
	PTZ_PIN_PTZ1 = 1,
	PTZ_PIN_PTZ2 = 2,
};
enum {
	SINGLE_VIN1 = 0,
	SINGLE_VIN2 = 1,
	SINGLE_VIN3 = 2,
	SINGLE_VIN4 = 3,
	SINGLE_VIN5 = 4, //only for TP2825B
	DIFF_VIN12 = 6,
	DIFF_VIN34 = 7
};

#define FLAG_LOSS		0x80
#define FLAG_H_LOCKED		0x20
#define FLAG_HV_LOCKED		0x60

#define FLAG_HDC_MODE		0x80
#define FLAG_HALF_MODE		0x40
#define FLAG_MEGA_MODE		0x20
#define FLAG_HDA_MODE		0x10

#define CHANNELS_PER_CHIP	4
#define MAX_CHIPS		2
#define SUCCESS			0
#define FAILURE			-1

#define BRIGHTNESS		0x10
#define CONTRAST		0x11
#define SATURATION		0x12
#define HUE			0X13
#define SHARPNESS		0X14

#define MAX_COUNT		0xffff

typedef struct _tp2802_register
{
	unsigned char chip;
	unsigned char ch;
	unsigned int reg_addr;
	unsigned int value;
} tp2802_register;

typedef struct _tp2802_work_mode
{
	unsigned char chip;
	unsigned char ch;
	unsigned char mode;
} tp2802_work_mode;

typedef struct _tp2802_video_mode
{
	unsigned char chip;
	unsigned char ch;
	unsigned char mode;
	unsigned char std;
} tp2802_video_mode;

typedef struct _tp2802_video_loss
{
	unsigned char chip;
	unsigned char ch;
	unsigned char is_lost;
} tp2802_video_loss;

typedef struct _tp2802_image_adjust
{
	unsigned char chip;
	unsigned char ch;
	unsigned int hue;
	unsigned int contrast;
	unsigned int brightness;
	unsigned int saturation;
	unsigned int sharpness;
} tp2802_image_adjust;

typedef struct _tp2802_PTZ_data
{
	unsigned char chip;
	unsigned char ch;
	unsigned char mode;
	unsigned char data[128];
} tp2802_PTZ_data;

// IOCTL Definitions
#define TP2802_IOC_MAGIC		'v'

#define TP2802_READ_REG			_IOWR(TP2802_IOC_MAGIC, 1, tp2802_register)
#define TP2802_WRITE_REG		_IOW(TP2802_IOC_MAGIC, 2, tp2802_register)
#define TP2802_SET_VIDEO_MODE		_IOW(TP2802_IOC_MAGIC, 3, tp2802_video_mode)
#define TP2802_GET_VIDEO_MODE		_IOWR(TP2802_IOC_MAGIC, 4, tp2802_video_mode)
#define TP2802_GET_VIDEO_LOSS		_IOWR(TP2802_IOC_MAGIC, 5, tp2802_video_loss)
#define TP2802_SET_IMAGE_ADJUST		_IOW(TP2802_IOC_MAGIC, 6, tp2802_image_adjust)
#define TP2802_GET_IMAGE_ADJUST		_IOWR(TP2802_IOC_MAGIC, 7, tp2802_image_adjust)
#define TP2802_SET_PTZ_DATA		_IOW(TP2802_IOC_MAGIC, 8, tp2802_PTZ_data)
#define TP2802_GET_PTZ_DATA		_IOWR(TP2802_IOC_MAGIC, 9, tp2802_PTZ_data)
#define TP2802_SET_SCAN_MODE		_IOW(TP2802_IOC_MAGIC, 10, tp2802_work_mode)
#define TP2802_DUMP_REG			_IOW(TP2802_IOC_MAGIC, 11, tp2802_register)
#define TP2802_FORCE_DETECT		_IOW(TP2802_IOC_MAGIC, 12, tp2802_work_mode)
#define TP2802_SET_SAMPLE_RATE		_IOW(TP2802_IOC_MAGIC, 13, tp2802_audio_samplerate)
#define TP2802_SET_AUDIO_PLAYBACK	_IOW(TP2802_IOC_MAGIC, 14, tp2802_audio_playback)
#define TP2802_SET_AUDIO_DA_VOLUME	_IOW(TP2802_IOC_MAGIC, 15, tp2802_audio_da_volume)
#define TP2802_SET_AUDIO_DA_MUTE	_IOW(TP2802_IOC_MAGIC, 16, tp2802_audio_da_mute)
#define TP2802_SET_BURST_DATA		_IOW(TP2802_IOC_MAGIC, 17, tp2802_PTZ_data)
#define TP2802_SET_VIDEO_INPUT		_IOW(TP2802_IOC_MAGIC, 18, tp2802_register)
#define TP2802_GET_VIDEO_INPUT		_IOW(TP2802_IOC_MAGIC, 19, tp2802_register)
#define TP2802_SET_PTZ_MODE		_IOW(TP2802_IOC_MAGIC, 20, tp2802_PTZ_data)
#define TP2802_SET_RX_MODE		_IOW(TP2802_IOC_MAGIC, 21, tp2802_PTZ_data)

#define TP2802_SET_FIFO_DATA		_IOW(TP2802_IOC_MAGIC, 25, tp2802_PTZ_data)
#define TP2802_SET_FIFO_MODE		_IOW(TP2802_IOC_MAGIC, 26, tp2802_PTZ_data)

#endif // end of __TP2825B_H__

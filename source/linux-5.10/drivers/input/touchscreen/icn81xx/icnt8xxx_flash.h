#ifndef _ICN8XXX_FLASH_H_
#define _ICN8XXX_FLASH_H_
#include <linux/delay.h>
#include "cts_core.h"
#include "cts_firmware.h"

//-----------------------------------------------------------------------------
// Global CONSTANTS
//-----------------------------------------------------------------------------
 #define MD25D40_ID1                      0x514013
 #define MD25D40_ID2                      0xC84013
 #define MD25D20_ID1                      0x514012
 #define MD25D20_ID2                      0xC84012
 #define GD25Q10_ID                        0xC84011
 #define MX25L512E_ID                     0xC22010
 #define MD25D05_ID                         0x514010
 #define MD25D10_ID                         0x514011

#define FLASH_TOTAL_SIZE                0x00010000     
#define FLASH_PAGE_SIZE                 0x1000 
#define FLASH_AHB_BASE_ADDR             0x00100000 
#define FLASH_PATCH_PARA_BASE_ADDR      (FLASH_TOTAL_SIZE - FLASH_PAGE_SIZE)          //  allocate 1 page for patch para, 0xff00
#define FLASH_CODE_INFO_BASE_ADDR       (FLASH_PATCH_PARA_BASE_ADDR - FLASH_PAGE_SIZE)        //  0xfe00,allocate 1 page for system para
#define FLASH_CRC_ADDR                  (FLASH_AHB_BASE_ADDR + FLASH_CODE_INFO_BASE_ADDR + 0x00) //  0xfe00
#define FLASH_CODE_LENGTH_ADDR          (FLASH_AHB_BASE_ADDR + FLASH_CODE_INFO_BASE_ADDR + 0x04) //  0xfe04

#define    SFCTL_BASE_87       (0x0000F600)

#define    CMD_SEL_87          (SFCTL_BASE_87 + 0x0000)
#define    FLASH_ADDR_87       (SFCTL_BASE_87 + 0x0004)
#define    SRAM_ADDR_87        (SFCTL_BASE_87 + 0x0008)
#define    DATA_LENGTH_87      (SFCTL_BASE_87 + 0x000C)
#define    START_DEXC_87       (SFCTL_BASE_87 + 0x0010)
#define    RELEASE_FLASH_87    (SFCTL_BASE_87 + 0x0014)
#define    CLEAR_HW_STATE_87   (SFCTL_BASE_87 + 0x0018)
#define    CRC_RESULT_87       (SFCTL_BASE_87 + 0x001C)
#define    SW_CRC_START_87     (SFCTL_BASE_87 + 0x0020)
#define    SF_BUSY_87          (SFCTL_BASE_87 + 0x0024)
#define    WATCHDOG_CRC_CFG_87 (SFCTL_BASE_87 + 0x0028)

#define    SFCTL_BASE_89       (0x040600)
#define    CMD_SEL_89          (SFCTL_BASE_89 + 0x0000)
#define    FLASH_ADDR_89       (SFCTL_BASE_89 + 0x0004)
#define    SRAM_ADDR_89        (SFCTL_BASE_89 + 0x0008)
#define    DATA_LENGTH_89      (SFCTL_BASE_89 + 0x000C)
#define    START_DEXC_89       (SFCTL_BASE_89 + 0x0010)
#define    RELEASE_FLASH_89    (SFCTL_BASE_89 + 0x0014)
#define    CLEAR_HW_STATE_89   (SFCTL_BASE_89 + 0x0018)
#define    CRC_RESULT_89       (SFCTL_BASE_89 + 0x001C)
#define    SW_CRC_START_89     (SFCTL_BASE_89 + 0x0020)
#define    SF_BUSY_89          (SFCTL_BASE_89 + 0x0024)

#define FLASH_CMD_FAST_READ                        0x01
#define FLASH_CMD_ERASE_SECTOR                     0x02
#define FLASH_CMD_ERASE_BLOCK                      0x03
#define FLASH_CMD_PAGE_PROGRAM                     0x04
#define FLASH_CMD_READ_STATUS                      0x05
#define FLASH_CMD_READ_IDENTIFICATION              0x06

#define FLASH_EARSE_4K                             0
#define FLASH_EARSE_32K                            1

#define FLASH_STOR_INFO_ADDR                       0xe000
#define SRAM_EXCHANGE_ADDR                         0xd000
#define SRAM_EXCHANGE_ADDR1                        0xd100
#define SRAM_EXCHANGE_ADDR_89                      0x026000
#define SRAM_EXCHANGE_ADDR1_89                     0x026100
#define FIRMWARA_INFO_AT_BIN_ADDR                  0x00f4

#define B_SIZE                            120//96//32 //128//64//32


//-----------------------------------------------------------------------------
// Macro DEFINITIONS
//-----------------------------------------------------------------------------


//#define swap_ab(a,b)       {char temp;temp=a;a=b;b=temp;}
//#define U16LOBYTE(var)     (*(unsigned char *) &var) 
//#define U16HIBYTE(var)     (*(unsigned char *)((unsigned char *) &var + 1))     

#define STRUCT_OFFSET(StructName,MemberName) ((int)(&(((StructName*)0)->MemberName)))


//-----------------------------------------------------------------------------
// Struct, Union and Enum DEFINITIONS
//-----------------------------------------------------------------------------

typedef enum
{
    R_OK = 0,
    R_FILE_ERR,
    R_STATE_ERR,
    R_ERASE_ERR,
    R_PROGRAM_ERR,
    R_VERIFY_ERR,
}E_UPGRADE_ERR_TYPE;

//-----------------------------------------------------------------------------
// Global VARIABLES
//-----------------------------------------------------------------------------
// junfuzhang 20160913, check crc in sram before boot
extern unsigned short icnt87_sram_crc;
extern unsigned short icnt87_sram_length;
//junfuzhang 20160913, adding end

//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------

extern unsigned short icnt87_sram_crc;
extern unsigned short icnt87_sram_length;

extern int icn87xx_calculate_crc(struct cts_device *cts_dev, unsigned short len);
extern int icn87xx_boot_sram(struct cts_device *cts_dev);
extern int  icn85xx_bootfrom_sram(struct cts_device *cts_dev);
extern int  icn89xx_bootfrom_sram(struct cts_device *cts_dev);

extern int  icn85xx_read_flashid(struct cts_device *cts_dev);
extern int  icn87xx_read_flashid(struct cts_device *cts_dev);
extern int  icn89xx_read_flashid(struct cts_device *cts_dev);

extern int  icnt85xx_fw_update(struct cts_device *cts_dev,const struct cts_firmware *firmware, bool to_flash);
extern int  icnt87xx_fw_update(struct cts_device *cts_dev,const struct cts_firmware *firmware, bool to_flash);
extern int  icnt89xx_fw_update(struct cts_device *cts_dev,const struct cts_firmware *firmware, bool to_flash);

#endif


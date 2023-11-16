#define LOG_TAG         "Firmware"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "icnt8xxx_flash.h"

#include "cts_firmware.h"
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/namei.h>

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#include "icnt89xx_fw.h"
#include "icnt88xx_fw.h"
#include "icnt87xx_fw.h"
#include "icnt86xx_fw.h"
#include "icnt85xx_fw.h"
#include "icnt82xx_fw.h"
#include "icnt81xx_fw.h"

struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "ICNT89xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT89XX,
        .fwid = CTS_FWID_ICNT89XX,
        .data = icnt89xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt89xx_driver_builtin_firmware),
        .ver_offset = 0x114 //maybe = 0x100
    },
    {
        .name = "ICNT86xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT86XX,
        .fwid = CTS_FWID_ICNT86XX,
        .data = icnt86xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt86xx_driver_builtin_firmware),
        .ver_offset = 0x100
    },
    {
        .name = "ICNT88xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT88XX,
        .fwid = CTS_FWID_ICNT88XX,
        .data = icnt88xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt88xx_driver_builtin_firmware),
        .ver_offset = 0x100
    },
    {
        .name = "ICNT87xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT87XX,
        .fwid = CTS_FWID_ICNT87XX,
        .data = icnt87xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt87xx_driver_builtin_firmware),
        .ver_offset = 0x100
    },
    {
        .name = "ICNT85xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT85XX,
        .fwid = CTS_FWID_ICNT85XX,
        .data = icnt85xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt85xx_driver_builtin_firmware),
        .ver_offset = 0x100
    },
    {
        .name = "ICNT82xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT82XX,
        .fwid = CTS_FWID_ICNT82XX,
        .data = icnt82xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt82xx_driver_builtin_firmware),
        .ver_offset = 0x114
    },
    {
        .name = "ICNT81xx",      /* MUST set non-NULL */
        .hwid = CTS_HWID_ICNT81XX,
        .fwid = CTS_FWID_ICNT81XX,
        .data = icnt81xx_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnt81xx_driver_builtin_firmware),
        .ver_offset = 0x100
    },
};

#define NUM_DRIVER_BUILTIN_FIRMWARE ARRAY_SIZE(cts_driver_builtin_firmwares)
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef  SUPPORT_SENSOR_ID
#define MAX_SUPPORT_ID_NUM  9u

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
    struct _sensor_id_a {
        u8 id;
        u8 *array;
        int size;
    };
    struct sensor_id_array {
        char *name;
        struct _sensor_id_a  id_tables[MAX_SUPPORT_ID_NUM+1];
    };
    static struct sensor_id_array sensor_id_array_table[] = {
        { "ICNT89xx", {
            {0x00, icnt89xx_driver_builtin_firmware_00, sizeof(icnt89xx_driver_builtin_firmware_00),},
            {0x01, icnt89xx_driver_builtin_firmware_01, sizeof(icnt89xx_driver_builtin_firmware_01),},
            {0x02, icnt89xx_driver_builtin_firmware_02, sizeof(icnt89xx_driver_builtin_firmware_02),},
            {0x10, icnt89xx_driver_builtin_firmware_10, sizeof(icnt89xx_driver_builtin_firmware_10),},
            {0x11, icnt89xx_driver_builtin_firmware_11, sizeof(icnt89xx_driver_builtin_firmware_11),},
            {0x12, icnt89xx_driver_builtin_firmware_12, sizeof(icnt89xx_driver_builtin_firmware_12),},
            {0x20, icnt89xx_driver_builtin_firmware_20, sizeof(icnt89xx_driver_builtin_firmware_20),},
            {0x21, icnt89xx_driver_builtin_firmware_21, sizeof(icnt89xx_driver_builtin_firmware_21),},
            {0x22, icnt89xx_driver_builtin_firmware_22, sizeof(icnt89xx_driver_builtin_firmware_22),},
            {0xff, icnt89xx_driver_builtin_firmware, sizeof(icnt89xx_driver_builtin_firmware),},
            },
        },
        { "ICNT86xx", {
            {0x00, icnt86xx_driver_builtin_firmware_00, sizeof(icnt86xx_driver_builtin_firmware_00),},
            {0x01, icnt86xx_driver_builtin_firmware_01, sizeof(icnt86xx_driver_builtin_firmware_01),},
            {0x02, icnt86xx_driver_builtin_firmware_02, sizeof(icnt86xx_driver_builtin_firmware_02),},
            {0x10, icnt86xx_driver_builtin_firmware_10, sizeof(icnt86xx_driver_builtin_firmware_10),},
            {0x11, icnt86xx_driver_builtin_firmware_11, sizeof(icnt86xx_driver_builtin_firmware_11),},
            {0x12, icnt86xx_driver_builtin_firmware_12, sizeof(icnt86xx_driver_builtin_firmware_12),},
            {0x20, icnt86xx_driver_builtin_firmware_20, sizeof(icnt86xx_driver_builtin_firmware_20),},
            {0x21, icnt86xx_driver_builtin_firmware_21, sizeof(icnt86xx_driver_builtin_firmware_21),},
            {0x22, icnt86xx_driver_builtin_firmware_22, sizeof(icnt86xx_driver_builtin_firmware_22),},
            {0xff, icnt86xx_driver_builtin_firmware, sizeof(icnt86xx_driver_builtin_firmware),},
            },
        },
        { "ICNT87xx", {
            {0x00, icnt87xx_driver_builtin_firmware_00, sizeof(icnt87xx_driver_builtin_firmware_00),},
            {0x01, icnt87xx_driver_builtin_firmware_01, sizeof(icnt87xx_driver_builtin_firmware_01),},
            {0x02, icnt87xx_driver_builtin_firmware_02, sizeof(icnt87xx_driver_builtin_firmware_02),},
            {0x10, icnt87xx_driver_builtin_firmware_10, sizeof(icnt87xx_driver_builtin_firmware_10),},
            {0x11, icnt87xx_driver_builtin_firmware_11, sizeof(icnt87xx_driver_builtin_firmware_11),},
            {0x12, icnt87xx_driver_builtin_firmware_12, sizeof(icnt87xx_driver_builtin_firmware_12),},
            {0x20, icnt87xx_driver_builtin_firmware_20, sizeof(icnt87xx_driver_builtin_firmware_20),},
            {0x21, icnt87xx_driver_builtin_firmware_21, sizeof(icnt87xx_driver_builtin_firmware_21),},
            {0x22, icnt87xx_driver_builtin_firmware_22, sizeof(icnt87xx_driver_builtin_firmware_22),},
            {0xff, icnt87xx_driver_builtin_firmware, sizeof(icnt87xx_driver_builtin_firmware),},
            },
        },
        { "ICNT81xx", {
            {0x00, icnt81xx_driver_builtin_firmware_00, sizeof(icnt81xx_driver_builtin_firmware_00),},
            {0x01, icnt81xx_driver_builtin_firmware_01, sizeof(icnt81xx_driver_builtin_firmware_01),},
            {0x02, icnt81xx_driver_builtin_firmware_02, sizeof(icnt81xx_driver_builtin_firmware_02),},
            {0x10, icnt81xx_driver_builtin_firmware_10, sizeof(icnt81xx_driver_builtin_firmware_10),},
            {0x11, icnt81xx_driver_builtin_firmware_11, sizeof(icnt81xx_driver_builtin_firmware_11),},
            {0x12, icnt81xx_driver_builtin_firmware_12, sizeof(icnt81xx_driver_builtin_firmware_12),},
            {0x20, icnt81xx_driver_builtin_firmware_20, sizeof(icnt81xx_driver_builtin_firmware_20),},
            {0x21, icnt81xx_driver_builtin_firmware_21, sizeof(icnt81xx_driver_builtin_firmware_21),},
            {0x22, icnt81xx_driver_builtin_firmware_22, sizeof(icnt81xx_driver_builtin_firmware_22),},
            {0xff, icnt81xx_driver_builtin_firmware, sizeof(icnt81xx_driver_builtin_firmware),},
            },
        },
        // if you want support other chip  value ,please add here
     };
#endif

#ifdef CFG_CTS_FIRMWARE_IN_FS
    struct _sensor_id_b {
        u8 id;
        const char *bin;
    };
    struct sensor_id_fs {
        char *name;
        struct _sensor_id_b  id_tables[MAX_SUPPORT_ID_NUM +1];
    };
    static struct sensor_id_fs sensor_id_bin_table[] = {
        { "ICNT89xx", {
            {0x00, "/etc/firmware/ICNT89xx_00.bin",},
            {0x01, "/etc/firmware/ICNT89xx_01.bin",},
            {0x02, "/etc/firmware/ICNT89xx_02.bin",},
            {0x10, "/etc/firmware/ICNT89xx_10.bin",},
            {0x11, "/etc/firmware/ICNT89xx_11.bin",},
            {0x12, "/etc/firmware/ICNT89xx_12.bin",},
            {0x20, "/etc/firmware/ICNT89xx_20.bin",},
            {0x21, "/etc/firmware/ICNT89xx_21.bin",},
            {0x22, "/etc/firmware/ICNT89xx_22.bin",},
            {0xff, "/etc/firmware/ICNT89xx.bin",},
            },
        },
        { "ICNT86xx", {
            {0x00, "/etc/firmware/ICNT86xx_00.bin",},
            {0x01, "/etc/firmware/ICNT86xx_01.bin",},
            {0x02, "/etc/firmware/ICNT86xx_02.bin",},
            {0x10, "/etc/firmware/ICNT86xx_10.bin",},
            {0x11, "/etc/firmware/ICNT86xx_11.bin",},
            {0x12, "/etc/firmware/ICNT86xx_12.bin",},
            {0x20, "/etc/firmware/ICNT86xx_20.bin",},
            {0x21, "/etc/firmware/ICNT86xx_21.bin",},
            {0x22, "/etc/firmware/ICNT86xx_22.bin",},
            {0xff, "/etc/firmware/ICNT86xx.bin",},
            },
        },
        { "ICNT87xx", {
            {0x00, "/etc/firmware/ICNT87xx_00.bin",},
            {0x01, "/etc/firmware/ICNT87xx_01.bin",},
            {0x02, "/etc/firmware/ICNT87xx_02.bin",},
            {0x10, "/etc/firmware/ICNT87xx_10.bin",},
            {0x11, "/etc/firmware/ICNT87xx_11.bin",},
            {0x12, "/etc/firmware/ICNT87xx_12.bin",},
            {0x20, "/etc/firmware/ICNT87xx_20.bin",},
            {0x21, "/etc/firmware/ICNT87xx_21.bin",},
            {0x22, "/etc/firmware/ICNT87xx_22.bin",},
            {0xff, "/etc/firmware/ICNT87xx.bin",},
            },
        },
        { "ICNT81xx", {
            {0x00, "/etc/firmware/ICNT81xx_00.bin",},
            {0x01, "/etc/firmware/ICNT81xx_01.bin",},
            {0x02, "/etc/firmware/ICNT81xx_02.bin",},
            {0x10, "/etc/firmware/ICNT81xx_10.bin",},
            {0x11, "/etc/firmware/ICNT81xx_11.bin",},
            {0x12, "/etc/firmware/ICNT81xx_12.bin",},
            {0x20, "/etc/firmware/ICNT81xx_20.bin",},
            {0x21, "/etc/firmware/ICNT81xx_21.bin",},
            {0x22, "/etc/firmware/ICNT81xx_22.bin",},
            {0xff, "/etc/firmware/ICNT81xx.bin",},
            },
        },
        // if you want support other chip  value ,please add here
     };
#endif

#else  //undefine SUPPORT_SENSOR_ID

#ifdef CFG_CTS_FIRMWARE_IN_FS
    struct bin_in_fs {
        char *name;
        const char *bin;
    };
    static struct bin_in_fs  bin_table[] = {
       { "ICNT89xx", "/etc/firmware/ICNT89xx.bin",},
       { "ICNT86xx", "/etc/firmware/ICNT86xx.bin",},
       { "ICNT87xx", "/etc/firmware/ICNT87xx.bin",},
       { "ICNT85xx", "/etc/firmware/ICNT85xx.bin",},
       { "ICNT82xx", "/etc/firmware/ICNT82xx.bin",},
       { "ICNT81xx", "/data/ICNT81xx.bin",},// { "ICNT81xx", "/etc/firmware/ICNT81xx.bin",},
    // if you want support other chip  value ,please add here
     };
#endif


#endif
#define CTS_FIRMWARE_FILE_SIZE    (48*1024)
//#define CTS_FIRMWARE_MULTI_SECTION_FILE_SIZE    (0x20000)
//#define CTS_SECTION_ENABLE_FLAG                 (0x0000C35A)

#define FIRMWARE_VERSION_OFFSET     0x100
#define FIRMWARE_VERSION(firmware, offset)  \
    get_unaligned_le16((firmware)->data + offset)

#if defined CFG_CTS_FIRMWARE_IN_FS
static bool is_firmware_size_valid(const struct cts_firmware *firmware)
{
    return (firmware->size > 0x102 &&
            firmware->size <= CTS_FIRMWARE_FILE_SIZE);
}
#endif

#if defined CFG_CTS_DRIVER_BUILTIN_FIRMWARE
static bool is_firmware_valid(const struct cts_firmware *firmware)
{
    if (firmware && firmware->data && is_firmware_size_valid(firmware)) {
       return true;  
    }
    return false;
}
#endif

#ifdef SUPPORT_SENSOR_ID

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
int cts_compare_sensor_id_from_bultin(const struct cts_device *cts_dev, 
    struct cts_firmware *firmware)
{
    int i,j;

    for (i = 0; i < ARRAY_SIZE(sensor_id_array_table); i++) {
        if ((strcmp(firmware->name, sensor_id_array_table[i].name) == 0)) {
            if(cts_dev->rtdata.is_chip_empty){
                cts_info("Chip is empty return default firmware bultin: chip_index:%d id:0x%02x id_index: %d ",
                i, sensor_id_array_table[i].id_tables[MAX_SUPPORT_ID_NUM].id, MAX_SUPPORT_ID_NUM);
                firmware->data = sensor_id_array_table[i].id_tables[MAX_SUPPORT_ID_NUM].array;
                firmware->size = sensor_id_array_table[i].id_tables[MAX_SUPPORT_ID_NUM].size;
                return 0;
            }
        for(j=0; j< ARRAY_SIZE(sensor_id_array_table[i].id_tables); j++){
            if(sensor_id_array_table[i].id_tables[j].id
                == cts_dev->confdata.hw_sensor_id){
                firmware->data = sensor_id_array_table[i].id_tables[j].array;
                firmware->size = sensor_id_array_table[i].id_tables[j].size;
                cts_info("Found %s firmware match sensor id bultin: chip_index:%d id: 0x%02x id_index: %d", 
                firmware->name, i, cts_dev->confdata.hw_sensor_id, j);
                return 0;
            }
        }
        }
    } 
    cts_info("Not found %s firmware match sensor id: 0x%02x bultin", 
    firmware->name,cts_dev->confdata.hw_sensor_id);
     return -EINVAL;
}
#endif

#define CTS_DEV_HW_MAX_GPIO_NUM             (8)
 
enum CTS_DEV_GPIO_STATUS {
    CTS_DEV_GPIO_PULL_DOWN = 0,
    CTS_DEV_GPIO_PULL_UP = 1,
    CTS_DEV_GPIO_FLOATING = 2,
};
 
static int cts_dev_hw_reg_set_bit(struct cts_device *cts_dev, u32 reg, int nr)
{
	int ret = 0; 
	u8 val = 0;
	
    ret = cts_hw_reg_readb(cts_dev, reg, &val);
    if (ret) {
       cts_err("set bit hw reg %d failed\n",reg);
       return ret;
    }
   
    val |= 1u << nr;
    ret = cts_hw_reg_writeb(cts_dev, reg, val);
    if (ret) {
       cts_err("set bit hw write reg %d val %d failed\n",reg,val);
       return ret;
    }
   
    return 0;
}
 
static int cts_dev_hw_reg_clr_bit(struct cts_device *cts_dev, u32 reg, int nr)
{
	int ret = 0;
	u8 val = 0;
	
    ret = cts_hw_reg_readb(cts_dev, reg, &val);
    if (ret) {
       cts_err("clr bit hw reg %d failed\n",reg);
       return ret;
    }
   
    val &= ~(1u << nr);
    ret = cts_hw_reg_writeb(cts_dev, reg, val);
    if (ret) {
       cts_err("clr bit hw write reg %d val %d failed\n",reg,val);
       return ret;
    }
   
    return 0;
}
 
static int cts_dev_set_gpio_direction(struct cts_device *cts_dev, int gpio, bool output)
{
    int ret = 0;
   
    cts_info("Set GPIO%u direction: %s", gpio, output ? "OUTPUT" : "INPUT");
 
    if (output) {
    	ret = cts_dev_hw_reg_set_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_DIR, gpio);
    } else {
    	ret = cts_dev_hw_reg_clr_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_DIR, gpio);
    }
    if (ret) {
       cts_err("set gpio direction failed\n");
       return ret;
    }
   
    return 0;
}
 
static int cts_dev_set_gpio_pullup(struct cts_device *cts_dev, int gpio)
{
    int ret = 0;
    cts_info("Set GPIO%u pull up", gpio);
 
    ret = cts_dev_hw_reg_set_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_PULL_UP, gpio);
    if (ret) {
       cts_err("set gpio pullup,set bit failed\n");
       return ret;
    }
    ret = cts_dev_hw_reg_clr_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_PULL_DOWN, gpio);
    if (ret) {
       cts_err("set gpio pullup,clr bit failed\n");
       return ret;
    }
    return 0;
}
 
static int cts_dev_set_gpio_pulldown(struct cts_device *cts_dev, int gpio)
{
	int ret = 0;
	
    cts_info("Set GPIO%u pull down", gpio);
    ret = cts_dev_hw_reg_clr_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_PULL_UP, gpio);
    if (ret) {
       cts_err("set gpio pulldown,clr bit failed\n");
       return ret;
    }
    ret = cts_dev_hw_reg_set_bit(cts_dev, CTS_DEVICE_HW_REG_GPIO_PULL_DOWN, gpio);
    if (ret) {
       cts_err("set gpio pulldown,set bit failed\n");
       return ret;
    }
    return 0;
}
 
static int cts_dev_get_gpio_value(struct cts_device *cts_dev, int gpio, bool *val)
{
    u8 v = 0;
    int ret = 0;
 
    ret = cts_hw_reg_readb(cts_dev, CTS_DEVICE_HW_REG_GPIO_GET_VAL, &v);
    if (ret) {
       cts_err("get gpio value hw reg failed\n");
       return ret;
    }
   
    *val = v & (1u << gpio) ? true : false;
   
    return 0;
}
 
static int cts_dev_get_gpio_status(struct cts_device *cts_dev, int gpio, u8 *status)
{
    int ret = 0;
    bool val = 0;
 
    cts_info("Get GPIO%u status");
   
    if (gpio < 0 || gpio > CTS_DEV_HW_MAX_GPIO_NUM) {
       cts_err("get gpio status gpio invalid\n");
       return -EINVAL;
    }
 
    ret = cts_dev_hw_reg_clr_bit(cts_dev, CTS_DEVICE_HW_REG_PINMUX, gpio);
    if (ret) {
       cts_err("get gpio status,he reg clr bit failed\n");
       return ret;
    }
    ret = cts_dev_set_gpio_direction(cts_dev, gpio, false);
    if (ret) {
       cts_err("get gpio status,set gpio direction failed\n");
       return ret;
    }
 
    ret = cts_dev_set_gpio_pullup(cts_dev, gpio);
    if (ret) {
       cts_err("get gpio status,set gpio pullup failed\n");
       return ret;
    }
    udelay(100);
 
    ret = cts_dev_get_gpio_value(cts_dev, gpio, &val);
    if (ret) {
       cts_err("get gpio status,get gpio value failed\n");
       return ret;
    }
    if (val) {
       ret = cts_dev_set_gpio_pulldown(cts_dev, gpio);
       if (ret) {
           cts_err("get gpio status,set gpio pulldown failed\n");
           return ret;
       }
       udelay(100);
 
       cts_dev_get_gpio_value(cts_dev, gpio, &val);
       if (ret) {
           cts_err("get gpio status,get gpio value failed\n");
           return ret;
       }
       if (val) {
           *status = CTS_DEV_GPIO_PULL_UP;
       } else {
           *status = CTS_DEV_GPIO_FLOATING;
       }
    } else {
       *status = CTS_DEV_GPIO_PULL_DOWN;
    }
   
    return 0;
}
 
int cts_dev_get_sensor_id(struct cts_device *cts_dev, u8 *sensor_id)
{
    int ret = 0;
    u8  id1, id2 = 0;
   
    ret = cts_dev_get_gpio_status(cts_dev, 2, &id1);
    if (ret) {
       cts_err("get gpio status id1 failed\n");
       return ret;
    }
   
    ret = cts_dev_get_gpio_status(cts_dev, 3, &id2);
    if (ret) {
       cts_err("get gpio status id2 failed\n");
       return ret;
    }
   
    *sensor_id = (id2 << 4) | id1;
    return 0;
}

#ifdef CFG_CTS_FIRMWARE_IN_FS
int cts_compare_sensor_id_from_fs(struct cts_device *cts_dev, 
        struct cts_firmware *firmware)
{
    int i,j = 0;
	int ret = 0;
	int retry=0;

	cts_info("%s", __func__);
	if(cts_dev->rtdata.is_chip_empty){
		for (retry = 0; retry < 3; retry++) {
			ret = cts_dev_get_sensor_id(cts_dev, &cts_dev->confdata.hw_sensor_id);
			if (ret == 0) {
				cts_info("Get hw sensor id: 0x%02x", cts_dev->confdata.hw_sensor_id);
				break;
			}
		}	
	}

    for (i = 0; i < ARRAY_SIZE(sensor_id_bin_table); i++) {
        if ((strcmp(firmware->name, sensor_id_bin_table[i].name) == 0)) {
            if(cts_dev->rtdata.is_chip_empty && retry >= 3){
                cts_dev->confdata.fw_name_index = i;
                cts_dev->confdata.fw_sensor_id_index = MAX_SUPPORT_ID_NUM;
                cts_info("Chip %s is empty return default firmware in fs: chip_index:%d id:0x%02x id_index: %d", 
                firmware->name, i, sensor_id_bin_table[i].id_tables[MAX_SUPPORT_ID_NUM].id,
                MAX_SUPPORT_ID_NUM);
                return 0;
            }

            for(j=0; j< ARRAY_SIZE(sensor_id_bin_table[i].id_tables); j++){
                if(sensor_id_bin_table[i].id_tables[j].id
                    == cts_dev->confdata.hw_sensor_id){
                    cts_dev->confdata.fw_name_index = i;
                    cts_dev->confdata.fw_sensor_id_index = j;

                    cts_info("Found %s firmware match sensor id in fs: chip_index:%d id:0x%02x id_index: %d", 
                    firmware->name, i, cts_dev->confdata.hw_sensor_id, j);
                    return 0;
                }
            }
        }
    } 
    cts_info("Not found %s firmware match sensor id: 0x%02x in fs", 
    firmware->name,cts_dev->confdata.hw_sensor_id);
     return -EINVAL;
}

#endif

#endif

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#ifdef CONFIG_CTS_SYSFS
int cts_get_chip_type_num_driver_builtin(void)
{
    return NUM_DRIVER_BUILTIN_FIRMWARE;
}

int cts_get_fw_num_driver_builtin(void)
{
#ifdef SUPPORT_SENSOR_ID
    return MAX_SUPPORT_ID_NUM+1;
#else
    return 1;
#endif
}

int cts_get_fw_sensor_id_driver_builtin(u32 chip_index, u32 fw_index)
{
#ifdef SUPPORT_SENSOR_ID
    return sensor_id_array_table[chip_index].id_tables[fw_index].id;
#else
    return 0xff;
#endif
}

int cts_get_fw_version_driver_builtin(const struct cts_firmware *firmware)
{
    return FIRMWARE_VERSION(firmware, firmware->ver_offset);
}
int cts_get_chip_type_index_driver_builtin(struct cts_device *cts_dev)
{
    int i;
    for(i=0; i< cts_get_chip_type_num_driver_builtin(); i++){
        if(strcmp(cts_dev->hwdata->name, cts_driver_builtin_firmwares[i].name) == 0){
            return i;
        }
    }
    return 0;
}

const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(const char *name)
{
    const struct cts_firmware *firmware;
    int i;

#if 1
    cts_info("Request driver builtin by name '%s'", name);

    firmware = cts_driver_builtin_firmwares;
    for (i = 0; i < NUM_DRIVER_BUILTIN_FIRMWARE; i++, firmware++) {
        if (strcmp(firmware->name, name) == 0) {
            if (is_firmware_valid(firmware)) {
                cts_info("Found driver builtin '%s' "
                        "hwid: %04x fwid: %04x size: %zu ver: %04x",
                    firmware->name, firmware->hwid, firmware->fwid,
                    firmware->size, FIRMWARE_VERSION(firmware, firmware->ver_offset));
                return firmware;
            }

            cts_warn("Found driver builtin '%s' "
                    "hwid: %04x fwid: %04x size: %zu invalid",
                firmware->name, firmware->hwid, firmware->hwid, firmware->size);
        }
    }

    return NULL;
#endif
}


const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
	struct cts_device *cts_dev, u32 chip_index, u32 fw_index)
{
    const struct cts_firmware *firmware;
    //int i;

    cts_info("Request driver builtin by chip index %u firmware index %u", chip_index,fw_index);

    if ((chip_index < NUM_DRIVER_BUILTIN_FIRMWARE)
        &&(fw_index < cts_get_fw_num_driver_builtin())){
        firmware = cts_driver_builtin_firmwares + chip_index;
#ifdef SUPPORT_SENSOR_ID
        memcpy(&cts_dev->confdata.firmware, firmware, sizeof(struct cts_firmware));
        cts_dev->confdata.firmware.data = sensor_id_array_table[chip_index].id_tables[fw_index].array;
        cts_dev->confdata.firmware.size = sensor_id_array_table[chip_index].id_tables[fw_index].size;
        firmware = &cts_dev->confdata.firmware;
#else

#endif
        if (is_firmware_valid(firmware)) {
            cts_info("Found driver builtin '%s' "
                    "hwid: %04x fwid: %04x size: %zu ver: %04x",
                firmware->name, firmware->hwid, firmware->fwid,
                firmware->size, FIRMWARE_VERSION(firmware, firmware->ver_offset));
            return firmware;
        }
        cts_warn("Found driver builtin '%s' "
                 "hwid: %04x fwid: %04x size: %zu INVALID",
            firmware->name, firmware->hwid, firmware->hwid, firmware->size);
    } else {
        cts_warn("Request driver builtin by chip_index %u too large >= %d"
            "or fw_index %u too large >= %d",
            chip_index, NUM_DRIVER_BUILTIN_FIRMWARE,
            fw_index,cts_get_fw_num_driver_builtin());
    }

    return NULL;

}
#endif /* CONFIG_CTS_SYSFS */

static struct cts_firmware * cts_request_newer_driver_builtin_firmware(
         struct cts_device *cts_dev, u16 hwid, u16 fwid, u16 device_fw_ver)
{
#define MATCH_HWID(firmware, hwid) \
    ((firmware)->hwid == (hwid))  //(hwid) == CTS_HWID_ANY ||
#define MATCH_FWID(firmware, fwid) \
    ((firmware)->fwid == (fwid)) //(fwid) == CTS_FWID_ANY || 

    struct cts_firmware *firmware = NULL;
    int    i = 0;
#ifdef SUPPORT_SENSOR_ID
	int ret = 0;
#endif

    cts_info("Request driver builtin if match hwid: %04x fwid: %04x && ver > %04x",
        hwid, fwid, device_fw_ver);

    firmware = cts_driver_builtin_firmwares;
    for (i = 0; i < ARRAY_SIZE(cts_driver_builtin_firmwares); i++, firmware++) {
        if (MATCH_HWID(firmware, hwid) || MATCH_FWID(firmware, fwid)) {

            memcpy(&cts_dev->confdata.firmware, firmware, sizeof(struct cts_firmware));

#ifdef SUPPORT_SENSOR_ID
            ret = cts_compare_sensor_id_from_bultin(cts_dev, &cts_dev->confdata.firmware);
            if(ret){
                cts_err("Not found match sensor id firmware, compare sensor id fail");
                goto No_newer_matched_fw;
            }
#endif

#if 1
            if (!is_firmware_valid(&cts_dev->confdata.firmware)) {
                cts_err("Found driver builtin '%s' "
                "hwid: %04x fwid: %04x INVALID ",
                firmware->name, firmware->hwid, firmware->fwid);
                continue;
            }

            firmware = &cts_dev->confdata.firmware;

            cts_info("Found match driver builtin '%s' "
            "hwid: %04x fwid: %04x size: %zu ver: %04x",
            firmware->name, firmware->hwid, firmware->fwid,
            firmware->size, FIRMWARE_VERSION(firmware, firmware->ver_offset));
#endif

#ifdef SUPPORT_SENSOR_ID
            if(1 == cts_dev->confdata.is_sensor_matched)
#else
            if(1)
#endif
            {
                if(FIRMWARE_VERSION(firmware, firmware->ver_offset) > device_fw_ver) {
                    cts_info("Found newer match driver builtin '%s' "
                    "hwid: %04x fwid: %04x size: %zu ver: %04x > %04x",
                    firmware->name, firmware->hwid, firmware->fwid,
                    firmware->size, FIRMWARE_VERSION(firmware, firmware->ver_offset), device_fw_ver);
                    return firmware;
                }else{
                    goto No_newer_matched_fw;
                }
            }else{
                if(cts_dev->rtdata.is_chip_empty){
                    cts_info("Chip is empty force update default firmware!!! ");
                }else{
                    cts_info("Firmware in chip is unmatch force update match sensor id firmware!!! ");
                }
                return firmware;
            }
        }
    }

No_newer_matched_fw:

    cts_info("No newer driver builtin found");
    return NULL;

#undef MATCH_HWID
#undef MATCH_FWID
}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
bool is_filesystem_mounted(const char *filepath)
{
    struct path root_path;
    struct path path;
    int ret = 0;

    ret = kern_path("/", LOOKUP_FOLLOW, &root_path);
    if (ret) {
        return false;
    }

    ret = kern_path(filepath, LOOKUP_FOLLOW, &path);
    if (ret) {
        goto err_put_root_path;
    }

    if (path.mnt->mnt_sb == root_path.mnt->mnt_sb) {
        /* not mounted */
        ret = false;
    } else {
        ret = true;
    }

    path_put(&path);
err_put_root_path:
    path_put(&root_path);

    return !!ret;
}

struct cts_firmware *cts_request_newer_firmware_from_fs(
        const struct cts_device *cts_dev, const char *filepath, u16 curr_version)
{
    struct cts_firmware *firmware;
    struct file *file;
    int ret, read_size = 0;
    u8 buff[2];
    u16 version = 0;
	loff_t pos = 0; 

    cts_info("Request from file '%s' if version > %04x",
        filepath, curr_version);

    firmware = (struct cts_firmware *)kzalloc(sizeof(*firmware), GFP_KERNEL);
    if (firmware == NULL) {
        cts_err("Request from file alloc struct firmware failed");
        return NULL;
    }

    firmware->name = cts_dev->hwdata->name;
    firmware->fwid = cts_dev->hwdata->fwid;
    firmware->hwid = cts_dev->hwdata->hwid;
    firmware->ver_offset = cts_dev->hwdata->ver_offset;

    file = filp_open(filepath, O_RDONLY, 0);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        goto err_free_firmware;
    }

    firmware->size = file_inode(file)->i_size;
    if (!is_firmware_size_valid(firmware)) {
        cts_info("File '%s' size: %zu invalid", filepath, firmware->size);
        goto err_close_file;
    }

	pos = FIRMWARE_VERSION_OFFSET;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	read_size = kernel_read(file, buff, 2, &pos);
#else
	read_size = kernel_read(file, pos, buff, 2);
#endif
	if (read_size < 0) {
		cts_err("Read version from offset 0x100 failed");
		goto err_close_file;
	}
    version = get_unaligned_le16(buff);

#ifdef SUPPORT_SENSOR_ID
    if(1 == cts_dev->confdata.is_sensor_matched){
        if (version <= curr_version) {
            cts_info("File '%s' size: %zu version: %04x <= %04x",
            filepath, firmware->size, version, curr_version);
            goto err_close_file;
        }
    }
#else
    if (version <= curr_version) {
        cts_info("File '%s' size: %zu version: %04x <= %04x",
            filepath, firmware->size, version, curr_version);
        goto err_close_file;
    }
#endif

    cts_info("File '%s' size: %zu version: %04x",
        filepath, firmware->size, version);

    firmware->data = (u8 *)kmalloc(firmware->size, GFP_KERNEL);
    if (firmware->data == NULL) {
        cts_err("Request form fs alloc firmware data failed");
        goto err_close_file;
    }
	
	pos = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	read_size = kernel_read(file, firmware->data, firmware->size, &pos);
#else
	read_size = kernel_read(file, pos, firmware->data, firmware->size);
#endif
	if (read_size < 0 || read_size != firmware->size) {
		cts_err("Request from fs read whole file failed %d", read_size);
		goto err_free_firmware_data;
	}

    ret = filp_close(file, NULL);
    if (ret) {
        cts_warn("Close file '%s' failed %d", filepath, ret);
    }

    firmware->is_fw_in_fs = true;
    return firmware;

err_free_firmware_data:
    kfree(firmware->data);
err_close_file:
    filp_close(file, NULL);
err_free_firmware:
    kfree(firmware);
    firmware = NULL;

    return NULL;
}

const struct cts_firmware *cts_request_firmware_from_fs(
        const struct cts_device *cts_dev, const char *filepath)
{
    cts_info("Request from file '%s'", filepath);

    return cts_request_newer_firmware_from_fs(cts_dev,filepath, 0);
}

int cts_update_firmware_from_file(struct cts_device *cts_dev,
        const char *filepath, bool to_flash)
{
    const struct cts_firmware *firmware;
    int ret = 0;

    cts_info("Update from file '%s' to %s", filepath,
        to_flash ? "flash" : "sram");

    firmware = cts_request_firmware_from_fs(cts_dev, filepath);
    if(firmware == NULL) {
        cts_err("Request from file '%s' failed", filepath);
        return -EFAULT;
    }

    ret = cts_update_firmware(cts_dev, firmware, to_flash);
    if (ret) {
        cts_err("Update to %s from file failed %d",
            to_flash ? "flash" : "sram", ret);
        goto err_release_firmware;
    }

    cts_info("Update from file success");

err_release_firmware:
    cts_release_firmware(firmware);

    return ret;

}
#endif /*CFG_CTS_FIRMWARE_IN_FS*/

struct cts_firmware *cts_request_firmware(
    struct cts_device *cts_dev,u16 hwid, u16 fwid, u16 curr_firmware_ver)
{
#ifdef CFG_CTS_FIRMWARE_IN_FS
    int i;
    const u8 *p=NULL;
#endif
    struct cts_firmware *firmware_builtin = NULL;
    struct cts_firmware *firmware_from_file = NULL;
#if (defined(CFG_CTS_FIRMWARE_IN_FS) && defined(SUPPORT_SENSOR_ID))
    int ret = 0;
#endif

#if 1
    if (hwid == CTS_HWID_INVALID) {
        hwid =  CTS_HWID_ANY;
    }
    if (fwid == CTS_FWID_INVALID) {
        fwid =  CTS_FWID_ANY;
    }

    cts_info("Request newer firmware if match hwid: %04x fwid: %04x && ver > %04x",
        hwid, fwid, curr_firmware_ver);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
    firmware_builtin = cts_request_newer_driver_builtin_firmware(
        cts_dev,hwid, fwid, curr_firmware_ver);
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_KERNEL_BUILTIN_FIRMWARE
    
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
    #ifdef SUPPORT_SENSOR_ID
    ret = cts_compare_sensor_id_from_fs(cts_dev, &cts_dev->confdata.firmware);
    if(ret){
        firmware_from_file = NULL;
    }else{
        p= sensor_id_bin_table[cts_dev->confdata.fw_name_index].id_tables[cts_dev->confdata.fw_sensor_id_index].bin;
        cts_info("chip index: %d bin file path index: %d",cts_dev->confdata.fw_name_index,
        cts_dev->confdata.fw_sensor_id_index);

        if (is_filesystem_mounted(p)){
            firmware_from_file = cts_request_newer_firmware_from_fs(
            cts_dev, p, firmware_builtin ? FIRMWARE_VERSION(firmware_builtin, firmware_builtin->ver_offset) :
            curr_firmware_ver);
        }
    }
    #else
    /* Check firmware in file system when probe only when build to .ko */

    for (i = 0; i < ARRAY_SIZE(bin_table); i++) {
        if (strcmp(cts_dev->hwdata->name, bin_table[i].name) == 0){
            p = bin_table[i].bin;
            cts_info("Request bin file from fs path index: %d",i);
            break;
        }
    }
    if(p){
        if (is_filesystem_mounted(p)) {
        firmware_from_file = cts_request_newer_firmware_from_fs(
        cts_dev,p,firmware_builtin ? 
        FIRMWARE_VERSION(firmware_builtin, firmware_builtin->ver_offset) :curr_firmware_ver);
        }
    }else{
        firmware_from_file = NULL;
    }
   
    #endif
#endif /* CFG_CTS_FIRMWARE_IN_FS */

    return firmware_from_file ? firmware_from_file : firmware_builtin;

#endif

}

void cts_release_firmware(const struct cts_firmware *firmware)
{
    cts_info("Release firmware");

     /* Builtin firmware with non-NULL name, no need to free*/
    if (firmware && firmware->is_fw_in_fs) {
        kfree(firmware->data);
        kfree(firmware);
    }
}

int cts_update_firmware(struct cts_device *cts_dev,
        const struct cts_firmware *firmware, bool to_flash)
{

    int ret, retries = 0;

    cts_info("Update firmware to %s ver: %04x size: %zu",
        to_flash ? "flash" : "sram",
        FIRMWARE_VERSION(firmware,firmware->ver_offset), firmware->size);

    cts_dev->rtdata.updating = true;

    retries = 0;

retry_upgrade:

    retries++;
    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Device enter program mode failed %d", ret);
        //return ret;
    }

    if(strcmp(firmware->name, "ICNT81xx") == 0){
        ret = icnt87xx_fw_update(cts_dev, firmware, 1); 
        if (ret) {
            cts_err("Update firmware failed %d ",ret);
        }
    }else if(strcmp(firmware->name, "ICNT87xx") == 0){
        ret = icnt87xx_fw_update(cts_dev, firmware, 0); 
        if (ret) {
            cts_err("Update firmware failed %d ",ret);
        }    
    }else if(strcmp(firmware->name, "ICNT82xx") == 0){


    }else if((strcmp(firmware->name, "ICNT85xx") == 0)
    ||(strcmp(firmware->name, "ICNT86xx") == 0)||(strcmp(firmware->name, "ICNT88xx") == 0)){
        ret = icnt85xx_fw_update(cts_dev, firmware, to_flash);
        if (ret) {
            cts_err("Update firmware failed %d ",ret);
        }
    }else if(strcmp(firmware->name, "ICNT89xx") == 0){
        ret = icnt89xx_fw_update(cts_dev, firmware, to_flash);
        if (ret) {
            cts_err("Update firmware failed %d ",ret);
        }
    }

    if(ret){
        if(retries < 3){
            cts_err("Update firmware retry: %d ",retries);
            goto retry_upgrade;
        }
    }
    
    ret = cts_enter_normal_mode(cts_dev);
    cts_dev->rtdata.updating = false;

#ifdef CONFIG_CTS_CHARGER_DETECT
	if (cts_is_charger_exist(cts_dev)) {
		cts_charger_plugin(cts_dev);
	}
#endif /* CONFIG_CTS_CHARGER_DETECT */
	
#ifdef CONFIG_CTS_GLOVE
	if (cts_is_glove_enabled(cts_dev)) {
		cts_enter_glove_mode(cts_dev);
	}	 
#endif

    return ret;
    
    //if (ret) {
    //    cts_err("Device enter normal mode failed %d", ret);
    //     goto out;
    // }
    
    //cts_dev->rtdata.i2c_addr        = CTS_NORMAL_MODE_I2CADDR;
    //cts_dev->rtdata.addr_width      = 2;
   // cts_dev->rtdata.program_mode    = false;
   // cts_dev->rtdata.updating = false;
   // return ret;

}

bool cts_is_firmware_updating(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.updating;
}


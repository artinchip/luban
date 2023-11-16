#ifndef CTS_FIRMWARE_H
#define CTS_FIRMWARE_H

#include "cts_config.h"

struct cts_firmware {
    char *name;   /* MUST set to non-NULL if driver builtin firmware */
    u16 hwid;
    u16 fwid;
    u8 *data;
    size_t size;
    u16 ver_offset;
    bool is_fw_in_fs;

};

struct cts_sfctrl {
    u32     reg_base;
    u32     xchg_sram_base;
    size_t  xchg_sram_size;

   // const struct cts_sfctrl_ops *ops;
};

struct cts_device;

extern struct cts_firmware *cts_request_firmware(
     struct cts_device *cts_dev,u16 hwid, u16 fwid, u16 device_fw_ver);
extern void cts_release_firmware(const struct cts_firmware *firmware);

#ifdef SUPPORT_SENSOR_ID
extern int cts_match_firmware_id(const struct cts_device *cts_dev, 
                    struct cts_firmware *firmware, u16 device_fw_ver);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
#define file_inode(file) ((file)->f_dentry->d_inode)
#endif

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
extern int cts_get_chip_type_num_driver_builtin(void);
extern int cts_get_fw_sensor_id_driver_builtin(u32 chip_index, u32 fw_index);
extern int cts_get_fw_version_driver_builtin(const struct cts_firmware *firmware);
extern int cts_get_fw_num_driver_builtin(void);
extern int cts_get_chip_type_index_driver_builtin(struct cts_device *cts_dev);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(const char *name);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
        struct cts_device *cts_dev, u32 chip_index, u32 index);
#else /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
static inline int cts_get_chip_type_num_driver_builtin(void) {return 0;}
static inline int cts_get_fw_sensor_id_driver_builtin(u32 chip_index, u32 fw_index){return 0xff;}
static inline int cts_get_fw_version_driver_builtin(const struct cts_firmware *firmware){return 0;}
static inline int cts_get_fw_num_driver_builtin(void) {return 0;}
static inline int cts_get_chip_type_index_driver_builtin(struct cts_device *cts_dev){return 0;}
static inline const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(const char *name) {return NULL;}
static inline const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
        struct cts_device *cts_dev, u32 chip_index, u32 index) {return NULL;}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
extern bool cts_is_firmware_updating(const struct cts_device *cts_dev);

extern int cts_update_firmware(struct cts_device *cts_dev,
        const struct cts_firmware *firmware, bool to_flash);

#ifdef CFG_CTS_FIRMWARE_IN_FS
extern const struct cts_firmware *cts_request_firmware_from_fs(const struct cts_device *cts_dev, const char *filepath);
extern int cts_update_firmware_from_file(
        struct cts_device *cts_dev, const char *filepath, bool to_flash);
#endif /* CFG_CTS_FIRMWARE_IN_FS */





#endif /* CTS_FIRMWARE_H */


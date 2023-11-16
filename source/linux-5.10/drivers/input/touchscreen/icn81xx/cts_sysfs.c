#define LOG_TAG         "Sysfs"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_test.h"
#include "cts_firmware.h"

#ifdef CONFIG_CTS_SYSFS
#define MAX_ARG_NUM                 (100)
#define MAX_ARG_LENGTH              (1024)
static char cmdline_param[MAX_ARG_LENGTH + 1];
static int  argc;
static char *argv[MAX_ARG_NUM];

static int parse_arg(const char *buf, size_t count)
{
    char *p;

    memcpy(cmdline_param, buf, min((size_t)MAX_ARG_LENGTH, count));
    cmdline_param[count] = '\0';

    argc = 0;
    p = strim(cmdline_param);
    if (p == NULL || p[0] == '\0') {
        return 0;
    }

    while (p && p[0] != '\0' && argc < MAX_ARG_NUM) {
        argv[argc++] = strsep(&p, " ,");
    }

    return argc;
}

/* echo addr value1 value2 value3 ... valueN > write_reg */
static ssize_t write_firmware_register_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 addr;
    int i, ret;
    u8 *data = NULL;

    parse_arg(buf, count);

    cts_info("Write firmware register '%.*s'", (int)count, buf);

    if (argc < 2) {
        cts_err("Too few args %d", argc);
        return -EFAULT;
    }

    ret = kstrtou16(argv[0], 0, &addr);
    if (ret) {
        cts_err("Invalid address %s", argv[0]);
        return -EINVAL;
    }

    data = (u8 *)kmalloc(argc - 1, GFP_KERNEL);
    if (data == NULL) {
        cts_err("Allocate buffer for write data failed\n");
        return -ENOMEM;
    }

    for (i = 1; i < argc; i++) {
        ret = kstrtou8(argv[i], 0, data + i - 1);
        if (ret) {
            cts_err("Invalid value %s", argv[i]);
            goto free_data;
        }
    }

    ret = cts_fw_reg_writesb(cts_dev, addr, data, argc - 1);
    if (ret) {
        cts_err("Write firmware register addr: 0x%04x size: %d failed",
            addr, argc - 1);
        goto free_data;
    }

free_data:
    kfree(data);

    return (ret < 0 ? ret : count);
}
static DEVICE_ATTR(write_reg, S_IWUSR, NULL, write_firmware_register_store);

static ssize_t read_firmware_register_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
#define PRINT_ROW_SIZE          (16)
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 addr, size, i, remaining;
    u8 *data = NULL;
    ssize_t count = 0;
    int ret = 0;

    cts_info("Read firmware register '%.*s'", (int)count, buf);

    if (argc != 2) {
        return sprintf(buf,
            "Invalid num args %d\n"
            "  1. echo addr size > read_reg\n"
            "  2. cat read_reg\n", argc);
    }

    ret = kstrtou16(argv[0], 0, &addr);
    if (ret) {
        return sprintf(buf, "Invalid address: %s\n", argv[0]);
    }
    ret = kstrtou16(argv[1], 0, &size);
    if (ret) {
        return sprintf(buf, "Invalid size: %s\n", argv[1]);
    }

    data = (u8 *)kmalloc(size, GFP_KERNEL);
    if (data == NULL) {
        return sprintf(buf, "Allocate buffer for read data failed\n");
    }

    cts_info("Read firmware register from 0x%04x size %u", addr, size);

    ret = cts_fw_reg_readsb(cts_dev, addr, data, (size_t)size);
    if (ret) {
        count = sprintf(buf,
            "Read firmware register from 0x%04x size %u failed %d\n",
            addr, size, ret);
        goto err_free_data;
    }

    remaining = size;
    for (i = 0; i < size && count <= PAGE_SIZE; i += PRINT_ROW_SIZE) {
        size_t linelen = min((size_t)remaining, (size_t)PRINT_ROW_SIZE);
        remaining -= PRINT_ROW_SIZE;

        count += snprintf(buf + count, PAGE_SIZE - count, "%04x: ", addr);

        /* Lower version kernel return void */
        hex_dump_to_buffer(data + i, linelen, PRINT_ROW_SIZE, 1,
                    buf + count, PAGE_SIZE - count, true);
        count += strlen(buf + count);

        if (count < PAGE_SIZE) {
            buf[count++] = '\n';
            addr += PRINT_ROW_SIZE;
        } else {
            break;
        }
    }

err_free_data:
    kfree(data);

    return count;
#undef PRINT_ROW_SIZE
}

/* echo addr size > read_reg */
static ssize_t read_firmware_register_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);

    return (argc == 0 ? 0 : count);
}
static DEVICE_ATTR(read_reg, S_IWUSR | S_IRUSR,
    read_firmware_register_show, read_firmware_register_store);

static ssize_t curr_firmware_info_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    int ret = 0;

    ret = sprintf(buf,
                  "Current firmware: \n"
                  "version:      0x%04x\n"
                  "Num rows:     %u\n"
                  "Num cols:     %u\n"
                  "X Resolution: %u\n"
                  "Y Resolution: %u\n"
                  "hw sensor id: 0x%02x\n"
                  "fw sensor id: 0x%02x\n"
                  "sensor id matched: 0x%02x\n",
                cts_data->cts_dev.fwdata.version,
                cts_data->cts_dev.fwdata.rows,
                cts_data->cts_dev.fwdata.cols,
                cts_data->cts_dev.fwdata.res_x,
                cts_data->cts_dev.fwdata.res_y,
                cts_data->cts_dev.confdata.hw_sensor_id,
                cts_data->cts_dev.confdata.fw_sensor_id,
                cts_data->cts_dev.confdata.is_sensor_matched
                );
    return ret;
}
static DEVICE_ATTR(curr_ver_info, S_IRUGO, curr_firmware_info_show, NULL);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
static ssize_t driver_builtin_firmware_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int i, chip_index, count = 0;
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;

    chip_index = cts_get_chip_type_index_driver_builtin(cts_dev);

    count += snprintf(buf + count, PAGE_SIZE - count,
            "Total %d builtin firmware:\n",
            cts_get_fw_num_driver_builtin());

    for (i = 0; i < cts_get_fw_num_driver_builtin(); i++) {
         const struct cts_firmware *firmware =
            cts_request_driver_builtin_firmware_by_index(cts_dev, chip_index, i);
        if (firmware) {
            count += snprintf(buf + count, PAGE_SIZE - count,
                        "%-2d: sensor_id:%02x hwid:%04x fwid:%04x size:%5zu ver:0x%04x desc: %s\n",
                        i, cts_get_fw_sensor_id_driver_builtin(chip_index, i),
                        firmware->hwid, firmware->fwid,
                        firmware->size, cts_get_fw_version_driver_builtin(firmware),
                        firmware->name);
         } else {
            count += snprintf(buf + count, PAGE_SIZE - count,
                        "%-2d: INVALID\n", i);
         }
    }

    return count;
}

/* echo index/name [flash/sram] > driver_builtin_firmware */
static ssize_t driver_builtin_firmware_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    const struct cts_firmware *firmware;
    bool to_flash = true;
    int ret, chip_index, fw_index = -1;

    parse_arg(buf, count); 
    if (argc != 1 && argc != 2) {
        cts_err("Invalid num args %d\n"
                "  echo index/name [flash/sram] > driver_builtin_firmware\n", argc);
        return -EFAULT;
    }
    if (isdigit(*argv[0])) {
        fw_index = simple_strtoul(argv[0], NULL, 0);
        cts_info("firmware index: %d",fw_index);
    }else {
        cts_err("Invalid firmware index %u", fw_index);
        return -EINVAL;
    }
    if (argc > 1) {
        if (strncasecmp(argv[1], "flash", 5) == 0) {
            to_flash = true;
        } else if (strncasecmp(argv[1], "sram", 4) == 0) {
            to_flash = false;
        } else {
            cts_err("Invalid location '%s', must be 'flash' or 'sram'", argv[1]);
            return -EINVAL;
        }
    }
    chip_index = cts_get_chip_type_index_driver_builtin(cts_dev);

    cts_info("Update driver builtin firmware '%s' to %s",
        argv[1], to_flash ? "flash" : "sram");
    if (fw_index >= 0 && fw_index < cts_get_fw_num_driver_builtin()) {
        firmware = cts_request_driver_builtin_firmware_by_index(cts_dev, chip_index, fw_index);
    } else {
        firmware = NULL;//cts_request_driver_builtin_firmware_by_name(buf);
        cts_err("Invalid firmware index %u",fw_index);
    }
    if (firmware) {
        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            return ret;
        }

        ret = cts_update_firmware(cts_dev, firmware, to_flash);
        if (ret) {
            cts_err("Update firmware failed %d", ret);
            goto err_start_device;
        }

        ret = cts_start_device(cts_dev);
        if (ret) {
            cts_err("Start device failed %d", ret);
            return ret;
        }
    } else {
        cts_err("Firmware '%s' NOT found", argv[0]);
        return -ENOENT;
    }

    return count;

err_start_device:
    cts_start_device(cts_dev);

    return ret;
}
static DEVICE_ATTR(driver_builtin_firmware, S_IWUSR | S_IRUGO,
        driver_builtin_firmware_show, driver_builtin_firmware_store);
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
/* echo filepath [flash/sram] > update_firmware_from_file */
static ssize_t update_firmware_from_file_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    const struct cts_firmware *firmware;
    bool to_flash = true;
    int ret = 0;
    // int index;

    parse_arg(buf, count);

    if (argc > 2) {
        cts_err("Invalid num args %d\n"
                       "  echo filepath [flash/sram] > update_from_file\n", argc);
        return -EFAULT;
    } else if (argc > 1) {
        if (strncasecmp(argv[1], "flash", 5) == 0) {
            to_flash = true;
        } else if (strncasecmp(argv[1], "sram", 4) == 0) {
            to_flash = false;
        } else {
            cts_err("Invalid location '%s', must be 'flash' or 'sram'", argv[1]);
            return -EINVAL;
        }
    }

    cts_info("Update firmware from file '%s'", argv[0]);
	do_gettimeofday(&start_tv);
	//cts_info("update start time>>>>>>>>>>>>> %ldS.%4ldms",start_tv.tv_sec,start_tv.tv_usec/1000);

    firmware = cts_request_firmware_from_fs(cts_dev, argv[0]);
    if (firmware) {
        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            return ret;
        }

        ret = cts_update_firmware(cts_dev, firmware, to_flash);
        if (ret) {
            cts_err("Update firmware failed %d", ret);
            //return ret;
        }

        ret = cts_start_device(cts_dev);
        if (ret) {
            cts_err("Start device failed %d", ret);
            return ret;
        }
		do_gettimeofday(&end_tv);
		//cts_info("update end time<<<<<<<<<<< %ldS.%4ldms",end_tv.tv_sec,end_tv.tv_usec/1000);
		cts_info(">>>>>> update usage time = %4ldms <<<<<<",end_tv.tv_sec*1000+end_tv.tv_usec/1000-start_tv.tv_sec*1000-start_tv.tv_usec/1000);

        cts_release_firmware(firmware);
    } else {
        cts_err("Request firmware from file '%s' failed", argv[0]);
        return -ENOENT;
    }

    return count;
}
static DEVICE_ATTR(update_from_file, S_IWUSR, NULL, update_firmware_from_file_store);
#endif /* CFG_CTS_FIRMWARE_IN_FS */

static ssize_t updating_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "Updating: %s\n",
        cts_data->cts_dev.rtdata.updating ? "Y" : "N");
}
static DEVICE_ATTR(updating, S_IRUGO, updating_show, NULL);

static struct attribute *cts_dev_firmware_atts[] = {
    &dev_attr_read_reg.attr,
    &dev_attr_write_reg.attr,
    &dev_attr_curr_ver_info.attr,
//	&dev_attr_curr_version.attr,
//	&dev_attr_rows.attr,
//    &dev_attr_cols.attr,
//	&dev_attr_res_x.attr,
//    &dev_attr_res_y.attr,
//    &dev_attr_esd_protection.attr,
//    &dev_attr_monitor_mode.attr,
//    &dev_attr_auto_compensate.attr,
#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
    &dev_attr_driver_builtin_firmware.attr,
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
#ifdef CFG_CTS_FIRMWARE_IN_FS
    &dev_attr_update_from_file.attr,
#endif /* CFG_CTS_FIRMWARE_IN_FS */
    &dev_attr_updating.attr,
    NULL
};

static const struct attribute_group cts_dev_firmware_attr_group = {
    .name  = "firmware",
    .attrs = cts_dev_firmware_atts,
};

#if 1
static ssize_t fw_version_test_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 version = 0;
    int ret = 0;

    if (argc != 1) {
		return sprintf(buf, "Invalid num args %d\n"
							"\n"
							"example:\n"
							"echo 0x1501 > fw_version_test\n"
							"cat fw_version_test\n", argc);
    }

    ret = kstrtou16(argv[0], 0, &version);
    if (ret) {
        return sprintf(buf, "Invalid version: %s\n", argv[0]);
    }

    cts_info("fw version test, version = 0x%x", version);

    ret = cts_fw_version_test(cts_dev, version);
    if (ret) {
        return sprintf(buf, "fw version test FAILED %d, version = 0x%x\n",
            ret, version);
    } else {
        return sprintf(buf, "fw version test PASSED, version = 0x%x\n",
            version);
    }
}

/* echo version > fw_version_test */
static ssize_t fw_version_test_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);
    
    return count;
}
static DEVICE_ATTR(fw_version_test, S_IWUSR | S_IRUGO, fw_version_test_show, fw_version_test_store);

static ssize_t rawdata_test_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 th_min = 0, th_max = 0;
    int ret = 0;

    if (argc != 2) {
        return sprintf(buf, "Invalid num args %d\n"
							"\n"
							"example:\n"
							"echo 1800 2200 > rawdata_test\n"
							"cat rawdata_test\n", argc);
    }

    ret = kstrtou16(argv[0], 0, &th_min);
    if (ret) {
        return sprintf(buf, "Invalid threshold_min: %s\n", argv[0]);
    }
    ret = kstrtou16(argv[1], 0, &th_max);
    if (ret) {
        return sprintf(buf, "Invalid threshold_max: %s\n", argv[1]);
    }
    
    cts_info("Rawdata test, threshold_min = %u threshold_max = %u", th_min, th_max);

    ret = cts_rawdata_test(cts_dev, th_min, th_max);
    if (ret) {
        return sprintf(buf, "Rawdata test FAILED %d, threshold_min = %u threshold_max = %u\n",
            ret, th_min, th_max);
    } else {
        return sprintf(buf, "Rawdata test PASSED, threshold = %u threshold_max = %u\n",
            th_min, th_max);
    }
}

/* echo th_min th_max > rawdata_test */
static ssize_t rawdata_test_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);
    
    return count;
}
static DEVICE_ATTR(rawdata_test, S_IWUSR | S_IRUGO, rawdata_test_show, rawdata_test_store);

static ssize_t open_test_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 threshold = 0;
    int ret = 0;

    if (argc != 1) {
		return sprintf(buf, "Invalid num args %d\n"
							"\n"
							"example:\n"
							"echo 800 > open_test\n"
							"cat open_test\n", argc);
    }

    ret = kstrtou16(argv[0], 0, &threshold);
    if (ret) {
        return sprintf(buf, "Invalid threshold: %s\n", argv[0]);
    }

    cts_info("Open test, threshold = %u", threshold);

    ret = cts_open_test(cts_dev, threshold);
    if (ret) {
        return sprintf(buf, "Open test FAILED %d, threshold = %u\n",
            ret, threshold);
    } else {
        return sprintf(buf, "Open test PASSED, threshold = %u\n",
            threshold);
    }
}

/* echo threshod > open_test */
static ssize_t open_test_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);
    
    return count;
}
static DEVICE_ATTR(open_test, S_IWUSR | S_IRUGO, open_test_show, open_test_store);

static ssize_t short_test_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 threshold = 0;
    int ret = 0;

    if (argc != 1) {
		return sprintf(buf, "Invalid num args %d\n"
							"\n"
							"example:\n"
							"echo 500 > short_test\n"
							"cat short_test\n", argc);
    }

    ret = kstrtou16(argv[0], 0, &threshold);
    if (ret) {
        return sprintf(buf, "Invalid threshold: %s\n", argv[0]);
    }

    cts_info("Short test, threshold = %u", threshold);

    ret = cts_short_test(cts_dev, threshold);
    if (ret) {
        return sprintf(buf, "Short test FAILED %d, threshold = %u\n",
            ret, threshold);
    } else {
        return sprintf(buf, "Short test PASSED, threshold = %u\n",
            threshold);
    }
}

/* echo threshod > short_test */
static ssize_t short_test_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);

    return count;
}
static DEVICE_ATTR(short_test, S_IWUSR | S_IRUGO,
        short_test_show, short_test_store);

static ssize_t testing_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "Testting: %s\n",
        cts_data->cts_dev.rtdata.testing ? "Y" : "N");
}
static DEVICE_ATTR(testing, S_IRUGO, testing_show, NULL);

static ssize_t cts_test_all_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    
#ifdef SUPPORT_TEST_CFG_FILE
    //const char *path = CTS_TEST_CFG_FILE_PATH;
    return cts_test(&cts_data->cts_dev, CTS_TEST_CFG_FILE_PATH);
#else
    return cts_test(&cts_data->cts_dev, NULL);
#endif

}
static DEVICE_ATTR(cts_test_all, S_IRUGO, cts_test_all_show, NULL);

#ifdef CFG_CTS_HAS_RESET_PIN
static ssize_t reset_test_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int ret;
    int count;

    ret = cts_reset_test(cts_dev);
    if (ret == 0) {
        count = sprintf(buf, "reset pin test sucessful\n");  
    }    
    else { 
        count = sprintf(buf, "reset pin test failed\n");  
    }          

    return count;
}
static DEVICE_ATTR(reset_test, S_IRUGO, reset_test_show, NULL);
#endif

static struct attribute *cts_dev_test_atts[] = {
    &dev_attr_fw_version_test.attr,
    &dev_attr_short_test.attr,
    &dev_attr_open_test.attr,
    &dev_attr_rawdata_test.attr,
    &dev_attr_cts_test_all.attr,
    &dev_attr_testing.attr,
#ifdef CFG_CTS_HAS_RESET_PIN
	&dev_attr_reset_test.attr,
#endif
    NULL
};

static const struct attribute_group cts_dev_test_attr_group = {
    .name  = "test",
    .attrs = cts_dev_test_atts,
};

#endif

static ssize_t ic_type_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "IC Type : %s\n",
        cts_data->cts_dev.hwdata->name);
}
static DEVICE_ATTR(ic_type, S_IRUGO, ic_type_show, NULL);

static ssize_t program_mode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "Program mode: %s\n",
        cts_data->cts_dev.rtdata.program_mode ? "Y" : "N");
}
static ssize_t program_mode_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num args %d", argc);
        return -EFAULT;
    }

    if (*argv[0] == '1' || tolower(*argv[0]) == 'y') {
        int ret = cts_enter_program_mode(&cts_data->cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            return ret;
        }
    }else{
        cts_err("Invalid arg value");
    }

    return count;
}
static DEVICE_ATTR(program_mode, S_IWUSR | S_IRUGO,
        program_mode_show, program_mode_store);

static ssize_t normal_mode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "Normal mode: %s\n",
        cts_data->cts_dev.rtdata.program_mode ? "N" : "Y");
}
static ssize_t normal_mode_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num args %d", argc);
        return -EFAULT;
    }

    if (*argv[0] == '1' || tolower(*argv[0]) == 'y') {
        int ret = cts_enter_normal_mode(&cts_data->cts_dev);
        if (ret) {
            cts_err("Enter normal mode failed %d", ret);
            return ret;
        }
    }else{
        cts_err("Invalid arg value");
    }

    return count;
}
static DEVICE_ATTR(normal_mode, S_IWUSR | S_IRUGO,
        normal_mode_show, normal_mode_store);

static ssize_t device_en_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "is device enabled: %s\n",
        cts_is_device_enabled(&cts_data->cts_dev) ? "Y" : "N");
}
static ssize_t device_en_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num args %d", argc);
        return -EFAULT;
    }

    if (*argv[0] == '1' || tolower(*argv[0]) == 'y') {
        int ret = cts_start_device(&cts_data->cts_dev);
        if (ret) {
            cts_err("start device fail %d", ret);
            return ret;
        }
    }else if(*argv[0] == '0' || tolower(*argv[0]) == 'n'){
        int ret = cts_stop_device(&cts_data->cts_dev);
        if (ret) {
            cts_err("stop device fail %d", ret);
            return ret;
        }
    }else{
        cts_err("Invalid arg value");
    }

    return count;
}
static DEVICE_ATTR(device_en, S_IWUSR | S_IRUGO,
        device_en_show, device_en_store);

#ifdef CONFIG_CTS_ESD_PROTECTION
    //cts_enable_esd_protection(cts_data);
static ssize_t esd_check_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return sprintf(buf, "esd check enabled: %s\n",
        cts_data->esd_enabled ? "Y" : "N");
}
static ssize_t esd_check_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num args %d", argc);
        return -EFAULT;
    }

    if (*argv[0] == '1' || tolower(*argv[0]) == 'y') {
        int ret = cts_enable_esd_protection(cts_data);
        if (ret) {
            cts_err("enable esd check fail %d", ret);
            return ret;
        }
    }else if(*argv[0] == '0' || tolower(*argv[0]) == 'n'){
        int ret = cts_disable_esd_protection(cts_data);
        if (ret) {
            cts_err("disable esd check fail %d", ret);
            return ret;
        }
    }else{
        cts_err("Invalid arg value");
    }

    return count;
}
static DEVICE_ATTR(esd_check, S_IWUSR | S_IRUGO,
        esd_check_show, esd_check_store);

#endif /* CONFIG_CTS_ESD_PROTECTION */

static ssize_t rawdata_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
#define RAWDATA_BUFFER_SIZE(cts_dev) \
    (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)
    
#define RAWDATA_SELF_CAP_BUFFER_SIZE(cts_dev) \
    ((cts_dev->fwdata.rows + cts_dev->fwdata.cols) * 2)    

    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 *rawdata = NULL, *self_cap = NULL;
    int ret, r, c, count = 0;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    bool data_valid = true;

    cts_info("Show rawdata");

    rawdata = (u16 *)kmalloc(RAWDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate memory for rawdata failed\n");
        return -ENOMEM;
    }
    
    self_cap = (u16 *)kmalloc(RAWDATA_SELF_CAP_BUFFER_SIZE(cts_dev), GFP_KERNEL);
    if (self_cap == NULL) {
        kfree(rawdata);
        cts_err("Allocate memory for rawdata failed\n");
        return -ENOMEM;
    }
    
    ret = cts_enable_get_rawdata(cts_dev);
    if (ret) {
        count += sprintf(buf, "Enable read raw data failed %d\n", ret);
        goto err_free_rawdata;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
    if (ret) {
        count += sprintf(buf, "Send cmd QUIT_GESTURE_MONITOR failed %d\n", ret);
        goto err_free_rawdata;
    }
    msleep(50);

    ret = cts_get_rawdata(cts_dev, rawdata);
    if(ret) {
        count += sprintf(buf, "Get raw data failed %d\n", ret);
        data_valid = false;
        // Fall through to disable get rawdata
    }    

    ret = cts_get_selfcap_rawdata(cts_dev, self_cap);
    if(ret) {
        count += sprintf(buf, "Get raw data failed %d\n", ret);
        data_valid = false;
        // Fall through to disable get rawdata
    }

    ret = cts_disable_get_rawdata(cts_dev);
    if (ret) {
        count += sprintf(buf, "Disable read raw data failed %d\n", ret);
        // Fall through to show rawdata
    }

    if (data_valid) {
#define SPLIT_LINE_STR \
                    "----------------------------------------------------------------------------------------------\n"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  " %2u  "
#define DATA_FORMAT_STR     "%4u "

        max = min = rawdata[0];
        sum = 0;
        max_r = max_c = min_r = min_c = 0;
        for (r = 0; r < cts_dev->fwdata.rows; r++) {
            for (c = 0; c < cts_dev->fwdata.cols; c++) {
                u16 val = rawdata[r * cts_dev->fwdata.cols + c];

                sum += val;
                if (val > max) {
                    max = val;
                    max_r = r;
                    max_c = c;
                } else if (val < min) {
                    min = val;
                    min_r = r;
                    min_c = c;
                }
            }
        }
        average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

        count += sprintf(buf + count,
            SPLIT_LINE_STR
            "Raw data MIN: [%d][%d]=%u, MAX: [%d][%d]=%u, AVG=%u\n"
            SPLIT_LINE_STR
            "   | ", min_r, min_c, min, max_r, max_c, max, average);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count += sprintf(buf + count, COL_NUM_FORMAT_STR, c);
        }
        count += sprintf(buf + count, "\n" SPLIT_LINE_STR);

        for (r = 0; r < cts_dev->fwdata.rows && count < PAGE_SIZE; r++) {
            count += sprintf(buf + count, ROW_NUM_FORMAT_STR, r);
            for (c = 0; c < cts_dev->fwdata.cols && count < PAGE_SIZE; c++) {
                count += snprintf(buf + count, PAGE_SIZE - count - 1,
                    DATA_FORMAT_STR, rawdata[r * cts_dev->fwdata.cols + c]);
            }
            buf[count++] = '\n';
        }
        count+= sprintf(buf + count, "\n\nself cap rawdata:\n");
        for (r = 0; 
            r < (cts_dev->fwdata.rows + cts_dev->fwdata.cols) 
                && count < PAGE_SIZE; 
            r++) {
                if (r == cts_dev->fwdata.cols) {
                    buf[count++] = '\n';
                }        
                count += snprintf(buf + count, PAGE_SIZE - count,
                    DATA_FORMAT_STR, self_cap[r]);
        }
        buf[count++] = '\n';
        
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
    }

err_free_rawdata:
    kfree(rawdata);
    kfree(self_cap);

    return (data_valid ? count : ret);

#undef RAWDATA_BUFFER_SIZE
#undef RAWDATA_SELF_CAP_BUFFER_SIZE
}
static DEVICE_ATTR(rawdata, S_IRUGO, rawdata_show, NULL);

static ssize_t diffdata_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
#define DIFFDATA_BUFFER_SIZE(cts_dev) \
        (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

#define RAWDATA_SELF_CAP_BUFFER_SIZE(cts_dev) \
    ((cts_dev->fwdata.rows + cts_dev->fwdata.cols) * 2)    

    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    s16 *diffdata = NULL, *self_cap = NULL;
    int ret, r, c, count = 0;
    int max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    bool data_valid = true;

    cts_info("Show diffdata");

    diffdata = (s16 *)kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
    if (diffdata == NULL) {
        cts_err("Allocate memory for diffdata failed");
        return -ENOMEM;
    }
    self_cap = (u16 *)kmalloc(RAWDATA_SELF_CAP_BUFFER_SIZE(cts_dev), GFP_KERNEL);
    if (self_cap == NULL) {
        kfree(diffdata);
        cts_err("Allocate memory for rawdata failed\n");
        return -ENOMEM;
    }

    ret = cts_enable_get_rawdata(cts_dev);
    if (ret) {
        cts_err("Enable read diff data failed %d", ret);
        goto err_free_diffdata;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
    if (ret) {
        cts_err("Send cmd QUIT_GESTURE_MONITOR failed %d", ret);
        goto err_free_diffdata;
    }
    msleep(50);

    ret = cts_get_diffdata(cts_dev, diffdata);
    if(ret) {
        cts_err("Get diff data failed %d", ret);
        data_valid = false;
        // Fall through to disable get diffdata
    }
    ret = cts_get_selfcap_diffdata(cts_dev, self_cap);
    if(ret) {
        count += sprintf(buf, "Get raw data failed %d\n", ret);
        data_valid = false;
        // Fall through to disable get rawdata
    }
    ret = cts_disable_get_rawdata(cts_dev);
    if (ret) {
        cts_err("Disable read diff data failed %d", ret);
        // Fall through to show diffdata
    }

    if (data_valid) {
#define SPLIT_LINE_STR \
                    "----------------------------------------------------------------------------------------------\n"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%4u "
#define DATA_FORMAT_STR     "%4d "

        max = min = diffdata[0];
        sum = 0;
        max_r = max_c = min_r = min_c = 0;
        for (r = 0; r < cts_dev->fwdata.rows; r++) {
            for (c = 0; c < cts_dev->fwdata.cols; c++) {
                s16 val = diffdata[r * cts_dev->fwdata.cols + c];

                sum += val;
                if (val > max) {
                    max = val;
                    max_r = r;
                    max_c = c;
                } else if (val < min) {
                    min = val;
                    min_r = r;
                    min_c = c;
                }
            }
        }
        average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

        count += sprintf(buf + count,
            SPLIT_LINE_STR
            "Diff data MIN: [%d][%d]=%d, MAX: [%d][%d]=%d, AVG=%d\n"
            SPLIT_LINE_STR
            "   | ", min_r, min_c, min, max_r, max_c, max, average);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count += sprintf(buf + count, COL_NUM_FORMAT_STR, c);
        }
        count += sprintf(buf + count, "\n" SPLIT_LINE_STR);

        for (r = 0; r < cts_dev->fwdata.rows; r++) {
            count += sprintf(buf + count, ROW_NUM_FORMAT_STR, r);
            for (c = 0; c < cts_dev->fwdata.cols; c++) {
                count += snprintf(buf + count, PAGE_SIZE - count,
                    DATA_FORMAT_STR, diffdata[r * cts_dev->fwdata.cols + c]);
           }
           buf[count++] = '\n';
        }
        count+= sprintf(buf + count, "\n\nself cap diffdata:\n");
        for (r = 0; 
            r < (cts_dev->fwdata.rows + cts_dev->fwdata.cols) 
                && count < PAGE_SIZE; 
            r++) {
                if (r == cts_dev->fwdata.cols) {
                    buf[count++] = '\n';
                }        
                count += snprintf(buf + count, PAGE_SIZE - count,
                    DATA_FORMAT_STR, self_cap[r]);
        }
        buf[count++] = '\n';
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
    }

err_free_diffdata:
    kfree(diffdata);

	return (data_valid ? count : ret);
#undef DIFFDATA_BUFFER_SIZE
#undef RAWDATA_SELF_CAP_BUFFER_SIZE
}
static DEVICE_ATTR(diffdata, S_IRUGO, diffdata_show, NULL);

#ifdef CFG_CTS_HAS_RESET_PIN
static ssize_t reset_pin_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    cts_info("Read RESET-PIN");

    return snprintf(buf, PAGE_SIZE,
        "Reset pin: %d, status: %d\n",
        cts_data->pdata->rst_gpio, 
        gpio_get_value(cts_data->pdata->rst_gpio));
}

static ssize_t reset_pin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;

    cts_info("Write RESET-PIN");
    cts_info("Chip staus maybe changed");

    cts_plat_set_reset(cts_dev->pdata, (buf[0] == '1') ? 1 : 0);
    return count;
}
static DEVICE_ATTR(reset_pin, S_IRUSR | S_IWUSR, reset_pin_show, reset_pin_store);
#endif        

static ssize_t irq_pin_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    cts_info("Read IRQ-PIN");

    return snprintf(buf, PAGE_SIZE,
        "IRQ pin: %d, status: %d\n",
        cts_data->pdata->int_gpio, 
        gpio_get_value(cts_data->pdata->int_gpio));
}
static DEVICE_ATTR(irq_pin, S_IRUGO, irq_pin_show, NULL);

static ssize_t irq_info_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct irq_desc *desc;

    cts_info("Read IRQ-INFO");

    desc = irq_to_desc(cts_data->pdata->irq);
    if (desc == NULL) {
        return snprintf(buf, PAGE_SIZE, "IRQ: %d descriptor not found\n",
            cts_data->pdata->irq);
    }

    return snprintf(buf, PAGE_SIZE,
        "IRQ num: %d, depth: %u, "
        "count: %u, unhandled: %u, last unhandled eslape: %lu\n",
        cts_data->pdata->irq, desc->depth, 
        desc->irq_count, desc->irqs_unhandled, desc->last_unhandled);
}
static DEVICE_ATTR(irq_info, S_IRUGO, irq_info_show, NULL);

static struct attribute *cts_dev_misc_atts[] = {
    &dev_attr_ic_type.attr,
    &dev_attr_device_en.attr,
    &dev_attr_program_mode.attr,
    &dev_attr_normal_mode.attr,
    &dev_attr_rawdata.attr,
    &dev_attr_diffdata.attr,
#ifdef CONFIG_CTS_ESD_PROTECTION
    &dev_attr_esd_check.attr,
#endif
#ifdef CFG_CTS_HAS_RESET_PIN    
	&dev_attr_reset_pin.attr,
#endif   
    &dev_attr_irq_pin.attr,
    &dev_attr_irq_info.attr,
    NULL
};

static const struct attribute_group cts_dev_misc_attr_group = {
    .name  = "misc",
    .attrs = cts_dev_misc_atts,
};

static const struct attribute_group *cts_dev_attr_groups[] = {
    &cts_dev_firmware_attr_group,
    //&cts_dev_flash_attr_group,
    &cts_dev_test_attr_group,
    &cts_dev_misc_attr_group,
    NULL
};

int cts_sysfs_add_device(struct device *dev)
{
    int ret = 0, i = 0;

    cts_info("Add device attr groups");

    // Low version kernel NOT support sysfs_create_groups()
    for (i = 0; cts_dev_attr_groups[i]; i++) {
        ret = sysfs_create_group(&dev->kobj, cts_dev_attr_groups[i]);
        if (ret) {
            while (--i >= 0) {
                sysfs_remove_group(&dev->kobj, cts_dev_attr_groups[i]);
            }
            break;
        }
    }

    if (ret) {
        cts_err("Add device attr failed %d", ret);
    }

    ret = sysfs_create_link(NULL, &dev->kobj, "chipone-ts");
    if(ret)
    {
        cts_err("Create sysfs link failed %d", ret);
    }

    return ret;
}

void cts_sysfs_remove_device(struct device *dev)
{
    int i;

    cts_info("Remove device attr groups");

    sysfs_remove_link(NULL, "chipone-ts");

    // Low version kernel NOT support sysfs_remove_groups()
    for (i = 0; cts_dev_attr_groups[i]; i++) {
        sysfs_remove_group(&dev->kobj, cts_dev_attr_groups[i]);
    }
}

#endif /* CONFIG_CTS_SYSFS */


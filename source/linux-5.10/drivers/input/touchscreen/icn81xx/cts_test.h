#ifndef CTS_TEST_H
#define CTS_TEST_H

struct cts_device;

#define PHYSICAL_MAX_NUM_ROW  27
#define PHYSICAL_MAX_NUM_COL   36

#define  RX_CHANNEL_NUM  36
#define  TX_CHANNEL_NUM  24

#define    SUPPORT_SAVE_TEST_LOG
#define    CTS_TEST_LOG_PATH                 "/etc/firmware/cts_test.log"
#define    SUPPORT_TEST_CFG_FILE
#ifdef  SUPPORT_TEST_CFG_FILE
#define    CTS_TEST_CFG_FILE_PATH        "/etc/firmware/cts_test_conf.cfg"
#endif
enum adc_data 
{
    CHANNEL_OFFSET = 0,
    INTERNAL_REFERENCE_RESISTOR,
    ODD_RX_TO_GROUND_SHORT_RESISTOR,
    EVEN_RX_TO_GROUND_SHORT_RESISTOR,
    ODD_TX_TO_GROUND_SHORT_RESISTOR,
    EVEN_TX_TO_GROUND_SHORT_RESISTOR,
};

struct tiny_short_config{
    u8 u8row_num;
    u8 u8col_num;
    u8 short_tx_num;
    u8 short_rx_num;
    u32 tiny_short_threshold;
    int tiny_short_resistor;
    u32 tiny_short_result;
    
    u8 rx_order[PHYSICAL_MAX_NUM_COL];
    u8 tx_order[PHYSICAL_MAX_NUM_ROW];
    
    u32 rx_data[PHYSICAL_MAX_NUM_COL];
    u32 tx_data[PHYSICAL_MAX_NUM_ROW];
    u8 rx_status[PHYSICAL_MAX_NUM_COL];
    u8 tx_status[PHYSICAL_MAX_NUM_ROW];

    u32 rx_logic_data[PHYSICAL_MAX_NUM_COL];
    u32 tx_logic_data[PHYSICAL_MAX_NUM_ROW];
    u8 rx_logic_status[PHYSICAL_MAX_NUM_COL];
    u8 tx_logic_status[PHYSICAL_MAX_NUM_ROW];
    
};

struct item_test_para{
    char *para1;
    char *para2;

    u16 threshold1;
    u16 threshold2;
};

struct cts_test_cfg{
    char *item_name;
    u8 need_test;
    u8 result;
        
    struct item_test_para  para;
    int (*cts_test_fun)(struct cts_device *cts_dev, u16 para1, u16 para2);
};

struct cts_log{
    u8 log_en;
    char *log_buf;
    u16 log_cnt;
};

extern int cts_fw_version_test( struct cts_device * cts_dev, u16 version);
extern int cts_rawdata_test(struct cts_device *cts_dev, u16 th_min, u16 th_max);
extern int cts_short_test(struct cts_device *cts_dev, u16 threshold);
extern int cts_open_test(struct cts_device *cts_dev, u16 th);
extern int cts_reset_test(struct cts_device *cts_dev);
extern int cts_test(struct cts_device *cts_dev,  const char *filepath);


#endif /* CTS_TEST_H */


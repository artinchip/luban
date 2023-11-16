#define LOG_TAG         "I2CDrv"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"

struct ctp_config_info config_info =
{
	.input_type = CTP_TYPE,
	//.name = NULL,
	.int_number = 0,
};


#define TS_WAKEUP_LOW_PERIOD (20)
#define TS_WAKEUP_HIGH_PERIOD (20)

static const unsigned short normal_i2c[2] = {0x48, I2C_CLIENT_END};

bool cts_show_debug_log = 0;
module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control");

static int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
      
    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
           return -ENODEV; 
	printk("twi_id %d adapter->nr %d\n",config_info.twi_id,adapter->nr);
	if(config_info.twi_id == adapter->nr)
	{
        strlcpy(info->type, CFG_CTS_DEVICE_NAME, I2C_NAME_SIZE);
        return 0;
	
	}
	else
		return -ENODEV;
}

static void ctp_print_info(struct ctp_config_info info)
{
	printk("info.ctp_used:%d\n",info.ctp_used);
	printk("info.twi_id:%d\n",info.twi_id);
	printk("info.screen_max_x:%d\n",info.screen_max_x);
	printk("info.screen_max_y:%d\n",info.screen_max_y);
	printk("info.revert_x_flag:%d\n",info.revert_x_flag);
	printk("info.revert_y_flag:%d\n",info.revert_y_flag);
	printk("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
	printk("info.irq_gpio_number:%d\n",info.irq_gpio);
	printk("info.wakeup_gpio_number:%d\n",info.wakeup_gpio);
}

static int ctp_get_system_config(void)
{   
    ctp_print_info(config_info);
   
    if((config_info.screen_max_x == 0) || (config_info.screen_max_y == 0))
	{
        printk("%s:read config error!\n",__func__);
        return -1;
    }
    return 0;
}

static void ctp_wakeup(void)
{	
	gpio_set_value(config_info.wakeup_gpio, 0);
	msleep(TS_WAKEUP_LOW_PERIOD);
	gpio_set_value(config_info.wakeup_gpio, 1);
	msleep(TS_WAKEUP_HIGH_PERIOD);

	return;
}

#if 0
static int cts_suspend(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Suspend");

    cts_lock_device(&cts_data->cts_dev);
    ret = cts_suspend_device(&cts_data->cts_dev);
    cts_unlock_device(&cts_data->cts_dev);

    if (ret) {
        cts_err("Suspend device failed %d", ret);
        // TODO:
        //return ret;
    }

    ret = cts_stop_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

#ifdef CONFIG_CTS_GESTURE
    /* Enable IRQ wake if gesture wakeup enabled */
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        ret = cts_plat_enable_irq_wake(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ wake failed %d", ret);
            return ret;
        }
        ret = cts_plat_enable_irq(cts_data->pdata);
        if (ret){
            cts_err("Enable IRQ failed %d",ret);
            return ret;
        }
    }
#endif /* CONFIG_CTS_GESTURE */

    /** - To avoid waking up while not sleeping,
            delay 20ms to ensure reliability */
    msleep(20);

    return 0;
}
static int cts_resume(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Resume");

#ifdef CONFIG_CTS_PROXIMITY


#endif/* CONFIG_CTS_PROXIMITY */

#ifdef CONFIG_CTS_GESTURE
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        ret = cts_plat_disable_irq_wake(cts_data->pdata);
        if (ret) {
            cts_warn("Disable IRQ wake failed %d", ret);
            //return ret;
        }
        if ((ret = cts_plat_disable_irq(cts_data->pdata)) < 0) {
            cts_err("Disable IRQ failed %d", ret);
            //return ret;
        }
    }
#endif /* CONFIG_CTS_GESTURE */

    ret = cts_resume_device(&cts_data->cts_dev);
    if(ret) {
        cts_warn("Resume device failed %d", ret);
        return ret;
    }

    ret = cts_start_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
        return ret;
    }

    return 0;
}
#endif

static int cts_i2c_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct chipone_ts_data *cts_data = NULL;
    int ret = 0;

	cts_info("Driver for platform %s",CFG_CTS_DRIVER_VERSION);
	
	cts_info("Probe i2c client: name='%s' addr=0x%02x flags=0x%02x irq=%d",
			client->name, client->addr, client->flags, client->irq);

#if !defined(CONFIG_MTK_PLATFORM)
    if (client->addr != CTS_NORMAL_MODE_I2CADDR) {
        cts_err("Probe i2c addr 0x%02x != driver config addr 0x%02x",
            client->addr, CTS_NORMAL_MODE_I2CADDR);
        return -ENODEV;
    };
#endif

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        cts_err("Check functionality failed");
        return -ENODEV;
    }

    cts_data = (struct chipone_ts_data *)kzalloc(sizeof(*cts_data), GFP_KERNEL);
    if (cts_data == NULL) {
        cts_err("Allocate chipone_ts_data failed");
        return -ENOMEM;
    }

    cts_data->pdata = (struct cts_platform_data *)kzalloc(
            sizeof(struct cts_platform_data), GFP_KERNEL);
    if (cts_data->pdata == NULL) {
        cts_err("Allocate cts_platform_data failed");
        ret = -ENOMEM;
        goto err_free_cts_data;
    }

    i2c_set_clientdata(client, cts_data);
    cts_data->i2c_client = client;

    ret = cts_init_platform_data(cts_data->pdata, client);
    if (ret < 0) {
        cts_err("Init platform data %d", ret);
        goto err_free_cts_data;
    }

    cts_data->cts_dev.pdata = cts_data->pdata;
    cts_data->pdata->cts_dev = &cts_data->cts_dev;

    cts_data->workqueue = create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-workqueue");
    if (cts_data->workqueue == NULL) {
        cts_err("Create workqueue failed");
        ret = -ENOMEM;
        goto err_deinit_pdata;
    }

    ret = cts_plat_reset_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Reset device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_probe_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Probe device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_plat_init_touch_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init touch device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_plat_init_vkey_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init vkey device failed %d", ret);
        goto err_deinit_touch_device;
    }
    
    ret = cts_plat_init_gesture(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init gesture failed %d", ret);
        goto err_deinit_vkey_device;
    }

    cts_init_esd_protection(cts_data);

#if 0
    ret = cts_tool_init(cts_data);
    if (ret < 0) {
        cts_warn("Init tool node failed %d", ret);
    }

    ret = cts_sysfs_add_device(&client->dev);
    if (ret < 0) {
        cts_warn("Add sysfs entry for device failed %d", ret);
    }
#endif
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    ret = cts_init_pm_fb_notifier(cts_data);
    if (ret) {
        cts_err("Init FB notifier failed %d", ret);
        goto err_deinit_sysfs;
    }   
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

    ret = cts_plat_request_irq(cts_data->pdata);
    if (ret < 0) {
        cts_err("Request IRQ failed %d", ret);
        goto err_register_fb;
    }
	
    ret = cts_start_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
        goto err_free_irq;
    }

    return 0;
    
err_free_irq:
    cts_plat_free_irq(cts_data->pdata);

err_register_fb:
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    cts_deinit_pm_fb_notifier(cts_data);
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */
//err_deinit_sysfs:
	cts_sysfs_remove_device(&client->dev);
#ifdef CONFIG_CTS_LEGACY_TOOL
    cts_tool_deinit(cts_data);
#endif /* CONFIG_CTS_LEGACY_TOOL */

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_deinit_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CONFIG_CTS_GESTURE
    cts_plat_deinit_gesture(cts_data->pdata);
#endif /* CONFIG_CTS_GESTURE */

err_deinit_vkey_device:
#ifdef CONFIG_CTS_VIRTUALKEY
    cts_plat_deinit_vkey_device(cts_data->pdata);
#endif /* CONFIG_CTS_VIRTUALKEY */

err_deinit_touch_device:
    cts_plat_deinit_touch_device(cts_data->pdata);

err_free_resource:
//    cts_plat_free_resource(cts_data->pdata);

//err_destroy_workqueue:
    destroy_workqueue(cts_data->workqueue);

err_deinit_pdata:
    cts_deinit_platform_data(cts_data->pdata);
    kfree(cts_data->pdata);

err_free_cts_data:
    kfree(cts_data);

    cts_err("Probe failed %d", ret);

    return ret;
}

static int cts_i2c_driver_remove(struct i2c_client *client)
{
    struct chipone_ts_data *cts_data;
    int ret = 0;

    cts_info("Remove");

    cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
    if (cts_data) {
        ret = cts_stop_device(&cts_data->cts_dev);
        if (ret) {
            cts_warn("Stop device failed %d", ret);
        }

        //input_free_device(cts_data->pdata->ts_input_dev);

        cts_plat_free_irq(cts_data->pdata);

#ifdef CONFIG_CTS_PM_FB_NOTIFIER       
        cts_deinit_pm_fb_notifier(cts_data);
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

        cts_tool_deinit(cts_data);

        cts_sysfs_remove_device(&client->dev);

        cts_deinit_esd_protection(cts_data);

        if (cts_data->pdata) {
            cts_plat_deinit_touch_device(cts_data->pdata);

            cts_plat_deinit_vkey_device(cts_data->pdata);

            cts_plat_deinit_gesture(cts_data->pdata);

 //           cts_plat_free_resource(cts_data->pdata);

            cts_deinit_platform_data(cts_data->pdata);

            kfree(cts_data->pdata);
        }

        if (cts_data->workqueue) {
            destroy_workqueue(cts_data->workqueue);
        }

        kfree(cts_data);
    }else {
        cts_warn("Chipone i2c driver remove while NULL chipone_ts_data");
        return -EINVAL;
    }

    return ret;
}

static const struct i2c_device_id cts_i2c_device_id_table[] = {
    {CFG_CTS_DEVICE_NAME, 0},
    {}
};

static struct i2c_driver cts_i2c_driver = {
	.class = I2C_CLASS_HWMON,
    .probe = cts_i2c_driver_probe,
    .remove = cts_i2c_driver_remove,
    .driver = {
        .name = CFG_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .id_table = cts_i2c_device_id_table,
    .address_list	= normal_i2c,
};

static int __init cts_i2c_driver_init(void)
{
	int ret = 0;
    cts_info("Init");

	if (input_sensor_startup(&(config_info.input_type))) 
	{		
		printk("%s: ctp_fetch_sysconfig_para err.\n", __func__);		
		return 0;	
	} 
	else 
	{		
		ret = input_sensor_init(&(config_info.input_type));		
		if (0 != ret) 
		{			
			printk("%s:ctp_ops.init_platform_resource err. \n", __func__);    		
		}	
	}	
	
	if(config_info.ctp_used == 0)		
	{	   	
		pr_err("*** ctp_used set to 0 !\n"); 			
		pr_err("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");			
		return 0;		
	}		

	if(ctp_get_system_config())
	{
		printk("%s:read config fail!\n",__func__);
		return ret;
	}

	ctp_wakeup();
	printk("ctp_wakeup \n");
	cts_i2c_driver.detect = ctp_detect;
	printk("ctp_detect \n");

    return i2c_add_driver(&cts_i2c_driver);
}

static void __exit cts_i2c_driver_exit(void)
{
    cts_info("Exit");

    i2c_del_driver(&cts_i2c_driver);
}

module_init(cts_i2c_driver_init);
module_exit(cts_i2c_driver_exit);

MODULE_DESCRIPTION("Chipone Touchscreen Driver for QualComm platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");


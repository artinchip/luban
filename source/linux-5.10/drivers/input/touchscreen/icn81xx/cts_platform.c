#define LOG_TAG         "Plat"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"


size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_I2C_XFER_SIZE;
}

u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata, 
    size_t xfer_size)
{
    return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

    struct i2c_msg msg = {
        .flags    = 0,
        .addr    = i2c_addr,
        .buf    = (u8 *)src,
        .len    = len,
       // .scl_rate   = 200000
    };

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
        if (ret != 1) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr    = i2c_addr,
            .flags    = 0,
            .buf    = (u8 *)wbuf,
            .len    = wlen,
            //.scl_rate   = 200000
        },
        {
            .addr    = i2c_addr,
            .flags    = I2C_M_RD,
            .buf    = (u8 *)rbuf,
            .len    = rlen,
          //  .scl_rate   = 200000
        }
    };

    if (wbuf == NULL || wlen == 0) {
        num_msg = 1;
    } else {
        num_msg = 2;
    }

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

        if (ret != num_msg) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_is_i2c_online(struct cts_platform_data *pdata, u8 i2c_addr)
{
	u8 dummy_bytes[2] = {0x00, 0x00};
	int ret = 0;

    ret = cts_plat_i2c_write(pdata, i2c_addr, dummy_bytes, sizeof(dummy_bytes), 5, 2);
    if (ret) {
        cts_err("!!! I2C addr 0x%02x is offline !!!", i2c_addr);
        return false;
    } else {
        cts_dbg("I2C addr 0x%02x is online", i2c_addr);
        return true;
    }
}

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
	int ret = 0;

    cts_dbg("Handle IRQ");

    cts_lock_device(pdata->cts_dev);
    ret = cts_irq_handler(pdata->cts_dev);
    if (ret) {
        cts_err("Device handle IRQ failed %d", ret);
    }
    cts_unlock_device(pdata->cts_dev);
}

static irqreturn_t cts_plat_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif /* CONFIG_GENERIC_HARDIRQS */

    cts_dbg("IRQ handler");

    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ handler with NULL dev_id");
        return IRQ_NONE;
    }

#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_irq(pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    cts_plat_disable_irq(pdata);

    queue_work(cts_data->workqueue, &pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

#ifndef CONFIG_GENERIC_HARDIRQS
static void cts_plat_touch_dev_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_irq(pdata);

    cts_plat_enable_irq(pdata);
}
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CFG_CTS_FORCE_UP
static void cts_plat_touch_event_timeout(struct timer_list *arg)
{
    cts_warn("Touch event timeout");

    cts_plat_release_all_touch((struct cts_platform_data *)arg);
}
#endif

int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
{
	struct input_dev *input_dev;
	int ret = 0;

    cts_info("Init");

	pdata->int_gpio = config_info.irq_gpio;
	pdata->irq = gpio_to_irq(pdata->int_gpio);
	pdata->rst_gpio = config_info.wakeup_gpio;

    pdata->i2c_client = i2c_client;
    rt_mutex_init(&pdata->dev_lock);

    pdata->i2c_client->irq = pdata->irq;
    spin_lock_init(&pdata->irq_lock);

    input_dev = input_allocate_device();
    if (input_dev == NULL) {
        cts_err("Failed to allocate input device.");
        return -ENOMEM;
    }

    /** - Init input device */
    input_dev->name = CFG_CTS_DEVICE_NAME;
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &pdata->i2c_client->dev;

	input_dev->evbit[0] =   BIT_MASK(EV_SYN) |
		BIT_MASK(EV_KEY) |
		BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT,input_dev->propbit);//touch deviceType

	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	
	input_set_abs_params(input_dev, ABS_X, 0, config_info.screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, config_info.screen_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0 , 0);

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

    input_set_drvdata(input_dev, pdata);

    ret = input_register_device(input_dev);
    if (ret) {
        cts_err("Failed to register input device");
        return ret;
    }

    pdata->ts_input_dev = input_dev;

#if !defined(CONFIG_GENERIC_HARDIRQS)
    INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_VIRTUALKEY
    {
        u8 vkey_keymap[CFG_CTS_NUM_VKEY] = CFG_CTS_VKEY_KEYCODES;
        memcpy(pdata->vkey_keycodes, vkey_keymap, sizeof(vkey_keymap));
        pdata->vkey_num = CFG_CTS_NUM_VKEY;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CONFIG_CTS_GESTURE
    {
        u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;
        memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
        pdata->gesture_num = CFG_CTS_NUM_GESTURE;
    }
#endif /* CONFIG_CTS_GESTURE */

#ifdef CFG_CTS_FORCE_UP
	//setup_timer(&pdata->touch_event_timeout_timer, 
	timer_setup(&pdata->touch_event_timeout_timer, 
	cts_plat_touch_event_timeout, (unsigned long)pdata);
#endif

    return 0;
}

int cts_deinit_platform_data(struct cts_platform_data *pdata)
{
    cts_info("De-Init");
    if (pdata->ts_input_dev) {
        input_unregister_device(pdata->ts_input_dev);
        pdata->ts_input_dev = NULL;
    }
    return 0;
}

int cts_plat_request_irq(struct cts_platform_data *pdata)
{
	int ret = 0;

    cts_info("Request IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    /* Note:
     * If IRQ request succeed, IRQ will be enbled !!!
     */
    ret = request_threaded_irq(pdata->irq,
            NULL, cts_plat_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
            pdata->i2c_client->dev.driver->name, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq,
            cts_plat_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
            pdata->i2c_client->dev.driver->name, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

void cts_plat_free_irq(struct cts_platform_data *pdata)
{
    free_irq(pdata->irq, pdata);
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Enable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (pdata->irq_is_disable)/* && !cts_is_device_suspended(pdata->chip)) */{  
            cts_dbg("Real enable IRQ");
            enable_irq(pdata->irq);
            pdata->irq_is_disable = false;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

int cts_plat_disable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Disable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (!pdata->irq_is_disable) {
            cts_dbg("Real disable IRQ");
            disable_irq_nosync(pdata->irq);
            pdata->irq_is_disable = true;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_reset_device(struct cts_platform_data *pdata)
{
	struct cts_device *cts_dev = pdata->cts_dev;

	cts_info("Reset device");

	gpio_set_value(pdata->rst_gpio, 0);
	msleep(1);
	gpio_set_value(pdata->rst_gpio, 1);
	msleep(50);

	cts_get_program_i2c_addr(cts_dev);
	return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    cts_info("Set Reset,val=%d",val);
    if (val) {
        gpio_set_value(pdata->rst_gpio, 1);
    }
    else {
        gpio_set_value(pdata->rst_gpio, 0);
    }
    return 0;            
}
#endif

int cts_plat_power_up_device(struct cts_platform_data *pdata)
{
    cts_info("Power up device");

    return 0;
}

int cts_plat_power_down_device(struct cts_platform_data *pdata)
{
    cts_info("Power down device");

    return 0;
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
    cts_info("Init touch device");

    return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
    cts_info("De-init touch device");

    if (pdata->ts_input_dev) {
#ifndef CONFIG_GENERIC_HARDIRQS
        if (work_pending(&pdata->ts_irq_work)) {
            cancel_work_sync(&pdata->ts_irq_work);
        }

        input_free_device(pdata->ts_input_dev);
        pdata->ts_input_dev = NULL;
#endif /* CONFIG_GENERIC_HARDIRQS */
    }
}

int cts_plat_process_touch_msg(struct cts_platform_data *pdata,
            struct cts_device_touch_msg *msgs, int num)
{
    struct input_dev *input_dev = pdata->ts_input_dev;
    int i;
    int contact = 0;

    cts_dbg("Process touch %d msgs", num);

    for (i = 0; i < num; i++) {
        u16 x, y;

        x = le16_to_cpu(msgs[i].x);
        y = le16_to_cpu(msgs[i].y);

#if 0
#ifdef CFG_CTS_SWAP_XY
        swap(x,y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(pdata->res_x,x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(pdata->res_y,y);
#endif /* CFG_CTS_WRAP_Y */
#endif
	if(config_info.exchange_x_y_flag)
		swap(x,y);
	
	if(config_info.revert_x_flag)
		x = config_info.screen_max_x - x;
	
	if(config_info.revert_y_flag)
		y = config_info.screen_max_y - y;

        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
//		printk("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u\n",
//            i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);

#ifdef CONFIG_CTS_SLOTPROTOCOL
        input_mt_slot(input_dev, msgs[i].id);
        switch (msgs[i].event) {
            case CTS_DEVICE_TOUCH_EVENT_DOWN:
            case CTS_DEVICE_TOUCH_EVENT_MOVE:
            case CTS_DEVICE_TOUCH_EVENT_STAY:
                contact++;
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
                input_report_abs(input_dev, ABS_MT_POSITION_X, x);
                input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
                input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
                input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
                break;

            case CTS_DEVICE_TOUCH_EVENT_UP:
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                break;

            default:
                cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
                break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
        /**
         * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
         * in addition to the ABS_MT events, the last SYN_MT_REPORT event
         * may be omitted. Otherwise, the last SYN_REPORT will be dropped
         * by the input core, resulting in no zero-contact event
         * reaching userland.
         */
        switch (msgs[i].event) {
            case CTS_DEVICE_TOUCH_EVENT_DOWN:
            case CTS_DEVICE_TOUCH_EVENT_MOVE:
            case CTS_DEVICE_TOUCH_EVENT_STAY:
                contact++;
			#if 0
				input_report_abs(input_dev, ABS_MT_TRACKING_ID, msgs[i].id);
                input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
                input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
				input_report_key(input_dev, BTN_TOUCH, 1);
                input_report_abs(input_dev, ABS_MT_POSITION_X, x);
                input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
                input_mt_sync(input_dev);
			#endif
				input_report_abs(input_dev, ABS_X, x);
				input_report_abs(input_dev, ABS_Y, y);
				
				input_report_key(input_dev, BTN_TOUCH, 1);
                break;

            case CTS_DEVICE_TOUCH_EVENT_UP:
				break;
            default:
                cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
                break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
	input_report_key(input_dev, BTN_TOUCH, contact > 0);
	input_mt_sync_frame(input_dev);
#else
	if (contact == 0) {
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_mt_sync(input_dev);
	}	 
#endif
    input_sync(input_dev);
	
#ifdef CFG_CTS_FORCE_UP
	if (contact) {
		mod_timer(&pdata->touch_event_timeout_timer, jiffies + msecs_to_jiffies(100));
	} else {
		del_timer(&pdata->touch_event_timeout_timer);
	}
#endif

    return 0;
}

int cts_plat_release_all_touch(struct cts_platform_data *pdata)
{
    struct input_dev *input_dev = pdata->ts_input_dev;

#if defined(CONFIG_CTS_SLOTPROTOCOL)
    int id;
#endif /* CONFIG_CTS_SLOTPROTOCOL */

    cts_info("Release all touch");

#ifdef CONFIG_CTS_SLOTPROTOCOL
	for (id = 0; id < CFG_CTS_MAX_TOUCH_NUM; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		input_mt_sync(input_dev);
	}
    input_mt_sync_frame(input_dev);
#else /* CONFIG_CTS_SLOTPROTOCOL */
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_mt_sync(input_dev);
#endif /* CONFIG_CTS_SLOTPROTOCOL */

    input_sync(input_dev);

#ifdef CFG_CTS_FORCE_UP
	del_timer(&pdata->touch_event_timeout_timer);
#endif

    return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init VKey");

    pdata->vkey_state = 0;

    for (i = 0; i <  pdata->vkey_num; i++) {
        input_set_capability(pdata->ts_input_dev,
            EV_KEY, pdata->vkey_keycodes[i]);
    }

    return 0;
}

void cts_plat_deinit_vkey_device(struct cts_platform_data *pdata)
{
    cts_info("De-init VKey");

    pdata->vkey_state = 0;
}

int cts_plat_process_vkey(struct cts_platform_data *pdata, u8 vkey_state)
{
    u8  event;
    int i;

    event = pdata->vkey_state ^ vkey_state;

    cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

    for (i = 0; i < pdata->vkey_num; i++) {
        input_report_key(pdata->ts_input_dev,
                        pdata->vkey_keycodes[i], vkey_state & BIT(i) ? 1 : 0);
	input_sync(pdata->ts_input_dev);
    }

    pdata->vkey_state = vkey_state;

    return 0;
}

int cts_plat_release_all_vkey(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Release all vkeys");

    for (i = 0; i < pdata->vkey_num; i++) {
        if (pdata->vkey_state & BIT(i)) {
            input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i], 0);
        }
    }

    pdata->vkey_state = 0;

    return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CONFIG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
    cts_info("Enable IRQ wake");

    if (pdata->irq > 0) {
        if (!pdata->irq_wake_enabled) {
            pdata->irq_wake_enabled = true;
            return enable_irq_wake(pdata->irq);
        }

        cts_warn("Enable irq wake while already disabled");
        return -EINVAL;
    }

    cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
    cts_info("Disable IRQ wake");

    if (pdata->irq > 0) {
        if (pdata->irq_wake_enabled) {
            pdata->irq_wake_enabled = false;
            return disable_irq_wake(pdata->irq);
        }

        cts_warn("Disable irq wake while already disabled");
        return -EINVAL;
    }

    cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init gesture");

    // TODO: If system will issure enable/disable command, comment following line.
    //cts_enable_gesture_wakeup(pdata->cts_dev);

    for (i = 0; i < pdata->gesture_num; i ++) {
        input_set_capability(pdata->ts_input_dev, EV_KEY,
            pdata->gesture_keymap[i][1]);
    }

    return 0;
}

void cts_plat_deinit_gesture(struct cts_platform_data *pdata)
{
    cts_info("De-init gesture");
}

int cts_plat_process_gesture_info(struct cts_platform_data *pdata,
    struct cts_device_gesture_info *gesture_info)
{
    int i;

    cts_info("Process gesture, id=0x%02x", gesture_info->gesture_id);

#if defined(CFG_CTS_GESTURE_REPORT_KEY)
    for (i = 0; i < CFG_CTS_NUM_GESTURE; i++) {
        if (gesture_info->gesture_id == pdata->gesture_keymap[i][0]) {
            cts_info("Report key[%u]", pdata->gesture_keymap[i][1]);
            input_report_key(pdata->ts_input_dev,
                pdata->gesture_keymap[i][1], 1);
            input_sync(pdata->ts_input_dev);

            input_report_key(pdata->ts_input_dev,
                pdata->gesture_keymap[i][1], 0);
            input_sync(pdata->ts_input_dev);

            return 0;
        }
    }
#endif /* CFG_CTS_GESTURE_REPORT_KEY */

    cts_warn("Process unrecognized gesture id=%u",
        gesture_info->gesture_id);

    return -EINVAL;
}

#endif /* CONFIG_CTS_GESTURE */


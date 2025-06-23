#include <linux/gpio.h>
#include "asr_defs.h"
#include "asr_platform.h"
#include "asr_sdio.h"
#include "asr_pm.h"

#ifdef CONFIG_ASR_PM
extern void asr_notify_active(struct asr_hw *asr_hw);
extern void asr_notify_suspend(struct asr_hw *asr_hw);

int asr_pm_power_on(struct asr_plat *asr_plat)
{
	//TODO:
	return ENOTSUPP;
}

int asr_pm_power_off(struct asr_plat *asr_plat)
{
	//TODO:
	return ENOTSUPP;
}

int asr_pm_reset(struct asr_plat *asr_plat)
{
	//TODO:
	return ENOTSUPP;
}

static void enable_sdio_rescan(struct asr_plat *plat)
{
	struct mmc_host *host = plat->func->card->host;

	asr_dbg(TRACE, "rescan_disable: %d\n", plat->rescan_disable);
	if (!plat->rescan_disable) {
		host->rescan_disable = 0;
		queue_delayed_work(system_freezable_wq, &host->detect, HZ);
	}
}

static void disable_sdio_rescan(struct asr_plat *plat)
{
	struct mmc_host *host = plat->func->card->host;

	plat->rescan_disable = host->rescan_disable;
	asr_dbg(TRACE, "rescan_disable: %d\n", plat->rescan_disable);
	if (!plat->rescan_disable) {
		host->rescan_disable = 1;
		cancel_delayed_work(&host->detect);
	}
}

static int pm_suspend_mod(struct asr_plat *plat)
{
	int state = asr_sdio_get_state(plat);

	if (plat->pm_cnt) {
		asr_dbg(TRACE, "pm is acquired %d\n", plat->pm_cnt);
		return state;
	}

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	if (plat->async_resume) {
		asr_dbg(TRACE, "pm is async resuming %d\n", plat->async_resume);
		return state;
	}
#endif

	asr_dbg(TRACE, "state: %d\n", state);
	if (state == SDIO_STATE_RESUMING || state == SDIO_STATE_ACTIVE) {
		/* Disable the card detection of mmc host */
		disable_sdio_rescan(plat);
#ifdef CONFIG_GPIO_WAKEUP_MOD
		/* Set pm out gpio to low to suspend card */
		gpio_set_value(plat->pm_out_gpio, PM_LOW_LEVEL);
#endif /* CONFIG_GPIO_WAKEUP_MOD */

#ifdef CONFIG_GPIO_WAKEUP_HOST
		asr_sdio_set_state(plat, SDIO_STATE_SUSPENDING);
		/* Schedule polling to check the module state */
		mod_delayed_work(plat->pm_workq, &plat->polling_work, POLLING_INTERVAL);
#else
		asr_sdio_set_state(plat, SDIO_STATE_SUSPENDED);
		//TODO: notify suspended
#endif /* CONFIG_GPIO_WAKEUP_HOST */
	}

	return asr_sdio_get_state(plat);
}

static int pm_resume_mod(struct asr_plat *plat)
{
	int state = asr_sdio_get_state(plat);

	asr_dbg(TRACE, "state: %d\n", state);
	if (state == SDIO_STATE_SUSPENDING || state == SDIO_STATE_SUSPENDED) {
#ifdef CONFIG_GPIO_WAKEUP_MOD
		/* Set out gpio to low to suspend card */
		gpio_set_value(plat->pm_out_gpio, PM_HIGH_LEVEL);
#endif /* CONFIG_GPIO_WAKEUP_MOD */

		asr_sdio_set_state(plat, SDIO_STATE_RESUMING);
		/* Schedule polling timer to check the module state */
		mod_delayed_work(plat->pm_workq, &plat->polling_work, POLLING_INTERVAL);
	}

	return asr_sdio_get_state(plat);
}

int asr_pm_suspend(struct asr_plat *plat)
{
	int state;

	spin_lock_bh(&plat->pm_lock);
	state = pm_suspend_mod(plat);
	spin_unlock_bh(&plat->pm_lock);

	return state;
}

int asr_pm_resume(struct asr_plat *plat)
{
	int state;

	spin_lock_bh(&plat->pm_lock);
	state = pm_resume_mod(plat);
	spin_unlock_bh(&plat->pm_lock);

	return state;
}

int asr_pm_acquire(struct asr_plat *plat, unsigned int flag)
{
	int state;

	spin_lock_bh(&plat->pm_lock);
	/* Increment pm count, pm will check this count before suspend module */
	plat->pm_cnt++;

	state = asr_sdio_get_state(plat);
#if defined(CONFIG_GPIO_WAKEUP_HOST) && defined(CONFIG_ASR5505)
	/**
	 * When the light sleep of asr5505 is waking up, the SDIO pins will be
	 * pulled down, which will trigger the SDIO interrupt request of host
	 * driver. For firmware, if the host driver needs to receive data, it
	 * will first set the GPIO to active level and then trigger the SDIO
	 * interrupt. So if the state of host driver is suspended, the request
	 * is from ISR and the level of in GPIO is not active level, indicate
	 * there is no data to be received by host driver and the driver will
	 * ignore this request.
	 */
	if (flag & PM_FROMISR && state == SDIO_STATE_SUSPENDED &&
		gpio_get_value(plat->pm_in_gpio) == !plat->active_level) {
		spin_unlock_bh(&plat->pm_lock);
		return state;
	}
#endif

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	cancel_delayed_work(&plat->auto_suspend);

	if (state == SDIO_STATE_SUSPENDED || state == SDIO_STATE_SUSPENDING)
		state = pm_resume_mod(plat);

	if (state == SDIO_STATE_RESUMING && flag & PM_NONBLOCK)
		plat->async_resume = true;
#endif
	spin_unlock_bh(&plat->pm_lock);

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	if (state == SDIO_STATE_RESUMING && !(flag & PM_NONBLOCK)) {
		int retry = 100;

		while (retry) {
			wait_event_timeout(plat->pm_waitq,
								asr_sdio_get_state(plat) == SDIO_STATE_ACTIVE,
								msecs_to_jiffies(POLLING_INTERVAL));

			state = asr_sdio_get_state(plat);
			if (state != SDIO_STATE_RESUMING)
				break;

			retry--;
		}

		if (retry == 0)
			asr_err("wait to resume timeout\n");
	}
#endif

	return state;
}

int asr_pm_release(struct asr_plat *plat)
{
	int state;

	spin_lock_bh(&plat->pm_lock);
	/* Increment pm count, if pm_cnt is 0 print error log */
	if (plat->pm_cnt)
		plat->pm_cnt--;
	else
		asr_err("pm_cnt is 0\n");

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	if (!plat->pm_cnt && !plat->async_resume)
		mod_delayed_work(plat->pm_workq, &plat->auto_suspend, plat->suspend_delay);
#endif

	state = asr_sdio_get_state(plat);
	spin_unlock_bh(&plat->pm_lock);

	return state;
}

#ifdef CONFIG_AUTO_SUSPEND_RESUME
static void auto_suspend_func(struct work_struct *work)
{
	struct asr_plat *plat = container_of(work, struct asr_plat, auto_suspend.work);
	if (plat->asr_hw->ps_on)
		asr_pm_suspend(plat);
}
#endif

static void polling_work_func(struct work_struct *work)
{
	struct asr_plat *plat = container_of(work, struct asr_plat, polling_work.work);
	bool repoll = true;
	int state, old_state;
#ifdef CONFIG_GPIO_WAKEUP_HOST
	int level;
#else
	struct sdio_func *func = plat->func;
	int err;
	u8 val;
#endif

#ifdef CONFIG_GPIO_WAKEUP_HOST
	spin_lock_bh(&plat->pm_lock);

	old_state = asr_sdio_get_state(plat);
	level = gpio_get_value(plat->pm_in_gpio);
	switch (old_state) {
		case SDIO_STATE_SUSPENDING:
			if (level == !plat->active_level) {
				repoll = false;
				asr_sdio_set_state(plat, SDIO_STATE_SUSPENDED);
			}
			break;

		case SDIO_STATE_RESUMING:
			if (level == plat->active_level) {
				repoll = false;
				asr_sdio_set_state(plat, SDIO_STATE_ACTIVE);
			}
			break;

		case SDIO_STATE_SUSPENDED:
			repoll = false;
			if (level == plat->active_level) {
#ifdef CONFIG_GPIO_WAKEUP_MOD
				/* Set out gpio to high to resume module */
				gpio_set_value(plat->pm_out_gpio, PM_HIGH_LEVEL);
#endif /* CONFIG_GPIO_WAKEUP_MOD */
				asr_sdio_set_state(plat, SDIO_STATE_ACTIVE);
			}
			break;

		default:
			asr_dbg(TRACE, "polling error module state %d\n", old_state);
			repoll = false;
			break;
	}
#else
	if (asr_sdio_get_state(plat) == SDIO_STATE_RESUMING) {
		sdio_claim_host(func);
		val = sdio_readb(func, C2H_INTEVENT, &err);
		sdio_release_host(func);
	}

	spin_lock_bh(&plat->pm_lock);

	old_state = asr_sdio_get_state(plat);
	if (old_state == SDIO_STATE_RESUMING) {
		if (err) {
			asr_err("read sdio error: %d\n", err);
		} else {
			repoll = false;
			asr_sdio_set_state(plat, SDIO_STATE_ACTIVE);
		}
	} else {
		asr_dbg(TRACE, "polling error module state %d\n", old_state);
		repoll = false;
	}
#endif /* CONFIG_GPIO_WAKEUP_HOST */

	state = asr_sdio_get_state(plat);
	if (state != old_state) {
		if (state == SDIO_STATE_ACTIVE) {
			asr_dbg(TRACE, "module is active\n");
			/* Enable the card detection of mmc host */
			enable_sdio_rescan(plat);

#ifdef CONFIG_AUTO_SUSPEND_RESUME
			if (plat->async_resume)
				plat->async_resume = false;

			/**
			 * Queue auto suspend work to prevent the driver from not suspending
			 * due to not calling asr_pm_release
			 */
			if (!plat->pm_cnt)
				mod_delayed_work(plat->pm_workq, &plat->auto_suspend, plat->suspend_delay);

			/* Wake up waiting for the module to be active */
			if (waitqueue_active(&plat->pm_waitq))
				wake_up(&plat->pm_waitq);
#endif /* CONFIG_AUTO_SUSPEND_RESUME */

			/* Notify the module state is active */
			asr_notify_active(plat->asr_hw);
		} else if (state == SDIO_STATE_SUSPENDED) {
			asr_dbg(TRACE, "module is suspended\n");
			/* Notify the module state is suspended */
			asr_notify_suspend(plat->asr_hw);
		}
	}

	if (repoll) {
		plat->polling_cnt++;
		//todo: check the polling count
		if (plat->polling_cnt % 100 == 0)
			asr_dbg(TRACE, "repoll the card state %d\n", state);

		mod_delayed_work(plat->pm_workq, &plat->polling_work, POLLING_INTERVAL);
	} else {
		plat->polling_cnt = 0;
	}

	spin_unlock_bh(&plat->pm_lock);
}

#ifdef CONFIG_GPIO_WAKEUP_HOST
static irqreturn_t asr_pm_gpio_isr(int irq, void *dev_id)
{
	struct asr_plat *plat = dev_id;

	asr_dbg(TRACE, "state: %d\n", asr_sdio_get_state(plat));

	if (plat && plat->pm_workq)
		mod_delayed_work(plat->pm_workq, &plat->polling_work, 0);

	return IRQ_HANDLED;
}
#endif

static void pm_cmd_work_func(struct work_struct *work)
{
	struct asr_plat *plat = container_of(work, struct asr_plat, pm_cmd_work.work);
	struct asr_hw *asr_hw = plat->asr_hw;
	char *cmd;

	if (!asr_hw)
		return;

	cmd = asr_hw->mod_params->pm_cmd;
	if (strlen(cmd)) {
		asr_dbg(TRACE, "cmd: %s, state: %d\n", cmd, asr_sdio_get_state(plat));

		if (strncmp(cmd, "suspend", strlen("suspend")) == 0)
			asr_pm_suspend(plat);
		else if (strncmp(cmd, "resume", strlen("resume")) == 0)
			asr_pm_resume(plat);
		else if (strncmp(cmd, "isr_acquire", strlen("isr_acquire")) == 0)
			asr_pm_acquire(plat, PM_NONBLOCK | PM_FROMISR);
		else if (strncmp(cmd, "nb_acquire", strlen("nb_acquire")) == 0)
			asr_pm_acquire(plat, PM_NONBLOCK);
		else if (strncmp(cmd, "acquire", strlen("acquire")) == 0)
			asr_pm_acquire(plat, 0);
		else if (strncmp(cmd, "release", strlen("release")) == 0)
			asr_pm_release(plat);
#ifdef CONFIG_GPIO_WAKEUP_MOD
		else if (strncmp(cmd, "get_out_level", strlen("get_out_level")) == 0)
			asr_err("get_out_level: %d\n", gpio_get_value(plat->pm_out_gpio));
#endif /* CONFIG_GPIO_WAKEUP_MOD */
#ifdef CONFIG_GPIO_WAKEUP_HOST
		else if (strncmp(cmd, "get_in_level", strlen("get_in_level")) == 0)
			asr_err("get_in_level: %d\n", gpio_get_value(plat->pm_in_gpio));
#endif /* CONFIG_GPIO_WAKEUP_HOST */
#ifdef CONFIG_AUTO_SUSPEND_RESUME
		else if (strncmp(cmd, "spnd_delay", strlen("spnd_delay")) == 0) {
			char *pmsecs = strchr(cmd, '=');
			if (pmsecs) {
				pmsecs++; //point to delay time
				plat->suspend_delay = simple_strtoul(pmsecs, NULL, 0);
				asr_dbg(TRACE, "auto suspend delay %lums\n", plat->suspend_delay);
				plat->suspend_delay = msecs_to_jiffies(plat->suspend_delay);
			}
		}
#endif /* CONFIG_AUTO_SUSPEND_RESUME */

		asr_dbg(TRACE, "module state: %d\n", asr_sdio_get_state(plat));
		memset(cmd, 0x00, 20);
	}

	schedule_delayed_work(&plat->pm_cmd_work,
		msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));
}

int asr_pm_init(struct asr_hw *asr_hw)
{
	int ret = 0;
	struct asr_plat *plat = asr_hw->plat;
#ifdef CONFIG_GPIO_WAKEUP_HOST
	unsigned long flag;
#endif

	spin_lock_init(&plat->pm_lock);

	plat->pm_workq = create_singlethread_workqueue("pm");
	if (!plat->pm_workq) {
		asr_err("alloc workqueue failed\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&plat->polling_work, polling_work_func);

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	plat->suspend_delay = AUTO_SUSPEND_DELAY;

	INIT_DELAYED_WORK(&plat->auto_suspend, auto_suspend_func);
	queue_delayed_work(plat->pm_workq, &plat->auto_suspend, plat->suspend_delay);

	init_waitqueue_head(&plat->pm_waitq);
#endif

#ifdef CONFIG_GPIO_WAKEUP_MOD
	plat->pm_out_gpio = asr_hw->mod_params->pm_out_gpio;
	/* Initialize pm out pin to suspend/resume card */
	ret = gpio_request(plat->pm_out_gpio, "pm_out");
	if (ret) {
		asr_err("gpio_request %d error: %d\n", plat->pm_out_gpio, ret);
		return ret;
	}

	/* Default set the pm out pin to high to resume card */
	ret = gpio_direction_output(plat->pm_out_gpio, PM_HIGH_LEVEL);
	if (ret) {
		asr_err("gpio_direction_input %d error: %d\n", plat->pm_out_gpio, ret);
		return ret;
	}
#endif /* CONFIG_GPIO_WAKEUP_MOD */

#ifdef CONFIG_GPIO_WAKEUP_HOST
	plat->pm_in_gpio = asr_hw->mod_params->pm_in_gpio;
	/* Initialize pm input pin to wakeup host and indicate the suspend/resume status of card */
	ret = gpio_request(plat->pm_in_gpio, "pm_in");
	if (ret) {
		asr_err("gpio_request %d error: %d\n", plat->pm_in_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(plat->pm_in_gpio);
	if (ret) {
		asr_err("gpio_direction_input %d error: %d\n", plat->pm_in_gpio, ret);
		return ret;
	}

	/* Get the active GPIO level of the module */
	plat->active_level = CONFIG_MOD_ACTIVE_LEVEL;
	gpio_set_value(plat->pm_in_gpio, plat->active_level);

	plat->pm_irq = gpio_to_irq(plat->pm_in_gpio);
	if (plat->pm_irq < 0) {
		asr_err("gpio_to_irq %d error: %d\n", plat->pm_in_gpio, ret);
		return plat->pm_irq;
	}

	flag = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_SHARED | IRQF_ONESHOT;
	ret = request_irq(plat->pm_irq, asr_pm_gpio_isr, flag, "asr_pm", plat);
	if (ret) {
		asr_err("request_irq error: %d\n", ret);
		return ret;
	}

	/**
	 * Get the level of the in GPIO to update the card state. If the
	 * levle is high indicate the card is active, else indicate the
	 * card is suspended. At this stage, the card should be active.
	 * So if the level is low, print an error log here.
	 */
	if (gpio_get_value(plat->pm_in_gpio) == plat->active_level) {
		/* PM in gpio is low level indicate the card is avtive */
		asr_sdio_set_state(plat, SDIO_STATE_ACTIVE);
	} else {
		asr_err("The in GPIO level is not active %d:%d, please check\n",
			gpio_get_value(plat->pm_in_gpio), plat->active_level);
		asr_sdio_set_state(plat, SDIO_STATE_SUSPENDED);
	}
#else
	asr_sdio_set_state(plat, SDIO_STATE_ACTIVE);
#endif /* CONFIG_GPIO_WAKEUP_HOST */

	/* Initialize delayed work for pm cmd */
	INIT_DELAYED_WORK(&plat->pm_cmd_work, pm_cmd_work_func);
	schedule_delayed_work(&plat->pm_cmd_work,
		msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));

	return ret;
}

void asr_pm_deinit(struct asr_hw *asr_hw)
{
	struct asr_plat *plat = asr_hw->plat;

	/* Enable the card detection of mmc host */
	enable_sdio_rescan(plat);

#ifdef CONFIG_GPIO_WAKEUP_MOD
	gpio_free(plat->pm_out_gpio);
#endif /* CONFIG_GPIO_WAKEUP_MOD */

#ifdef CONFIG_GPIO_WAKEUP_HOST
	free_irq(plat->pm_irq, plat);
	gpio_free(plat->pm_in_gpio);
#endif /* CONFIG_GPIO_WAKEUP_HOST */

	cancel_delayed_work_sync(&plat->polling_work);

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	cancel_delayed_work_sync(&plat->auto_suspend);

	asr_sdio_set_state(plat, SDIO_STATE_POWEROFF);
	if (waitqueue_active(&plat->pm_waitq))
		wake_up(&plat->pm_waitq);
#endif

	if (plat->pm_workq) {
		destroy_workqueue(plat->pm_workq);
		plat->pm_workq = NULL;
	}

	cancel_delayed_work_sync(&plat->pm_cmd_work);
}
#endif /* CONFIG_ASR_PM */

#ifndef _ASR_PM_H_
#define _ASR_PM_H_

#define PM_NONBLOCK             0x00000001
#define PM_FROMISR              0x00000002

#define AUTO_SUSPEND_DELAY      msecs_to_jiffies(200)
#define POLLING_INTERVAL        msecs_to_jiffies(10)

int asr_pm_power_on(struct asr_plat *plat);

int asr_pm_power_off(struct asr_plat *plat);

int asr_pm_reset(struct asr_plat *plat);

int asr_pm_suspend(struct asr_plat *plat);

int asr_pm_resume(struct asr_plat *plat);

int asr_pm_init(struct asr_hw *asr_hw);

void asr_pm_deinit(struct asr_hw *asr_hw);

int asr_pm_acquire(struct asr_plat *plat, unsigned int flag);

int asr_pm_release(struct asr_plat *plat);

#endif

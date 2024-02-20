/**
 ******************************************************************************
 *
 * @file asr_debugfs.h
 *
 * @brief Miscellaneous utility function definitions
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_DEBUGFS_H_
#define _ASR_DEBUGFS_H_

#include <linux/workqueue.h>
#include <linux/if_ether.h>
#include <linux/version.h>

struct asr_hw;
struct asr_sta;

/* some macros taken from iwlwifi */
/* TODO: replace with generic read and fill read buffer in open to avoid double
 * reads */
#define DEBUGFS_ADD_FILE(name, parent, mode) do {               \
    if (!debugfs_create_file(#name, mode, parent, asr_hw,      \
                &asr_dbgfs_##name##_ops))                      \
    goto err;                                                   \
} while (0)

#define DEBUGFS_ADD_BOOL(name, parent, ptr) do {                \
    struct dentry *__tmp;                                       \
    __tmp = debugfs_create_bool(#name, S_IWUSR | S_IRUSR,       \
            parent, ptr);                                       \
    if (IS_ERR(__tmp) || !__tmp)                                \
    goto err;                                                   \
} while (0)

#if 0
#define DEBUGFS_ADD_X64(name, parent, ptr) do {                 \
    struct dentry *__tmp;                                       \
    __tmp = debugfs_create_x64(#name, S_IWUSR | S_IRUSR,        \
            parent, ptr);                                       \
    if (IS_ERR(__tmp) || !__tmp)                                \
    goto err;                                                   \
} while (0)

#define DEBUGFS_ADD_U64(name, parent, ptr, mode) do {           \
    struct dentry *__tmp;                                       \
    __tmp = debugfs_create_u64(#name, mode,                     \
            parent, ptr);                                       \
    if (IS_ERR(__tmp) || !__tmp)                                \
    goto err;                                                   \
} while (0)

#define DEBUGFS_ADD_X32(name, parent, ptr) do {                 \
    struct dentry *__tmp;                                       \
    __tmp = debugfs_create_x32(#name, S_IWUSR | S_IRUSR,        \
            parent, ptr);                                       \
    if (IS_ERR(__tmp) || !__tmp)                                \
    goto err;                                                   \
} while (0)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define DEBUGFS_ADD_U32(name, parent, ptr, mode) do {           \
    debugfs_create_u32(#name, mode,                     \
            parent, ptr);                                       \
} while (0)

#else
#define DEBUGFS_ADD_U32(name, parent, ptr, mode) do {           \
    struct dentry *__tmp;                                       \
    __tmp = debugfs_create_u32(#name, mode,                     \
            parent, ptr);                                       \
    if (IS_ERR(__tmp) || !__tmp)                                \
    goto err;                                                   \
} while (0)
#endif
/* file operation */
#define DEBUGFS_READ_FUNC(name)                                         \
    static ssize_t asr_dbgfs_##name##_read(struct file *file,          \
                                            char __user *user_buf,      \
                                            size_t count, loff_t *ppos);

#define DEBUGFS_WRITE_FUNC(name)                                         \
    static ssize_t asr_dbgfs_##name##_write(struct file *file,          \
                                             const char __user *user_buf,\
                                             size_t count, loff_t *ppos);

#define DEBUGFS_OPEN_FUNC(name)                              \
    static int asr_dbgfs_##name##_open(struct inode *inode, \
                                        struct file *file);

#define DEBUGFS_RELEASE_FUNC(name)                              \
    static int asr_dbgfs_##name##_release(struct inode *inode, \
                                           struct file *file);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 2, 0)
#define DEBUGFS_READ_FILE_OPS(name)                             \
    DEBUGFS_READ_FUNC(name);                                    \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .read   = asr_dbgfs_##name##_read,                         \
    .open   = generic_file_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_WRITE_FILE_OPS(name)                            \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .write  = asr_dbgfs_##name##_write,                        \
    .open   = generic_file_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_READ_WRITE_FILE_OPS(name)                       \
    DEBUGFS_READ_FUNC(name);                                    \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .write  = asr_dbgfs_##name##_write,                        \
    .read   = asr_dbgfs_##name##_read,                         \
    .open   = generic_file_open,                                \
    .llseek = generic_file_llseek,                              \
};
#else
#define DEBUGFS_READ_FILE_OPS(name)                             \
    DEBUGFS_READ_FUNC(name);                                    \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .read   = asr_dbgfs_##name##_read,                         \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_WRITE_FILE_OPS(name)                            \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .write  = asr_dbgfs_##name##_write,                        \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_READ_WRITE_FILE_OPS(name)                       \
    DEBUGFS_READ_FUNC(name);                                    \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations asr_dbgfs_##name##_ops = { \
    .write  = asr_dbgfs_##name##_write,                        \
    .read   = asr_dbgfs_##name##_read,                         \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};
#endif

#define DEBUGFS_READ_WRITE_OPEN_RELEASE_FILE_OPS(name)              \
    DEBUGFS_READ_FUNC(name);                                        \
    DEBUGFS_WRITE_FUNC(name);                                       \
    DEBUGFS_OPEN_FUNC(name);                                        \
    DEBUGFS_RELEASE_FUNC(name);                                     \
static const struct file_operations asr_dbgfs_##name##_ops = {     \
    .write   = asr_dbgfs_##name##_write,                           \
    .read    = asr_dbgfs_##name##_read,                            \
    .open    = asr_dbgfs_##name##_open,                            \
    .release = asr_dbgfs_##name##_release,                         \
    .llseek  = generic_file_llseek,                                 \
};

#ifdef CONFIG_ASR_DEBUGFS

struct asr_debugfs {
	unsigned long long rateidx;
	struct dentry *dir;
	bool trace_prst;

	char helper_cmd[64];
	struct work_struct helper_work;
	bool helper_scheduled;
	spinlock_t umh_lock;
	bool unregistering;

	//struct asr_fw_trace fw_trace;

	struct work_struct rc_stat_work;
	uint8_t rc_sta[NX_REMOTE_STA_MAX];
	uint8_t rc_write;
	uint8_t rc_read;
	struct dentry *dir_rc;
	struct dentry *dir_sta[NX_REMOTE_STA_MAX];
	int rc_config[NX_REMOTE_STA_MAX];
	struct list_head rc_config_save;
};

// Max duration in msecs to save rate config for a sta after disconnection
#define RC_CONFIG_DUR 600000

struct asr_rc_config_save {
	struct list_head list;
	unsigned long timestamp;
	int rate;
	u8 mac_addr[ETH_ALEN];
};

int asr_dbgfs_register(struct asr_hw *asr_hw, const char *name);
void asr_dbgfs_unregister(struct asr_hw *asr_hw);
//int asr_um_helper(struct asr_debugfs *asr_debugfs, const char *cmd);
//int asr_trigger_um_helper(struct asr_debugfs *asr_debugfs);
void asr_dbgfs_register_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta);
void asr_dbgfs_unregister_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta);

//int asr_dbgfs_register_fw_dump(struct asr_hw *asr_hw,struct dentry *dir_drv,struct dentry *dir_diags);
//void asr_dbgfs_trigger_fw_dump(struct asr_hw *asr_hw, char *reason);

//void asr_fw_trace_dump(struct asr_hw *asr_hw);
//void asr_fw_trace_reset(struct asr_hw *asr_hw);

#else

struct asr_debugfs {
};

static inline int asr_dbgfs_register(struct asr_hw *asr_hw, const char *name)
{
	return 0;
}

static inline void asr_dbgfs_unregister(struct asr_hw *asr_hw)
{
}

//static inline int asr_um_helper(struct asr_debugfs *asr_debugfs, const char *cmd) { return 0; }
//static inline int asr_trigger_um_helper(struct asr_debugfs *asr_debugfs) {}
static inline void asr_dbgfs_register_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta)
{
}

static inline void asr_dbgfs_unregister_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta)
{
}

//void asr_fw_trace_dump(struct asr_hw *asr_hw) {}
//void asr_fw_trace_reset(struct asr_hw *asr_hw) {}

#endif /* CONFIG_ASR_DEBUGFS */

#endif /* _ASR_DEBUGFS_H_ */

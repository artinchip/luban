#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <pthread.h>
#include "wpa_ctrl.h"
#include "wifi.h"
#include <unistd.h>
#include "log.h"
#define IFACE_VALUE_MAX 32

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;

/* socket pair used to exit from a blocking read */
static int exit_sockets[2];

static char primary_iface[IFACE_VALUE_MAX];
// TODO: use new ANDROID_SOCKET mechanism, once support for multiple
// sockets is in

#define WIFI_TEST_INTERFACE    "sta"

static const char IFACE_DIR[]       = "/var/run/wifidaemon";
static const char SUPP_CONFIG_TEMPLATE[]= "/etc/wifi/wpa_supplicant_src.conf";
static const char SUPP_CONFIG_FILE[]    = "/etc/wifi/wpa_supplicant.conf";
static const char CONTROL_IFACE_PATH[]  = "/var/run/wifidaemon";

static const char SUPP_ENTROPY_FILE[]   = WIFI_ENTROPY_FILE;
static unsigned char dummy_key[21] = { 0x02, 0x11, 0xbe, 0x33, 0x43, 0x35,
                                       0x68, 0x47, 0x84, 0x99, 0xa9, 0x2b,
                                       0x1c, 0xd3, 0xee, 0xff, 0xf1, 0xe2,
                                       0xf3, 0xf4, 0xf5 };

static const char IFNAME[]    = "IFNAME=";
#define IFNAMELEN           (sizeof(IFNAME) - 1)
static const char WPA_EVENT_IGNORE[]    = "CTRL-EVENT-IGNORE ";

static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret;

    module = load_file(filename, &size);
    if (!module)
        return -1;

    ret = init_module(module, size, args);

    free(module);

    return ret;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        wifimanager_debug(MSG_DEBUG, "Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

// int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
//                     int *dns1, int *dns2, int *server, int *lease) {
//     /* For test driver, always report success */
//     if (strcmp(primary_iface, WIFI_TEST_INTERFACE) == 0)
//         return 0;

//     if (ifc_init() < 0)
//         return -1;

//     if (do_dhcp(primary_iface) < 0) {
//         ifc_close();
//         return -1;
//     }
//     ifc_close();
//     get_dhcp_info(ipaddr, gateway, mask, dns1, dns2, server, lease);
//     return 0;
// }

// const char *get_dhcp_error_string() {
//     return dhcp_lasterror();
// }

static int get_process_state(const char *name)
{
	int bytes;
	char buf[10];
	char cmd[60];
	FILE   *strea;

	if (strlen(name) > 20) {
		wifimanager_debug(MSG_DEBUG, "%s :process name too long\n", name);
		return -1;
	}

	sprintf(cmd, "ps | grep %s | grep -v grep", name);
	strea = popen(cmd, "r" );
	if(!strea) return -1;
	bytes = fread( buf, sizeof(char), sizeof(buf), strea);
	pclose(strea);
	if(bytes > 0) {
		return 1;
	}else {
		wifimanager_debug(MSG_DEBUG, "%s :process not exist\n", name);
		return -1;
	}
}

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
int wifi_change_driver_state(const char *state)
{
    int len;
    int fd;
    int ret = 0;

    if (!state)
        return -1;
    fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_STATE_CTRL_PARAM, O_WRONLY));
    if (fd < 0) {
        wifimanager_debug(MSG_ERROR,"Failed to open driver state control param (%s)", strerror(errno));
        return -1;
    }
    len = strlen(state) + 1;
    if (TEMP_FAILURE_RETRY(write(fd, state, len)) != len) {
        wifimanager_debug(MSG_ERROR,"Failed to write driver state control param (%s)", strerror(errno));
        ret = -1;
    }
    close(fd);
    return ret;
}
#endif

int is_wifi_driver_loaded() {
//     char driver_status[IFACE_VALUE_MAX];
// #ifdef WIFI_DRIVER_MODULE_PATH
//     FILE *proc;
//     char line[sizeof(DRIVER_MODULE_TAG)+10];
// #endif

//     if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)
//             || strcmp(driver_status, "ok") != 0) {
//         return 0;  /* driver not loaded */
//     }
// #ifdef WIFI_DRIVER_MODULE_PATH
//     /*
//      * If the property says the driver is loaded, check to
//      * make sure that the property setting isn't just left
//      * over from a previous manual shutdown or a runtime
//      * crash.
//      */
//     if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
//         ALOGW("Could not open %s: %s", MODULE_FILE, strerror(errno));
//         property_set(DRIVER_PROP_NAME, "unloaded");
//         return 0;
//     }
//     while ((fgets(line, sizeof(line), proc)) != NULL) {
//         if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
//             fclose(proc);
//             return 1;
//         }
//     }
//     fclose(proc);
//     property_set(DRIVER_PROP_NAME, "unloaded");
//     return 0;
// #else
//     return 1;
// #endif
    return 0;
}

int wifi_load_driver()
{
// #ifdef WIFI_DRIVER_MODULE_PATH
//     char driver_status[IFACE_VALUE_MAX];
//     int count = 100; /* wait at most 20 seconds for completion */

//     if (is_wifi_driver_loaded()) {
//         return 0;
//     }

//     if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0)
//         return -1;

//     if (strcmp(FIRMWARE_LOADER,"") == 0) {
//         /* usleep(WIFI_DRIVER_LOADER_DELAY); */
//         property_set(DRIVER_PROP_NAME, "ok");
//     }
//     else {
//         property_set("ctl.start", FIRMWARE_LOADER);
//     }
//     sched_yield();
//     while (count-- > 0) {
//         if (property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
//             if (strcmp(driver_status, "ok") == 0)
//                 return 0;
//             else if (strcmp(driver_status, "failed") == 0) {
//                 wifi_unload_driver();
//                 return -1;
//             }
//         }
//         usleep(200000);
//     }
//     property_set(DRIVER_PROP_NAME, "timeout");
//     wifi_unload_driver();
//     return -1;
// #else
// #ifdef WIFI_DRIVER_STATE_CTRL_PARAM
//     if (is_wifi_driver_loaded()) {
//         return 0;
//     }

//     if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0)
//         return -1;
// #endif
//     property_set(DRIVER_PROP_NAME, "ok");
//     return 0;
// #endif
    return 0;
}


int wifi_unload_driver()
{
//     usleep(200000); /* allow to finish interface down */
// #ifdef WIFI_DRIVER_MODULE_PATH
//     if (rmmod(DRIVER_MODULE_NAME) == 0) {
//         int count = 20; /* wait at most 10 seconds for completion */
//         while (count-- > 0) {
//             if (!is_wifi_driver_loaded())
//                 break;
//             usleep(500000);
//         }
//         usleep(500000); /* allow card removal */
//         if (count) {
//             return 0;
//         }
//         return -1;
//     } else
//         return -1;
// #else
// #ifdef WIFI_DRIVER_STATE_CTRL_PARAM
//     if (is_wifi_driver_loaded()) {
//         if (wifi_change_driver_state(WIFI_DRIVER_STATE_OFF) < 0)
//             return -1;
//     }
// #endif
//     property_set(DRIVER_PROP_NAME, "unloaded");
//     return 0;
// #endif
    return 0;
}

int ensure_entropy_file_exists()
{
    int ret;
    int destfd;

    ret = access(SUPP_ENTROPY_FILE, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(SUPP_ENTROPY_FILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            wifimanager_debug(MSG_ERROR, "Cannot set RW to \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
            return -1;
        }
        return 0;
    }
    destfd = TEMP_FAILURE_RETRY(open(SUPP_ENTROPY_FILE, O_CREAT|O_RDWR, 0660));
    if (destfd < 0) {
        wifimanager_debug(MSG_DEBUG, "Cannot create \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
        return -1;
    }

    if (TEMP_FAILURE_RETRY(write(destfd, dummy_key, sizeof(dummy_key))) != sizeof(dummy_key)) {
        wifimanager_debug(MSG_ERROR, "Error writing \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
        close(destfd);
        return -1;
    }
    close(destfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(SUPP_ENTROPY_FILE, 0660) < 0) {
        wifimanager_debug(MSG_ERROR, "Error changing permissions of %s to 0660: %s\n",
             SUPP_ENTROPY_FILE, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }

    return 0;
}

int ensure_config_file_exists(const char *config_file)
{
    char buf[2048];
    int srcfd, destfd;
    struct stat sb;
    int nread;
    int ret;

    ret = access(config_file, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(config_file, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            wifimanager_debug(MSG_ERROR,"Cannot set RW to \"%s\": %s", config_file, strerror(errno));
            return -1;
        }
        return 0;
    } else if (errno != ENOENT) {
        wifimanager_debug(MSG_ERROR,"Cannot access \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    srcfd = TEMP_FAILURE_RETRY(open(SUPP_CONFIG_TEMPLATE, O_RDONLY));
    if (srcfd < 0) {
        wifimanager_debug(MSG_ERROR,"Cannot open \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
        return -1;
    }

    destfd = TEMP_FAILURE_RETRY(open(config_file, O_CREAT|O_RDWR, 0660));
    if (destfd < 0) {
        close(srcfd);
        wifimanager_debug(MSG_ERROR,"Cannot create \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    while ((nread = TEMP_FAILURE_RETRY(read(srcfd, buf, sizeof(buf)))) != 0) {
        if (nread < 0) {
            wifimanager_debug(MSG_ERROR,"Error reading \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(config_file);
            return -1;
        }
        TEMP_FAILURE_RETRY(write(destfd, buf, nread));
    }

    close(destfd);
    close(srcfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(config_file, 0660) < 0) {
        wifimanager_debug(MSG_ERROR,"Error changing permissions of %s to 0660: %s",
             config_file, strerror(errno));
        unlink(config_file);
        return -1;
    }

    // if (chown(config_file, AID_SYSTEM, AID_WIFI) < 0) {
    //     wifimanager_debug(MSG_ERROR,"Error changing group ownership of %s to %d: %s",
    //          config_file, AID_WIFI, strerror(errno));
    //     unlink(config_file);
    //     return -1;
    // }
    return 0;
}


int wifi_start_supplicant(int p2p_supported)
{
    // char supp_status[IFACE_VALUE_MAX] = {'\0'};
    // int count = 200; /* wait at most 20 seconds for completion */
    // const prop_info *pi;
    // unsigned serial = 0, i;

    // if (p2p_supported) {
    //     strcpy(supplicant_name, P2P_SUPPLICANT_NAME);
    //     strcpy(supplicant_prop_name, P2P_PROP_NAME);

    //     /* Ensure p2p config file is created */
    //     if (ensure_config_file_exists(P2P_CONFIG_FILE) < 0) {
    //         wifimanager_debug(MSG_ERROR,"Failed to create a p2p config file");
    //         return -1;
    //     }

    // } else {
    //     strcpy(supplicant_name, SUPPLICANT_NAME);
    //     strcpy(supplicant_prop_name, SUPP_PROP_NAME);
    // }

    // /* Check whether already running */
    // if (property_get(supplicant_prop_name, supp_status, NULL)
    //         && strcmp(supp_status, "running") == 0) {
    //     return 0;
    // }

    // /* Before starting the daemon, make sure its config file exists */
    // if (ensure_config_file_exists(SUPP_CONFIG_FILE) < 0) {
    //     wifimanager_debug(MSG_ERROR,"Wi-Fi will not be enabled");
    //     return -1;
    // }

    // if (ensure_entropy_file_exists() < 0) {
    //     wifimanager_debug(MSG_ERROR,"Wi-Fi entropy file was not created");
    // }

    // /* Clear out any stale socket files that might be left over. */
    // wpa_ctrl_cleanup();

    // /* Reset sockets used for exiting from hung state */
    // exit_sockets[0] = exit_sockets[1] = -1;

    // /*
    //  * Get a reference to the status property, so we can distinguish
    //  * the case where it goes stopped => running => stopped (i.e.,
    //  * it start up, but fails right away) from the case in which
    //  * it starts in the stopped state and never manages to start
    //  * running at all.
    //  */
    // pi = __system_property_find(supplicant_prop_name);
    // if (pi != NULL) {
    //     serial = __system_property_serial(pi);
    // }
    // property_get("wifi.interface", primary_iface, WIFI_TEST_INTERFACE);

    // property_set("ctl.start", supplicant_name);
    // sched_yield();

    // while (count-- > 0) {
    //     if (pi == NULL) {
    //         pi = __system_property_find(supplicant_prop_name);
    //     }
    //     if (pi != NULL) {
    //         /*
    //          * property serial updated means that init process is scheduled
    //          * after we sched_yield, further property status checking is based on this */
    //         if (__system_property_serial(pi) != serial) {
    //             __system_property_read(pi, NULL, supp_status);
    //             if (strcmp(supp_status, "running") == 0) {
    //                 return 0;
    //             } else if (strcmp(supp_status, "stopped") == 0) {
    //                 return -1;
    //             }
    //         }
    //     }
    //     usleep(100000);
    // }
    if(get_process_state("wpa_supplicant") != -1)
        return -1;

    system("wpa_supplicant -iwlan0 -Dnl80211 -c /etc/wifi/wpa_supplicant.conf -O /var/run/wifidaemon -B");
    return 0;
}

int wifi_stop_supplicant(int p2p_supported)
{
    // char supp_status[IFACE_VALUE_MAX] = {'\0'};
    // int count = 50; /* wait at most 5 seconds for completion */

    // if (p2p_supported) {
    //     strcpy(supplicant_name, P2P_SUPPLICANT_NAME);
    //     strcpy(supplicant_prop_name, P2P_PROP_NAME);
    // } else {
    //     strcpy(supplicant_name, SUPPLICANT_NAME);
    //     strcpy(supplicant_prop_name, SUPP_PROP_NAME);
    // }

    // /* Check whether supplicant already stopped */
    // if (property_get(supplicant_prop_name, supp_status, NULL)
    //     && strcmp(supp_status, "stopped") == 0) {
    //     return 0;
    // }

    // property_set("ctl.stop", supplicant_name);
    // sched_yield();

    // while (count-- > 0) {
    //     if (property_get(supplicant_prop_name, supp_status, NULL)) {
    //         if (strcmp(supp_status, "stopped") == 0)
    //             return 0;
    //     }
    //     usleep(100000);
    // }
    // wifimanager_debug(MSG_ERROR,"Failed to stop supplicant");

    system("killall wpa_supplicant");
    return 0;
}

#define SUPPLICANT_TIMEOUT      3000000  // microseconds
#define SUPPLICANT_TIMEOUT_STEP  100000  // microseconds
int wifi_connect_on_socket_path(const char *path)
{
    int  supplicant_timeout = SUPPLICANT_TIMEOUT;

    ctrl_conn = wpa_ctrl_open(path);
    while (ctrl_conn == NULL && supplicant_timeout > 0) {
        usleep(SUPPLICANT_TIMEOUT_STEP);
        supplicant_timeout -= SUPPLICANT_TIMEOUT_STEP;
        ctrl_conn = wpa_ctrl_open(path);
    }
    if (ctrl_conn == NULL) {
        wifimanager_debug(MSG_ERROR, "Unable to open connection to supplicant on \"%s\": %s\n",
             path, strerror(errno));
        return -1;
    }
    monitor_conn = wpa_ctrl_open(path);
    if (monitor_conn == NULL) {
        wifimanager_debug(MSG_ERROR, "monitor_conn is NULL!\n");
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
        return -1;
    }
    if (wpa_ctrl_attach(monitor_conn) != 0) {
        wifimanager_debug(MSG_ERROR, "attach monitor_conn error!\n");
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1) {
        wifimanager_debug(MSG_ERROR, "create socketpair error!\n");
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }

    wifimanager_debug(MSG_DEBUG, "connect to wpa_supplicant ok!\n");
    return 0;
}

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Establishes the control and monitor socket connections on the interface */
int wifi_connect_to_supplicant()
{
    static char path[PATH_MAX];
    if(get_process_state("wpa_supplicant") == -1)
        return -1;
    strncpy(primary_iface, "wlan0", IFACE_VALUE_MAX);
    if (access(IFACE_DIR, F_OK) == 0) {
        snprintf(path, sizeof(path), "%s/%s", IFACE_DIR, primary_iface);
    } else {
        wifimanager_debug(MSG_ERROR, "wpa_supplicant socket interface not exists\n");
        return -1;
    }
    return wifi_connect_on_socket_path(path);
}

int wifi_send_command(const char *cmd, char *reply, size_t *reply_len)
{
    int ret;
    if (ctrl_conn == NULL) {
        wifimanager_debug(MSG_ERROR, "Not connected to wpa_supplicant - \"%s\" command dropped.\n", cmd);
        return -1;
    }

    ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, reply_len, NULL);
    if (ret == -2) {
        wifimanager_debug(MSG_ERROR, "'%s' command timed out.\n", cmd);
        /* unblocks the monitor receive socket for termination */
        TEMP_FAILURE_RETRY(write(exit_sockets[0], "T", 1));
        return -2;
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        return -1;
    }
    if (strncmp(cmd, "PING", 4) == 0) {
        reply[*reply_len] = '\0';
    }
    return 0;
}

int wifi_supplicant_connection_active()
{
    if(get_process_state("wpa_supplicant") == -1)
        return -1;

    return 0;
}

int wifi_ctrl_recv(char *reply, size_t *reply_len)
{
    int res;
    int ctrlfd = wpa_ctrl_get_fd(monitor_conn);
    struct pollfd rfds[2];

    memset(rfds, 0, 2 * sizeof(struct pollfd));
    rfds[0].fd = ctrlfd;
    rfds[0].events |= POLLIN;
    rfds[1].fd = exit_sockets[1];
    rfds[1].events |= POLLIN;
    do {
        res = TEMP_FAILURE_RETRY(poll(rfds, 2, 30000));
        if (res < 0) {
            wifimanager_debug(MSG_ERROR,"Error poll = %d", res);
            return res;
        } else if (res == 0) {
            /* timed out, check if supplicant is active
             * or not ..
             */
            res = wifi_supplicant_connection_active();
            if (res < 0)
                return -2;
        }
    } while (res == 0);

    if (rfds[0].revents & POLLIN) {
        return wpa_ctrl_recv(monitor_conn, reply, reply_len);
    }

    wifimanager_debug(MSG_ERROR,"wifi_ctrl_recv poll error\n");
    /* it is not rfds[0], then it must be rfts[1] (i.e. the exit socket)
     * or we timed out. In either case, this call has failed ..
     */
    return -2;
}

int wifi_wait_on_socket(char *buf, size_t buflen)
{
    size_t nread = buflen - 1;
    int result;
    char *match, *match2;

    if (monitor_conn == NULL) {
        return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - connection closed");
    }

    result = wifi_ctrl_recv(buf, &nread);

    /* Terminate reception on exit socket */
    if (result == -2) {
        return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - connection closed");
    }

    if (result < 0) {
        wifimanager_debug(MSG_ERROR, "wifi_ctrl_recv failed: %s\n", strerror(errno));
        return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - recv error");
    }
    buf[nread] = '\0';
    /* Check for EOF on the socket */
    if (result == 0 && nread == 0) {
        /* Fabricate an event to pass up */
        wifimanager_debug(MSG_WARNING, "Received EOF on supplicant socket\n");
        return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - signal 0 received");
    }
    /*
     * Events strings are in the format
     *
     *     IFNAME=iface <N>CTRL-EVENT-XXX
     *        or
     *     <N>CTRL-EVENT-XXX
     *
     * where N is the message level in numerical form (0=VERBOSE, 1=DEBUG,
     * etc.) and XXX is the event nae. The level information is not useful
     * to us, so strip it off.
     */

    if (strncmp(buf, IFNAME, IFNAMELEN) == 0) {
        match = strchr(buf, ' ');
        if (match != NULL) {
            if (match[1] == '<') {
                match2 = strchr(match + 2, '>');
                if (match2 != NULL) {
                    nread -= (match2 - match);
                    memmove(match + 1, match2 + 1, nread - (match - buf) + 1);
                }
            }
        } else {
            return snprintf(buf, buflen, "%s", WPA_EVENT_IGNORE);
        }
    } else if (buf[0] == '<') {
        match = strchr(buf, '>');
        if (match != NULL) {
            nread -= (match + 1 - buf);
            memmove(buf, match + 1, nread + 1);
            //printf("supplicant generated event without interface - %s\n", buf);
        }
    } else {
        /* let the event go as is! */
        //printf("supplicant generated event without interface and without message level - %s\n", buf);
    }

    return nread;
}

int wifi_wait_for_event(char *buf, size_t buflen)
{
    return wifi_wait_on_socket(buf, buflen);
}

void wifi_close_sockets()
{
    char reply[4096] = {0};
    int ret = 0;

    if (monitor_conn != NULL) {
        wpa_ctrl_detach(monitor_conn);
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }

/*
    ret = wifi_command("TERMINATE", reply, sizeof(reply));
    if(ret) {
        wifimanager_debug(MSG_WARNING, "do terminate error!");
    }
*/
    if (ctrl_conn != NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }

    if (exit_sockets[0] >= 0) {
        close(exit_sockets[0]);
        exit_sockets[0] = -1;
    }

    if (exit_sockets[1] >= 0) {
        close(exit_sockets[1]);
        exit_sockets[1] = -1;
    }
}

void wifi_close_supplicant_connection()
{
    wifi_close_sockets();
}

int wifi_command(char const *cmd, char *reply, size_t reply_len)
{
    static unsigned int command_times = 0;
    if(!cmd || !cmd[0]) {
        return -1;
    }

    wifimanager_debug(MSG_DEBUG, "-----------------WPAS CMD [%d]----------------\n", command_times++);
    wifimanager_debug(MSG_DEBUG, ">>>>>> %s\n", cmd);

    --reply_len; // Ensure we have room to add NUL termination.
    if (wifi_send_command(cmd, reply, &reply_len) != 0) {
        wifimanager_debug(MSG_DEBUG, "<<<<<< Command %d Failed\n", command_times);
        return -1;
    }

    // Strip off trailing newline.
    if (reply_len > 0 && reply[reply_len-1] == '\n') {
        reply[reply_len-1] = '\0';
    } else {
        reply[reply_len] = '\0';
    }

    wifimanager_debug(MSG_DEBUG, "<<<<<< %s\n", reply);
    return 0;
}

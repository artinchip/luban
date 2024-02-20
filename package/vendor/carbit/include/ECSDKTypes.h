#ifndef ECSDKTYPES_H
#define ECSDKTYPES_H

#include <stdint.h>
#include <string>
#include <vector>

#ifdef PLATFORM_WINDOWS
#ifdef ECLIBRARY_EXPORTS
#define EC_DLL_EXPORT _declspec(dllexport)
#else
#define EC_DLL_EXPORT _declspec(dllimport)
#endif
#else // !PLATFORM_WIN32
#define EC_DLL_EXPORT
#endif

using  std::string;
using  std::vector;

namespace  ECSDKFrameWork {

#define	ECSDK_ASYNC					 1              ///< asynchronous message
#define ECSDK_OK                     0              ///< success
#define ECSDK_ERR_INVAL_OP          -1              ///< invalid operation
#define ECSDK_ERR_INVAL_PARAM       -2              ///< invalid parameter(s)
#define ECSDK_ERR_OP_FAIL           -3              ///< operation failed
#define ECSDK_ERR_LICENSE_AUTH_FAIL -4              ///< license authorize failed
#define ECSDK_ERR_APP_NOT_STARTED   -5              ///< phone app not started
#define ECSDK_ERR_APP_AUTH_FAIL     -6              ///< phone app authorization failed.
#define ECSDK_ERR_APP_AUTH_PENDING  -7              ///< phone app authorization pending.


enum ECSDKConnectedStatus
{
    EC_CONNECT_STATUS_DEVICE_ATTACHED,
    EC_CONNECT_STATUS_DEVICE_DEATTACHED,
    EC_CONNECT_STATUS_CONNECTING,
    EC_CONNECT_STATUS_CONNECT_FAILED,
    EC_CONNECT_STATUS_CONNECT_SUCCEED,
    EC_CONNECT_STATUS_DISCONNECTED,
    EC_CONNECT_STATUS_APP_EXIT,
    EC_CONNECT_STATUS_INTERRUPTED_BY_APP,
};

enum ECSDKConnectedType
{
    EC_CONNECT_TYPE_ANDROID_USB= 0,                 ///< android usb
    EC_CONNECT_TYPE_ANDROID_WIFI,                   ///< android wifi
    EC_CONNECT_TYPE_IOS_USB_EAP,                    ///< iphone usb eap, app screen
    EC_CONNECT_TYPE_IOS_USB_MUX,                    ///< iphone usb mux, app screen
    EC_CONNECT_TYPE_IOS_USB_AIRPLAY,                ///< iphone usb airplay, system screen
    EC_CONNECT_TYPE_IOS_WIFI_APP,                   ///< iphone wifi app, app screen
    EC_CONNECT_TYPE_IOS_WIFI_AIRPLAY,               ///< iphone wifi airplay, system screen
    EC_CONNECT_TYPE_IOS_USB_LIGHTNING,              ///< iphone usb lightning connect(闪连)
    EC_CONNECT_TYPE_MAX,                            ///< reserve
};

enum ECAuthSuccessCode
{
	EC_CAR_NETWORK_AUTH_CHECK_UUID_LICENSE = 0x1010,
	EC_CAR_NETWORK_AUTH_REGISTER_UUID_LICENSE = 0x1020,
	EC_CAR_NETWORK_AUTH_DOWNLOAD_UUID_LICENSE = 0x1030,
	EC_PHONE_NETWORK_AUTH_CHECK_UUID_LICENSE = 0x2010,
	EC_PHONE_NETWORK_AUTH_REGISTER_UUID_LICENSE = 0x2020,
	EC_PHONE_NETWORK_AUTH_DOWNLOAD_UUID_LICENSE = 0x2030
};

enum ECSDKMirrorStatus
{
    EC_MIRROR_STATUS_MIRROR_AUTH_PENDING,
    EC_MIRROR_STATUS_MIRROR_STARTED,
    EC_MIRROR_STATUS_MIRROR_FAILED,
	EC_MIRROR_STATUS_MIRROR_STOPPED,
};

/**
* @enum ECTouchEventType
*
* @brief touch event type
*
* @see ECTouchEventData
*/
enum ECTouchEventType
{
    EC_TOUCH_UP = 0,                             ///< touch up
    EC_TOUCH_DOWN,                               ///< touch down
    EC_TOUCH_MOVE,                               ///< touch move
};

/**
* @enum ECBtnCode
*
* @brief phone app button code
*
* @see ECBtnInfo
*/
enum  ECBtnCode
{
    // for driving mode
    EC_BTN_TALKIE = 0x1010,                         ///< start talkie
    EC_BTN_NAVIGATION = 0x1020,                     ///< start navigation
    EC_BTN_VOICE_ASSISTANT = 0x1030,                ///< start voice assistant
    EC_BTN_MUSIC_PLAY = 0x1040,                     ///< play music
    EC_BTN_MUSIC_NEXT = 0x1041,                     ///< play next music
    EC_BTN_MUSIC_PREVIOUS = 0x1042,                 ///< play previous music
    EC_BTN_MUSIC_PAUSE = 0x1043,                    ///< music pause
    EC_BTN_MUSIC_STOP = 0x1044,			            ///< music stop
    EC_BTN_MUSIC_PLAY_PAUSE = 0x1047,               ///< music switch between pause and stop

    EC_BTN_VOLUME_UP = 0x1050,                      ///< increase the volume of phone
    EC_BTN_VOLUME_DOWN = 0x1051,                    ///< decrease the volume of phone

    EC_BTN_TOPLEFT = 0x1060,                        ///< click top left button
    EC_BTN_TOPRIGHT = 0x1061,                       ///< click top right button
    EC_BTN_BOTTOMLEFT = 0x1062,                     ///< click bottom left button
    EC_BTN_BOTTOMRIGHT = 0x1063,                    ///< click bottom right button

    EC_BTN_MODE = 0x1070,                           ///< click mode button

    EC_BTN_APP_FRONT = 0x1080,						///< make the app of android phone switch to the foreground

    EC_BTN_APP_BACK = 0x1090,                       ///< it works like the function of the back button to the app of the connected phone.

    EC_BTN_ENFORCE_LANDSCAPE = 0x10A0,              ///< make the android phone enforce landscape.
    EC_BTN_CANCEL_LANDSCAPE = 0x10A1,               ///< make the android phone cancel landscape.
    EC_BTN_ENFORCE_OR_CANCEL_LANDSCAPE = 0x10A2,    ///< make the android phone enforce or cancel landscape.

	EC_BTN_FOCUS_BACKWARD = 0x1101,                      ///< 按键焦点向后
	EC_BTN_FOCUS_FORWARD = 0x1102,                      ///< 按键焦点后前
	EC_BTN_FOCUS_LEFT = 0x1103,                          ///< 按键焦点向左
	EC_BTN_FOCUS_RIGHT = 0x1104,                          ///< 按键焦点向右
	EC_BTN_FOCUS_UP = 0x1105,                              ///< 按键焦点向上
	EC_BTN_FOCUS_DOWN = 0x1106,                          ///< 按键焦点上下
	EC_BTN_FOCUS_ENTER = 0x1107,                          ///< 按键焦点确认

	EC_BTN_KEY_PARALLEL_WORLD_MENU = 0x1200,             ///< 平行世界MENU按键
	EC_BTN_KEY_PARALLEL_WORLD_HOME = 0x1201,             ///< 平行世界HOME按键
	EC_BTN_KEY_PARALLEL_WORLD_BACK = 0x1202,             ///< 平行世界BACK按键

	EC_BTN_SYSTEM_HOME = 0x2010,                         ///< The Android phone's HOME key
	EC_BTN_SYSTEM_BACK = 0x2012,                             ///< The Android phone's BACK key

    EC_BTN_MAX,                                     ///< reserve
};

/**
* @enum ECBtnEventType
*
* @brief physical button action code
*
* @see ECBtnInfo
*/
enum  ECBtnEventType
{
    EC_BTN_TYPE_UP = 0,                          ///< key up
    EC_BTN_TYPE_DOWN,                            ///< key down
    EC_BTN_TYPE_CLICK,                           ///< click
    EC_BTN_TYPE_DOUBLE_CLICK,                    ///< double click
    EC_BTN_TYPE_LONG_PRESS,                      ///< long press

    EC_BTN_TYPE_MAX,                             ///< reserve
};


/**
* @enum ECAppPage
*
* @brief app page
*
*/
enum ECAppPage
{
    EC_APP_PAGE_NAVIGATION = 1,                   ///< navigation page
    EC_APP_PAGE_MUSIC = 2,                        ///< music page
    EC_APP_PAGE_VR = 3,                           ///< voice assistance page
    EC_APP_PAGE_TALKIE = 4,                       ///< talkie page
    EP_APP_PAGE_NAVI_HOME = 5,                    ///< navigation to go home
    EC_APP_PAGE_NAVI_WORK = 6,                    ///< navigation to go to work
    EC_APP_PAGE_MAIN = 7,                         ///< main page
    EC_APP_PAGE_NAVI_GAS_STATION = 8,             ///< navigation to go to gas station
    EC_APP_PAGE_CAR_PARK = 9,                      ///< navigation to go to car park
	EC_APP_PAGE_4S_SHOP = 10,                     ///< navigation to 4s shop
	EC_APP_PAGE_MUSIC_XMLY = 11,                  ///< music of XMLY
	EC_APP_PAGE_MUSIC_QQ = 12,                    ///< music of QQ
	EC_APP_PAGE_MUSIC_RADIO = 13,                 ///< web radio
	EC_APP_PAGE_MUSIC_NATIVE = 14,                ///< music of local
	EC_APP_PAGE_WECHAT = 15,                      ///< wechat
	EC_APP_PAGE_PERSONAL_CENTER = 16,             ///< personal center
	EC_APP_PAGE_APP_MANAGER = 17,                 ///< the manager of the thirdparty app
	EC_APP_PAGE_ADD_OTA = 18,                      ///< the OTA download
	EC_APP_PAGE_WECHAT_MSG = 19,                  ///< the wechat msg page
	EC_APP_PAGE_PHONE = 20,                       ///< the phone page
	EC_APP_PAGE_ANDROID_PARALLEL_WORLD = 100,     ///< android parallel world page
	EC_APP_PAGE_PHONE_SCREEN = 101,               ///< phone screen mirror page
};

enum ECCarStatusType
{
	EC_CAR_REVERSING = 1,                         ///< car reverse    
	EC_CAR_BLUETOOTH,                             ///< car bluetooth
	EC_CAR_DRIVINGMODE,                           ///< car driving mode
	EC_CAR_AUDIO_FOCUS_CHANGE,                    ///< car audio focus change
	EC_CAR_IS_AUTO_START_EASYCONN,                ///< whether car auto start easyconn
	EC_CAR_MIC_STATUS,			                  ///< car mic status:open/closed
	EC_CAR_RUN_STATUS,							  ///< car easyconn run status
};

enum ECCarStatusValue
{
	EC_CAR_STATUS_FALSE = 0,
	EC_CAR_STATUS_TRUE,

	EC_CAR_STATUS_STARTED,
	EC_CAR_STATUS_STOPPED,

	EC_CAR_STATUS_CLOSED,
	EC_CAR_STATUS_UNCONNECTED,
	EC_CAR_STATUS_CONNECTED,
	EC_CAR_STATUS_CONNECTED_SAME,

	EC_CAR_STATUS_LONG_FUCUS_GAIN,
	EC_CAR_STATUS_LONG_FUCUS_LOSS,
	EC_CAR_STATUS_SHORT_FUCUS_GAIN,
	EC_CAR_STATUS_SHORT_FUCUS_LOSS,
	EC_CAR_STATUS_FADEDOWN_FUCUS_GAIN,
	EC_CAR_STATUS_FADEDOWN_FUCUS_LOSS,

	EC_CAR_STATUS_MIC_OPEN,
	EC_CAR_STATUS_MIC_CLOSED,

	EC_CAR_STATUS_RUN_FOREGROUND,
	EC_CAR_STATUS_RUN_BACKGROUND,
};

/**
* @enum ECStatusMessage
*
* @brief status message code
*
* @see IECPhoneMutual::onECStatusMessage
*/
enum ECStatusMessage
{
	EC_STATUS_MESSAGE_USER_NOT_AUTH_DEBUG = 0,       ///< user did not authorize car to use phone via usb debug mode.
	EC_STATUS_MESSAGE_APP_NOT_RUNNING = 1,           ///< phone app is not running
	EC_STATUS_MESSAGE_APP_FOREGROUND = 2,            ///< phone app runs in foreground,deprecated ,instead of EC_STATUS_MESSAGE_APP_FOREGROUND_SAME or EC_STATUS_MESSAGE_APP_FOREGROUND_SPLIT
	EC_STATUS_MESSAGE_APP_FOREGROUND_SAME = 3,       ///< phone app runs in foreground with same screen mode
	EC_STATUS_MESSAGE_APP_FOREGROUND_SPLIT = 4,      ///< phone app runs in foreground with split screen mode
	EC_STATUS_MESSAGE_APP_BACKGROUND_SAME = 5,       ///< phone app runs in background with same screen mode
	EC_STATUS_MESSAGE_APP_BACKGROUND_SPLIT = 6,      ///< phone app runs in background with split screen mode
	EC_STATUS_MESSAGE_APP_SCREENLOCKED = 7,          ///< screen locked
	EC_STATUS_MESSAGE_APP_UNSCREENLOCKED = 8,        ///< screen unlocked
	EC_STATUS_MESSAGE_APP_NAVI_STARTED = 9,          ///< phone app started to navigate
	EC_STATUS_MESSAGE_APP_NAVI_STOPPED = 10,           ///< phone app stopped to navigate
	EC_STATUS_MESSAGE_SWITCH_AOA_FAIL = 11,           ///< failed to switch AOA mode for current android phone
	EC_STATUS_MESSAGE_3RD_APP_HORIZONTAL_NOT_SUPPORT = 12,    ///< 3rd app does not support horizontal screen
	EC_STATUS_MESSAGE_3RD_APP_HORIZONTAL_SUPPORT = 13,        ///< 3rd app supports horizontal screen
	EC_STATUS_MESSAGE_APP_VR_STARTED = 14,            ///< vr of phone app started.
	EC_STATUS_MESSAGE_APP_VR_STOPPED = 15,            ///< vr of phone app stopped.
	EC_STATUS_MESSAGE_ACQUIRE_BLUETOOTH_A2DP = 16,    ///< HU needed to acquire bluetooth for music play and send play event to phone via bluetooth.
	EC_STATUS_MESSAGE_ACQUIRE_BLUETOOTH_A2DP_WITHOUT_SEND_PLAY = 17,    ///< HU needed to acquire bluetooth for music play but not send play event to phone.
	EC_STATUS_MESSAGE_SWITCH_TO_SYSTEM_MAIN_PAGE = 18,///< HU switch to system main page.
	EC_STATUS_MESSAGE_LAUNCH_PHONE_APP = 19,          ///< phone request HU to launch app.
	EC_STATUS_MESSAGE_APP_TALKIE_STARTED = 20,        ///< talkie of phone app started.
	EC_STATUS_MESSAGE_APP_TALKIE_STOPPED = 21,        ///< talkie of phone app stopped.
	EC_STATUS_MESSAGE_APP_MUSIC_PENDING = 22,         ///< music of phone app is pending, it will be deprecated.
	EC_STATUS_MESSAGE_APP_MUSIC_PLAYING = 23,         ///< music of phone app is playing, it will be deprecated.
	EC_STATUS_MESSAGE_APP_MUSIC_PAUSED = 24,          ///< music of phone app paused, it will be deprecated.
	EC_STATUS_MESSAGE_APP_MUSIC_STOPPED = 25,	     ///< music of phone app stopped, it will be deprecated.
	EC_STATUS_MESSAGE_SWITCH_TO_FRONT = 26,           ///< make easyconn switch to front.
	EC_STATUS_MESSAGE_OPEN_BLUETOOTH_PHONE = 27,      ///< make HU open bluetooth phone.
	EC_STATUS_MESSAGE_OPEN_BLUETOOTH_SETTING = 28,    ///< make HU open bluetooth setting.
	EC_STATUS_MESSAGE_TALKIE_MUTE = 29,               ///< talkie of phone app is mute.
	EC_STATUS_MESSAGE_TALKIE_NON_MUTE = 30,           ///< talkie of phone app is not mute.
	EC_STATUS_MESSAGE_TALKIE_NON_LOGIN = 31,          ///< user of phone app is not logged in.
	EC_STATUS_MESSAGE_SWITCH_EASYCONN_TO_BACKGROUND = 32,///< make easyconn switch to background.
	EC_STATUS_MESSAGE_HU_BT_NOT_MATCH = 33,           ///< the HU's BT is not connected with the phone.
	EC_STATUS_MESSAGE_ENABLE_UPLOAD_GPS_INFO = 34,    ///< phone app enable HU to upload gps info.
	EC_STATUS_MESSAGE_DISABLE_UPLOAD_GPS_INFO = 35,   ///< phone app disable HU to upload gps info.
	EC_STATUS_MESSAGE_ENABLE_ACCESSIBILITY = 36,      ///< android phone enable accessibility.
	EC_STATUS_MESSAGE_DISABLE_ACCESSIBILITY = 37,     ///< android phone disable accessibility.
	EC_STATUS_MESSAGE_START_DLNA_SERVICE = 38,        ///< HU needed to start DLNA service.
	EC_STATUS_MESSAGE_STOP_DLNA_SERVICE = 39,         ///< HU needed to stop DLNA service.
	EC_STATUS_MESSAGE_VR_TIPS_HIDE = 40,		      ///< HU needed to hide vr tips .
	EC_STATUS_MESSAGE_VR_TIPS_SHOW = 41,	          ///< HU needed to show vr tips .
	EC_STATUS_MESSAGE_IMUX_READ_VERSION_TIMEOUT = 42, ///< imux read verison package timeout error.
	EC_STATUS_MESSAGE_NET_LINK_OPEN = 43,             ///< netwodk sharing opened
	EC_STATUS_MESSAGE_NET_LINK_CLOSE = 44,            ///< network sharing closed
	EC_STATUS_MESSAGE_IOS_APP_MIRROR_STARTING = 45,   ///< ios app system mirror starting
	EC_STATUS_MESSAGE_IOS_APP_MIRROR_CLOSEING = 46,   ///< ios app system mirror closeing
	EC_STATUS_MESSAGE_MAX                        ///< reserve
};

enum ECNaviStatus
{
    EC_NAVI_STATUS_ACTIVE = 0,
    EC_NAVI_STATUS_INACTIVE,
    EC_NAVI_STATUS_MAX
};

enum ECNaviCameraType
{
    EC_NAVI_CAMERA_SPEED = 0,
    EC_NAVI_CAMERA_SPY,
    EC_NAVI_CAMERA_REDLIGHT,
    EC_NAVI_CAMERA_VIOLATION,
    EC_NAVI_CAMERA_BUS,
    EC_NAVI_CAMERA_EMERGENCY,
    EC_NAVI_CAMERA_MAX
};

enum ECNaviIcon
{
	EC_NAVI_ICON_UNDEFINED = 0,                //收到此值，不显示导航图标
	EC_NAVI_ICON_VEHICLE = 1,                  //自车.请忽略这个元素，从左转图标开始
	EC_NAVI_ICON_LEFT = 2,                     //左转
	EC_NAVI_ICON_RIGHT = 3,                    //右转
	EC_NAVI_ICON_LEFT_FRONT = 4,               //左前方
	EC_NAVI_ICON_RIGHT_FRONT = 5,              //右前方
	EC_NAVI_ICON_LEFT_BACK = 6,                //左后方
	EC_NAVI_ICON_RIGHT_BACK = 7,               //右后方
	EC_NAVI_ICON_LEFT_TURN_AROUND = 8,         //左转掉头
	EC_NAVI_ICON_STRAIGHT = 9,                 //直行
	EC_NAVI_ICON_ARRIVED_WAYPOINT = 10,        //到达途经点
	EC_NAVI_ICON_ENTER_ROUNDABOUT = 11,        //进入环岛
	EC_NAVI_ICON_OUT_ROUNDABOUT = 12,          //驶出环岛
	EC_NAVI_ICON_ARRIVED_SERVICE_AREA = 13,    //到达服务区
	EC_NAVI_ICON_ARRIVED_TOLLGATE = 14,        //到达收费站
	EC_NAVI_ICON_ARRIVED_DESTINATION = 15,     //到达目的地
	EC_NAVI_ICON_ARRIVED_TUNNEL = 16,          //到达隧道
	EC_NAVI_ICON_CROSSWALK = 17,               //通过人行横道
	EC_NAVI_ICON_OVERPASS = 18,                //通过过街天桥
	EC_NAVI_ICON_UNDERPASS = 19,               //通过地下通道
	EC_NAVI_ICON_SQUARE = 20,                  //通过广场
	EC_NAVI_ICON_PARK = 21,                    //通过公园
	EC_NAVI_ICON_STAIRCASE = 22,               //通过扶梯
	EC_NAVI_ICON_LIFT = 23,                    //通过直梯
	EC_NAVI_ICON_CABLEWAY = 24,                //通过索道
	EC_NAVI_ICON_SKY_CHANNEL = 25,             //通过空中通道
	EC_NAVI_ICON_CHANNEL = 26,                 //通过通道、建筑物穿越通道
	EC_NAVI_ICON_WALK_ROAD = 27,               //通过行人道路
	EC_NAVI_ICON_CRUISE_ROUTE = 28,            //通过游船路线
	EC_NAVI_ICON_SIGHTSEEING_BUSLINE = 29,     //通过观光车路线
	EC_NAVI_ICON_SLIDEWAY = 30,                //通过滑道
	EC_NAVI_ICON_LADDER = 31,                  //通过阶梯
	EC_NAVI_ICON_MERGE_LEFT = 51,              //靠左行驶
	EC_NAVI_ICON_MERGE_RIGHT = 52,             //靠右行驶
	EC_NAVI_ICON_SLOW = 53,                    //减速慢行
	EC_NAVI_ICON_ENTRY_RING_LEFT = 54,         //标准小环岛 绕环岛左转，右侧通行地区的逆时针环岛
	EC_NAVI_ICON_ENTRY_RING_RIGHT = 55,        //标准小环岛 绕环岛右转，右侧通行地区的逆时针环岛
	EC_NAVI_ICON_ENTRY_RING_CONTINUE = 56,     //标准小环岛 绕环岛直行，右侧通行地区的逆时针环岛
	EC_NAVI_ICON_ENTRY_RING_UTURN = 57,        //标准小环岛 绕环岛调头，右侧通行地区的逆时针环岛
	EC_NAVI_ICON_MAX
};

/*
* @struct ECNavigationHudInfo
*
* @brief navigation HUD info
*
* @see   IECAPPManagerListener::onPhoneAppHUD
*/
struct ECNavigationHudInfo
{
    ECNaviStatus status;
    string  currentRoad;
    int32_t	carDirection;
    ECNaviCameraType cameraType;
    int32_t	cameraSpeed;
    int32_t	cameraDistance;
    ECNaviIcon naviIcon;
	string  nextRoad;
    int32_t roadRemainingDistance;
    int32_t roadRemainingTime;
    int32_t destinationRemainingDistance;
    int32_t destinationRemainingTime;
    uint64_t arriveTime;
	string   arriveTimeZone;
};


/**
* @brief log level
*
* @see ECSDKAPP::ECLogConfig
*/
enum ECLogLevel
{
    EC_LOG_LEVEL_ALL = 0,                              ///< all log
    EC_LOG_LEVEL_DEBUG,                                ///< debug log
    EC_LOG_LEVEL_INFO,                                 ///< info log
    EC_LOG_LEVEL_WARN,                                 ///< warn log
    EC_LOG_LEVEL_ERROR,                                ///< error log
    EC_LOG_LEVEL_FATAL,                                ///< fatal log
    EC_LOG_LEVEL_OFF                                   ///< no log
};

/**
* @brief log module
*
* @see ECSDKAPP::ECLogConfig
*/
enum ECLogModule
{
    EC_LOG_MODULE_SDK = 0x01,                       ///< output sdk module log
    EC_LOG_MODULE_ADB = 0x02,                       ///< output adb module log
    EC_LOG_MODULE_MUX = 0x04,                       ///< output mux module log
    EC_LOG_MODULE_USB = 0x08,                       ///< output usb module log
    EC_LOG_MODULE_APP = 0x10,                       ///< output ec app module log
	EC_LOG_MODULE_MDNS = 0x20,			    		///< output mdns log
	EC_LOG_MODULE_MCAST = 0x40,
	EC_LOG_MODULE_RTP = 0x80,
	EC_LOG_MODULE_TS = 0x100,
	EC_LOG_MODULE_UPNP = 0x200,
};

/**
* @brief log output destination
*
* @see ECSDKAPP::ECLogConfig
*/
enum ECLogOutputType
{
    EC_LOG_OUT_STD = 0,                              ///< log to std out
    EC_LOG_OUT_FILE,                                 ///< log to file
    EC_LOG_OUT_LOGCAT,                               ///< log to file only for android
    EC_LOG_OUT_SLOGINFO,                             ///< loginfo  only for qnx
};


enum ECAudioType
{
    EC_AUDIO_TYPE_TTS     = 0x01,                   ///< TTS audio
    EC_AUDIO_TYPE_VR      = 0x02,                   ///< VR audio
    EC_AUDIO_TYPE_TALKIE  = 0x04,                   ///< IM audio
    EC_AUDIO_TYPE_MUSIC   = 0x08,                   ///< Music audio
};

/**
* @enum ECAudioChannelType
*
* @brief audio channel type
*
*/
enum ECAudioChannelType
{
    EC_AUDIO_CHANNEL_MONO = 1,                   ///< mono channel
    EC_ADUIO_CHANNEL_STEREO                      ///< stereo channel
};

/**
* @enum ECAudioFormatType
*
* @brief audio format type
*
*/
enum ECAudioFormatType
{
    EC_AUDIO_FORMAT_U8 = 0,                      ///< PCM unsigned 8 bits
    EC_AUDIO_FORMAT_S8,                          ///< PCM signed 8 bits
    EC_AUDIO_FORMAT_U16_LE,                      ///< PCM unsigned little endian 16 bits
    EC_AUDIO_FORMAT_S16_LE,                      ///< PCM signed little endian 16 bits
    EC_AUDIO_FORMAT_U16_BE,                      ///< PCM unsigned big endian 16 bits
    EC_AUDIO_FORMAT_S16_BE,                      ///< PCM signed big endian 16 bits
    EC_AUDIO_FORMAT_U24_LE,                      ///< PCM unsigned little endian 24 bits
    EC_AUDIO_FORMAT_S24_LE,                      ///< PCM signed little endian 24 bits
    EC_AUDIO_FORMAT_U24_BE,                      ///< PCM unsigned big endian 24 bits
    EC_AUDIO_FORMAT_S24_BE,                      ///< PCM signed big endian 24 bits
    EC_AUDIO_FORMAT_U32_LE,                      ///< PCM unsigned little endian 32 bits
    EC_AUDIO_FORMAT_S32_LE,                      ///< PCM signed little endian 32 bits
    EC_AUDIO_FORMAT_U32_BE,                      ///< PCM unsigned big endian 32 bits
    EC_AUDIO_FORMAT_S32_BE,                      ///< PCM signed big endian 32 bits
    EC_AUDIO_FROMAT_F32                          ///< PCM float 32 bits
};

/**
 * @brief The ECOTAUpdateCheckMode enum
 */
enum ECOTAUpdateCheckMode
{
    EC_OTA_CHECK_VIA_DEFAULT = 0,                 ///< let sdk decide which way to check ota update.
    EC_OTA_CHECK_VIA_NETWORK,                     ///< use HU's network to check ota update.
    EC_OTA_CHECK_VIA_PHONE,                       ///< use the connected phone to check ota update.
    EC_OTA_CHECK_ONLY_LOCAL                       ///< check local only to gain specified downloaded software.
};

/*!
 * \brief Message argument used to change EasyCon language.
 *
 * This is a message argument related to the message value \ref EC_SYS_CMD_COMM_CHANGE_LANG
 */
enum ECLanguage
{
    EC_LANG_ZH_CN = 0, /*! Simplified Chinese.*/    // 简体中文
    EC_LANG_EN_EN,     /*! English. */              // 英文
    EC_LANG_ZH_TW,     /*! Traditional Chinese. */  // 繁体中文
    EC_LANG_AR_AE,     /*! Arabic. */               // 阿拉伯文
    EC_LANG_RU_RU,     /*! Russian. */              // 俄语
    EC_LANG_ES_ES,     /*! Spanish. */              // 西班牙语
    EC_LANG_FA_FA,     /*! Persian. */              // 波斯语
    EC_LANG_DE_GE,     /*! German. */               // 德语
    EC_LANG_FR_FR,     /*! French. */               // 法语
    EC_LANG_IT_IT,     /*! Italian. */              // 意大利语
    EC_LANG_PT_BR,     /*! Portuguese. */           // 葡萄牙语
    EC_LANG_IW_IL,     /*! Hebrew. */               // 希伯来语
};

/**
* @enum ECCallType
*
* @brief call type
*
* @see IECAPPManagerListener::onCallAction
*/
enum ECCallType
{
	EC_CALL_TYPE_DAIL = 0,                       ///< ring up
	EC_CALL_TYPE_HANG_UP,                        ///< ring off
	EC_CALL_TYPE_MAX,                            ///< reserve
};

/**
* @enum ECMicType
*
* @brief microphone type
*/
enum ECMicType
{
	EC_MIC_TYPE_NATIVE = 0,                      ///< microphone of car
	EC_MIC_TYPE_PHONE,                           ///< microphone of phone
	EC_MIC_TYPE_UNSUPPORT,                       ///< no microphone
};

enum ECMicFeature
{
	EC_MIC_SUPPORT_ECHO_CANCELLATION = 0x0001,                   ///< echo cancellation by HU
	EC_MIC_SUPPORT_ECHO_CANCELLATION_VIA_PHONE = 0x0002,         ///< echo cancellation by phone
	EC_MIC_SUPPORT_LEFT_CHANNEL_RECORD = 0x0004,                 ///< echo cancellation,left channel is record ,and the right channel is ref
};

/**
* @enum ECMediaLevel
*
* @brief car multimedia level
*/
enum ECMediaLevel
{
	EC_DVD_LEVEL_LOW = 0,                        ///< low-end
	EC_DVD_LEVEL_MIDDLE,                         ///< middle-end
	EC_DVD_LEVEL_HIGH,                           ///< high-end
};

/**
*
* @enum ECScreenType
*
* @brief HU screen type
*/
enum ECScreenType
{
	EC_CAR_SCREEN_HORIZONTAL,                    ///< horizontal screen type
	EC_CAR_SCREEN_VERTICAL,                      ///< vertical screen type
	EC_CAR_SCREEN_HORIZONTIAL_ROUND,             ///<   @deprecate
	EC_CAR_SCREEN_VERTICAL_ROUND,                 ///< @deprecated
	EC_CAR_SCREEN_UNKNOWN                         ///< unknown screen type
};

enum ECSupportConnect
{
	EC_SUPPORT_CONNECT_ADB = 0x01,
	EC_SUPPORT_CONNECT_AOA = 0x02,
	EC_SUPPORT_CONNECT_WIFIDIRECT = 0x08,
	EC_SUPPORT_CONNECT_EAP = 0x20,
	EC_SUPPORT_CONNECT_MUX = 0x40,
	EC_SUPPORT_CONNECT_LIGHTNING = 0x80,
	EC_SUPPORT_CONNECT_IPHONE_WIFI = 0x100,
	EC_SUPPORT_CONNECT_ANDROID_WIFI = 0x200,
	EC_SUPPORT_CONNECT_USB_AIRPLAY = 0x400,
	EC_SUPPORT_CONNECT_WIFI_AIRPLAY = 0x800
};

enum ECSupportFunction
{
	EC_SUPPORT_FUNCTION_INPUT = 0x01,					///< support users's text input with HU's soft keyboard
	EC_SUPPORT_FUNCTION_SPEECH_WAKE = 0x02,				///< support speech wake
	EC_SUPPORT_FUNCTION_VR_TEXT = 0x04,					///< support vr text display on HU's Screen directly instead of Mirroring
	EC_SUPPORT_FUNCTION_BT_MUSIC = 0x08,				///< support BT Music
	EC_SUPPORT_FUNCTION_BT_AUTO_PAIR = 0x10,			///< support BT Auto pairing
	EC_SUPPORT_FUNCTION_HU_HOTSPOT = 0x20,				///< support iPhone wifi connection via HU's AP, iphone can still access the Internet
	EC_SUPPORT_FUNCTION_GPS = 0x40,						///< support upload GPS information to app
	EC_SUPPORT_FUNCTION_WIFI_QR_CONNECT = 0x80,			///< support wifi QR connection
	EC_SUPPORT_FUNCTION_BT_CALL_BY_HU = 0x100,			///< support call action by BT
	EC_SUPPORT_FUNCTION_DLNA = 0x200,          			///< support DLNA service        			///< support DLNA service
	EC_SUPPORT_FUNCTION_PHONE_CONTROL_CAR = 0x400,		///< support phone control car devices
	EC_SUPPORT_FUNCTION_MEDIA_TRANSPORT_VIA_EC = 0x800, ///< support media data transport to HU via EC(wifi or usb)
	EC_SUPPORT_FUNCTION_ANDROID_PARALLEL_WORLD = 0x1000, ///< support Android app enable parallel world
    EC_SUPPORT_FUNCTION_BLE_CONNECT = 0x2000,            ///< support BLE connection
};

enum ECProductType
{
	EC_PRODUCT_TYPE_DEFAULT = 0,                 ///< default product type
	EC_PRODUCT_TYPE_DA,                          ///< product type of direct account 
	EC_PRODUCT_TYPE_MU,                          ///< product type mobile head unit 
	EC_PRODUCT_TYPE_NI                           ///< product type of navigation instrument 
};

/**
*
* @enum ECProjectFlavor
*
* @brief HU Project market for sale,This field will affect the function of SDK,
* SDK will carry a flavor by default,EC_PROJECT_FLAVOR_FACTORY_INSTALLED_PRODUCTS_CN or
* EC_PROJECT_FLAVOR_FACTORY_INSTALLED_PRODUCTS_OVERSEA.
*/
enum ECProjectFlavor
{
	EC_PROJECT_FLAVOR_DEFAULT = 0,                                   ///< default value
	EC_PROJECT_FLAVOR_AFTER_MARKET_INSTALLED_PRODUCTS_CN,            ///< aftermarket installed products in China
	EC_PROJECT_FLAVOR_FACTORY_INSTALLED_PRODUCTS_CN,                 ///< factory-installed products in China
	EC_PROJECT_FLAVOR_FACTORY_INSTALLED_PRODUCTS_OVERSEA,            ///< factory-installed products oversea
	EC_PROJECT_FLAVOR_AFTER_MARKET_INSTALLED_PRODUCTS_OVERSEA        ///< aftermarket installed products oversea
};

enum ECBluetoothPolicy
{
	EC_BLUETOOTH_POLICY_SEND_DEFAULT_WITH_A2DP_CONNECTED_ONLY = 0x00,       			///<The app will send A2DP message to the HU while app come out to the system Mirroring ,and phone's A2DP connected to the HU.
	EC_BLUETOOTH_POLICY_SEND_DEFAULT_WITH_HFP_OR_A2DP_CONNECTED = 0x01, 				///<The app will send A2DP message to the HU while app come out to the system Mirroring, whether phone's A2DP connected or HFP connected to HU.
	EC_BLUETOOTH_POLICY_ALWAYS_ACCEPT_BTN = 0x02                                        ///<The app will response to btn events no matter phone's bluetooth is connected.
};


enum ECMirrorMode
{
	EC_MIRROR_MODE_DEFAULT = 0,                  ///< default mirror mode.
	EC_MIRROR_MODE_FIXED_NAVIGATION              ///< fixed navigation mirror mode.
};


/**
* @enum ECVideoType
*
* @brief video type
*
* @see ECSDK::ECMirrorConfig
*/
enum ECVideoType
{
    EC_VIDEO_TYPE_H264 = 0,                      ///< H264
    EC_VIDEO_TYPE_MPEG4,                         ///< MPEG4
    EC_VIDEO_TYPE_JPEG,                          ///< JPEG
    EC_VIDEO_TYPE_MAX,                           ///< reserve
};

/**
*
* @enum ECScreenDirection
*
* @brief HU screen direction
*/
enum ECScreenDirection
{
	EC_SCREEN_DIRECTION_UNKNOWN,						 ///< unknown screen direction
	EC_SCREEN_DIRECTION_HORIZONTAL,                    ///< horizontal screen direction
	EC_SCREEN_DIRECTION_VERTICAL,                      ///< vertical screen direction
	EC_SCREEN_DIRECTION_HORIZONTAL_ROUND,              ///< @deprecated
	EC_SCREEN_DIRECTION_VERTICAL_ROUND,                ///< @deprecated
};

/**
*
* @struct ECVideoArea
*
* @brief Display the area of the video screen
*/
struct ECVideoArea {
    uint8_t    present;
    uint16_t   originXPixels;
    uint16_t   originYPixels;
    uint16_t   widthPixels;
    uint16_t   heightPixels;

	ECVideoArea() : present(0), originXPixels(0), originYPixels(0), widthPixels(0), heightPixels(0) {}
	ECVideoArea(uint8_t present, uint16_t x, uint16_t y, uint16_t w, uint16_t h) :
		present(present), originXPixels(x), originYPixels(y), widthPixels(w), heightPixels(h) {}
};

/**
*
* @struct ECVideoView
*
* @brief Define the area that the window can display
*/
struct ECVideoView {
    uint16_t index;
    ECVideoArea viewArea;
    ECVideoArea safeArea;
	ECVideoView() :index(0) {}
	ECVideoView(uint16_t index, ECVideoArea viewArea, ECVideoArea safeArea) : index(index), viewArea(viewArea), safeArea(safeArea) {}
};


/*!
* @struct ECVideoViewConfig
*
* @brief Define viewport parameters
*/
struct ECVideoViewConfig{
    uint16_t initArea;
    vector<ECVideoView> viewGroup;    // array

	ECVideoViewConfig() : initArea(0) {}
};

/**
* @enum ECMirrorFeature
*
* @brief Define mirror feature
*/
enum ECMirrorFeature
{
	EC_MIRROR_FEATURE_DEFAULT = 0x00,      ///< no feature
	EC_MIRROR_FEATURE_HUD_ELEMENT_NORMAL = 0x01,      ///< HUD default layout
	EC_MIRROR_FEATURE_HUD_ELEMENT_SIMPLE = 0x02       ///< HUD simple layout
};
typedef enum ECMirrorFeature ECMirrorFeature;

/**
* @struct ECMirrorConfig
*
* @brief mirror config type
*
* @see ECSDK::openMirrorConnection
*/
struct ECMirrorConfig
{
    ECVideoType type;                            ///< video type
    int         width;                           ///< video width in pixels
    int         height;                          ///< video height in pixels
    int         quality;                         ///< video quality,
                                                 ///< bitrate for H264 or MPEG4(typically, 1024*1024*4)
                                                 ///< picture quality factor[0~100] for JPEG, 0 is worst, 100 is best. Typically, 50.
    int         touchMode;                       ///< touch mode : 0x0 Single-Touch ;0x01 Multi-touch;0x02 not support touch; Single-Touch is default mode.
    int         capScreenMode;                   ///< capture screen mode,0x00,default;0x01,not used;0x02,disable multi thread codec;0x04, use soft codec, developers should not fix the default value while you did not know what it means.
	ECScreenDirection screenDirection;			 ///< HU screen direction.
    ECVideoViewConfig viewConfig;
	double      screenPhysicsWidth;              ///< Physical screen width in inches
	double      screenPhysicsHeight;             ///< Physical screen height in inches
	ECMirrorFeature mirrorFeature;               ///< mirror feature

	string      reserve;                         ///< reserve(must be zero clearing)

    ECMirrorConfig() : type(EC_VIDEO_TYPE_H264), width(0), height(0),
        quality(0), touchMode(0), capScreenMode(0), screenDirection(EC_SCREEN_DIRECTION_UNKNOWN),
		screenPhysicsWidth(0), screenPhysicsHeight(0), mirrorFeature(EC_MIRROR_FEATURE_DEFAULT), reserve("") {}
};

struct ECVideoInfo
{
    uint32_t realWidth;                          ///< the real width of video frame in pixels.
    uint32_t realHeight;                         ///< the real height of video frame in pixels.
    uint32_t mirrorWidth;                        ///< the mirror width of video frame in pixels.
    uint32_t mirrorHeight;                       ///< the mirror height of video frame in pixels.
    int32_t  direction;                          ///< the direction of video frame. -1: video file stream, 0: up vertical screen, 3: down vertical screen,4: left horizontal screen,7:right horizontal screen.
};

/**
* @struct ECAudioInfo
*
* @brief audio data information
*
*/
struct ECAudioInfo
{
    uint32_t               sampleRate;            ///< sample rate
    ECAudioChannelType     channel;               ///< channel type
    ECAudioFormatType      format;                ///< audio format type
};

enum ECMusicStatus
{
	EC_MUSIC_STATUS_PLAYING = 1,                  ///< music is playing.
	EC_MUSIC_STATUS_PAUSED,                       ///< music was paused.
	EC_MUSIC_STATUS_STOPPED,                      ///< music was stopped it is not used yet.
	EC_MUSIC_STATUS_PENDING,                      ///< music is pending, it is not used yet.
	EC_MUSIC_STATUS_PLAYING_BY_SPEECH,			  ///< music is playing ,and trigered by voice assistant
};

struct ECAppMusicInfo
{
	ECMusicStatus	status;                         ///< the status of the song.
	string			title;                          ///< the name of the song.
	string			artist;                         ///< the artist of the song.
	string			album;                          ///< the album of the song.
	string			albumArtist;                    ///< the artist of the album.
	uint64_t		length;                         ///< the total time of the song, in ms.
};

enum ECCarCmdEFFECTType
{
	EC_CAR_CMD_TYPE_EFFECTIVE_GLOBAL = 0,			///< cmd is globally effective.
	EC_CAR_CMD_TYPE_EFFECTIVE_PAGE = 1,				///< cmd is the page in effect.
	EC_CAR_CMD_TYPE_EFFECTIVE_GLOBAL_NO_WAKEUP = 2, ///< cmd is globally effective and no need wakeup.
};

/**
* @struct ECCarCmd
*
* @brief the struct of voice command, the attribute of "cmd" in ECCarCmd can be regular expression,
*        such as "(打开|开启)空调", it equals "打开空调" or "开启空调".
*
* @see   ECSDKAudioRecorder::registerCarCmds, AudioRecorderListener::onCarCmdNotified
*/
struct ECCarCmd
{
	int32_t type;									///< type is just used in IECCallback::onCarCmdNotified, it tell car type of cmd effect, see enum ECCarCmdEFFECTType.
    string id;                                     ///< same function command can have same id.
	string cmd;                                    ///< the command content matched, the content can be regular expression.
	string vrText;                                 ///< vrText is just used in IECCallback::onCarCmdNotified, the mathched command content.
    bool   pauseMusic;                             ///< pauseMusic is just used in ECSDK::registerCarCmds, it tell the connected phone whether pause music when the command was triggered.
	int32_t responser;								///< responser means whether is CAR response, 0 car response else phone response.
	uint32_t thresholdLevel;						///< options parameter,it works while cmd type is EC_CAR_CMD_TYPE_EFFECTIVE_PAGE , default value 0, use the thresholdLevel by App, 1~9999 can be set by developers,
													///< The larger the value, the lower the recognition rate and the lower the false wake-up rate.
};



//bool enableTTS, bool enableVR, bool enableTalkie, bool enableMusic, bool autoChangeToBT = true

/**
* @struct ECTouchEventData
*
* @brief touch data struct
*
* @see ECTouchInfo
*/
struct ECTouchEventData
{
    uint16_t    pointX;                    ///< touch point x
	uint16_t    pointY;                    ///< touch point y
	uint16_t    slot;                      ///< multi touch slot(default is 0)
    string      reserve;                   ///< reserve

	ECTouchEventData() :pointX(0), pointY(0), slot(0) {}
};


/*!
 * \brief The ECOTAResourceVersion struct
 *
 * \see ECSDKAPP::ECOTAConfig
 */
struct ECOTAResourceVersion{
    string		softWareId;                         ///< Unique identification of resources
	uint32_t    softVersion;                            ///< Version of the local package
};


/*!
 * \brief The ECOTAConfig struct \ref   ECSDKAPP::ECSDKOTAUpdate::initialize
 *
 */
struct ECOTAConfig{
    string    configPath;                               ///< This path will save the config files, such as "aaaaa.2.req".
	string    packagePath;                              ///< This path will save the software package and md5 file and log file.
															///<software package such as "aaaaa.3.bin",
															///<md5 file such as "aaaaa.3.md5",
															///<log file such as "aaaaa.2.3.succ.log" or "aaaaa.2.3.err.log".
    vector<ECOTAResourceVersion> otaResourceVersionList;    ///< Upgrade the resource version list
};


enum ECOTAUpdateErrorCode
{
	EC_OTA_ERROR_SOFTWARE_DOWNLOAD_FAILED_VIA_NETWORK = -1,
	EC_OTA_ERROR_SOFTWARE_SPACE_NOT_ENOUGH = -2,
	EC_OTA_ERROR_SOFTWARE_NETWORK_UNAVAILABLE = -3,
	EC_OTA_ERROR_SOFTWARE_DOWNLOAD_FAILED_VIA_PHONE = -4
};

struct ECOTAUpdateSoftware
{
	string		softwareId;                       ///< id of the software.
	string		softwareName;                     ///< name of the software.	
	uint64_t    softwareSize;                     ///< size of the software.
	uint32_t    versionCode;                      ///< current version code of the software.
	string		vcDesc;                           ///< the description of the version code.
	string		vcTitle;                          ///< the title of current version software.
	string		vcDetail;                         ///< the detail of current version software.
	string		createTime;                       ///< the create time of current version software.
	string		modifyTime;                       ///< the modify time of current version software.
	uint8_t     isFullDist;                       ///< 1 means full package, 0 means incremental package.
	string		packagePath;                      ///< the path of the software in the HU.
	string		md5Path;                          ///< the path of the md5 file in the HU.
	string		iconPath;                         ///< the path of the icon in the HU.
	bool        isRemote;                         ///< true means software need to be downloaded from remote to phone.
};


/*!
 * \brief The \ref ECSDKAPP::IECProject::getECLogConfig function arguments wrapper.
 */

struct ECLogConfig
{
    ECLogLevel          logLevel;
    ECLogOutputType     logType;
    int32_t             logModule;
    string              logFile;     ///<  path of log file
};


enum ECPhoneType {
    EC_PHONE_TYPE_NONE = 0,
    EC_PHONE_TYPE_ANDROID,
    EC_PHONE_TYPE_IPHONE
};


/**
* @enum ECGearType
*
* @brief car gear type
*
* @see ECSDKVehicleInfo::uploadGearStatus
*/
enum ECGearType
{
    EC_GEAR_NEUTRUAL = 0,                        ///< neutral
    EC_GEAR_MANUAL_1st,                          ///< manual first
    EC_GEAR_MANUAL_2nd,                          ///< manual second
    EC_GEAR_MANUAL_3rd,                          ///< manual third
    EC_GEAR_MANUAL_4th,                          ///< manual forth
    EC_GEAR_MANUAL_5th,                          ///< manual fifth
    EC_GEAR_MANUAL_6th,                          ///< manual sixth
    EC_GEAR_MANUAL_7th,                          ///< manual seventh
    EC_GEAR_MANUAL_8th,                          ///< manual eighth
    EC_GEAR_MANUAL_9th,                          ///< manual ninth
    EC_GEAR_MANUAL_10th,                         ///< manual tenth

    EC_GEAR_AUTO_DRIVE = 100,                    ///< automatic drive
    EC_GEAR_AUTO_PARK,                           ///< automatic park
    EC_GEAR_AUTO_REVERSE,                        ///< automatic reverse
};

/**
* @enum ECDrivingStatus
*
* @brief driving status
*
* @see ECSDKVehicleInfo::uploadDrivingStatus
*/
enum ECDrivingStatus
{
    EC_DRIVING_FREE = 0x0,                       ///< no limited
    EC_DRIVING_NO_VIDEO = 0x01,                  ///< no video
    EC_DRIVING_NO_KEYBOARD_INPUT = 0x02,         ///< no keyboard input
    EC_DRIVING_NO_VOICE_INPUT = 0x04,            ///< no voice
    EC_DRIVING_NO_CONFIG = 0x08,                 ///< no config
};

/**
* @enum ECHeadLightStatus
*
* @brief car headlight status
*
* @see ECSDKVehicleInfo::uploadLightStatus
*/
enum ECHeadLightStatus
{
    EC_HEADLIGHT_OFF = 0,                        ///< headlight off
    EC_HEADLIGHT_ON,                             ///< headlight on
    EC_HEADLIGHT_HIGH,                           ///< high-beam on
};

/**
* @enum ECTurnIndicatorStatus
*
* @brief car's turning indicator status
*
* @see ECSDKVehicleInfo::uploadLightStatus
*/
enum ECTurnIndicatorStatus
{
    EC_TURNINDICATOR_NONE = 0,                   ///< indicator off
    EC_TURNINDICATOR_LEFT,                       ///< left-hand indicator on
    EC_TURNINDICATOR_RIGHT,                      ///< right-hand indicator on
};

/**
* @enum ECSensorType
*
* @brief car sensor type
*
* @see ECSDKVehicleInfo::uploadSensorError
*/
enum ECSensorType
{
    EC_SENSOR_LOCATION = 1,                      ///< sensor location
    EC_SENSOR_COMPASS,                           ///< sensor compass
    EC_SENSOR_SPEED,                             ///< sensor speed
    EC_SENSOR_RPM,                               ///< sensor rpm
    EC_SENSOR_ODOMETER,                          ///< sensor odometer
    EC_SENSOR_FUEL,                              ///< sensor fuel
    EC_SENSOR_PARKING_BRAKE,                     ///< sensor handbrake
    EC_SENSOR_GEAR,                              ///< sensor gear
    EC_SENSOR_NIGHT_MODE,                        ///< sensor night-mode
    EC_SENSOR_ENV_STATUS,                        ///< sensor car environment
    EC_SENSOR_DRIVING_STATUS,                    ///< sensor driving status
    EC_SENSOR_PASSENGER_STATUS,                  ///< sensor passenger status
    EC_SENSOR_DOOR_STATUS,                       ///< sensor door status
    EC_SENSOR_LIGHT_STATUS,                      ///< sensor light status
    EC_SENSOR_TIRE_PRESSURE_STATUS,              ///< sensor tire pressure status
    EC_SENSOR_ACCLEROMETER_STATUS,               ///< sensor accelerated status
    EC_SENSOR_GYROSCOPE_STATUS,                  ///< sensor gyroscopes status
    EC_SENSOR_GPS_SATELLITE_STATUS,              ///< sensor GPS
    EC_SENSOR_MAX                                ///< reserve
};

/**
* @enum ECSensorErrorType
*
* @brief cat sensor error type
*
* @see ECSDKVehicleInfo::uploadSensorError
*/
enum ECSensorErrorType
{
    EC_SENSORERROR_OK = 0,                       ///< sensor ok
    EC_SENSORERROR_TRANSIENT,                    ///< sensor transient error
    EC_SENSORERROR_PERMANENT,                    ///< sensor permanent error
};

/**
* The type of the editable content.
*/
enum ECInputType
{
	EC_INPUT_TYPE_NONE = 0x00000000,
	EC_INPUT_TYPE_TEXT = 0x00000001,
	EC_INPUT_TYPE_TEXT_CAP_CHARACTERS = 0x00001001,
	EC_INPUT_TYPE_TEXT_CAP_WORDS = 0x00002001,
	EC_INPUT_TYPE_TEXT_CAP_SEQUENCE = 0x00004001,
	EC_INPUT_TYPE_TEXT_AUTO_CORRECT = 0x00008001,
	EC_INPUT_TYPE_TEXT_AUTO_COMPLETE = 0x00010001,
	EC_INPUT_TYPE_TEXT_MULTI_LINES = 0x00020001,
	EC_INPUT_TYPE_TEXT_IME_MULTI_LINES = 0x00040001,
	EC_INPUT_TYPE_TEXT_NO_SUGGESTIONS = 0x00080001,
	EC_INPUT_TYPE_TEXT_URI = 0x00000011,
	EC_INPUT_TYPE_TEXT_EMAIL_ADDRESS = 0x00000021,
	EC_INPUT_TYPE_TEXT_EMAIL_SUBJECT = 0x00000031,
	EC_INPUT_TYPE_TEXT_SHORT_MESSAGE = 0x00000041,
	EC_INPUT_TYPE_TEXT_LONG_MESSAGE = 0x00000051,
	EC_INPUT_TYPE_TEXT_PERSON_NAME = 0x00000061,
	EC_INPUT_TYPE_TEXT_POSTAL_ADDRESS = 0x00000071,
	EC_INPUT_TYPE_TEXT_PASSWORD = 0x00000081,
	EC_INPUT_TYPE_TEXT_VISIBLE_PASSWORD = 0x00000091,
	EC_INPUT_TYPE_TEXT_WEB_EDIT_TEXT = 0x000000a1,
	EC_INPUT_TYPE_TEXT_FILTER = 0x000000b1,
	EC_INPUT_TYPE_TEXT_PHONETIC = 0x000000c1,
	EC_INPUT_TYPE_TEXT_WEB_EMAIL_ADDRESS = 0x000000d1,
	EC_INPUT_TYPE_TEXT_WEB_PASSWORD = 0x000000e1,
	EC_INPUT_TYPE_NUMBER = 0x00000002,
	EC_INPUT_TYPE_NUMBER_SIGNED = 0x00001002,
	EC_INPUT_TYPE_NUMBER_DECIMAL = 0x00002002,
	EC_INPUT_TYPE_NUMBER_PASSWORD = 0x00000012,
	EC_INPUT_TYPE_PHONE = 0x00000003,
	EC_INPUT_TYPE_DATETIME = 0x00000004,
	EC_INPUT_TYPE_DATE = 0x00000014,
	EC_INPUT_TYPE_TIME = 0x00000024
};

/**
* The type of the Input Method Editor (IME).
* imeOptions may be combined with variations and flags to indicate desired behaviors.
*/
enum ECInputImeOptions
{
	EC_INPUT_IME_ACTION_UNSPECIFIED = 0x00000000,
	EC_INPUT_IME_ACTION_NONE = 0x00000001,
	EC_INPUT_IME_ACTION_GO = 0x00000002,
	EC_INPUT_IME_ACTION_SEARCH = 0x00000003,
	EC_INPUT_IME_ACTION_SEND = 0x00000004,
	EC_INPUT_IME_ACTION_NEXT = 0x00000005,
	EC_INPUT_IME_ACTION_DONE = 0x00000006,
	EC_INPUT_IME_ACTION_PREVIOUS = 0x00000007,
	EC_INPUT_IME_FLAG_NO_PERSONALIZED_LEARNING = 0x01000000,
	EC_INPUT_IME_FLAG_NO_FULL_SCREEN = 0x02000000,
	EC_INPUT_IME_FLAG_NAVIGATE_PREVIOUS = 0x04000000,
	EC_INPUT_IME_FLAG_NAVIGATE_NEXT = 0x08000000,
	EC_INPUT_IME_FLAG_NO_EXTRACT_UI = 0x10000000,
	EC_INPUT_IME_FLAG_NO_ACCESSORY_ACTION = 0x20000000,
	EC_INPUT_IME_FLAG_NO_ENTER_ACTION = 0x40000000,
	EC_INPUT_IME_FLAG_FORCE_ASCII = 0x80000000
};

/**
* inputType see ECInputType.
* imeOptions see ECInputImeOptions.
* imeOptions can be combined with action and flag.
*/
struct ECInputInfo
{
	int32_t  inputType;
	int32_t  imeOptions;
	string   rawText;
	uint32_t minLines;
	uint32_t maxLines;
	uint32_t maxLength;
};

struct ECForwardInfo
{
	string		ip;
	uint32_t	localPort;
	uint32_t	remotePort;
};

enum ECVRTextType
{
	EC_VR_TEXT_FROM_UNKNOWN = 0,
	EC_VR_TEXT_FROM_VR,
	EC_VR_TEXT_FROM_SPEAK
};

struct ECVRTextInfo
{
	ECVRTextType type;
	int32_t      sequence;
	string       plainText;
	string       htmlText;
};

struct ECPageInfo
{
	int32_t  page;
	string   name;
	string   iconMd5;
};

struct ECPageInstallResult {
	int32_t page;
	int32_t installState;
	char note[256];
};

enum ECIconFormat
{
	EC_ICON_PNG = 1,
	EC_ICON_JPG = 2
};

struct ECIconInfo
{
	int32_t page;
	int32_t iconFormat;
	vector<uint8_t>  iconData;
};

enum ECQrAction
{
	EC_QR_ACTION_DEFAULT = 0x00, ///< we use it when we are not sure current connect type,
	EC_QR_ACTION_WIFI_AP_MODE_CUSTOMIZED = 0x01, ///< connect via wifi AP mode which customized and it can not access internet Normally.
	EC_QR_ACTION_WIFI_AP_MODE_ACCESS_INTERNET = 0x02, ///< connect via wifi AP mode which can access internet.
	EC_QR_ACTION_WIFI_STATION_MODE = 0x04, ///< connect via wifi Station mode
	EC_QR_ACTION_WIFI_P2P_MODE = 0x08, ///< connect via wifi P2P mode
	EC_QR_ACTION_USB_ANDROID = 0x10, ///< connect via Android USB
	EC_QR_ACTION_USB_IPHONE = 0x20,  ///< connect via iPhone USB
	EC_QR_ACTION_BT = 0x40  ///< connect via BT
};

struct ECQRInfo
{
	string ssid;
	string pwd;
	string auth;                              ///< The values of auth are such as WEP/WPA/WPA2/WPA3.
	string mac;                               ///< The mac address of Wifi-p2p netcard
	string name;                              ///< The Wifi-p2p name.
	string bm;                                ///< The mac address of car bt.
	int64_t action;                              ///< The type of current connection，see enum ECQrAction.
};

struct ECQrCodeParam {
	int32_t ecc;                                ///< ECC level of Qr Code. -1:default level; 0 ~ 3:7%、15%、25%、30% fault-tolerant
	uint32_t minVersion;                         ///< 1 <= minVersion < maxVersion <= 40
	uint32_t maxVersion;                         ///< 1 <= minVersion < maxVersion <= 40
	int32_t mask;                               ///< Qr Code mask.The mask is either between 0 to 7 to force that mask, or -1 to automatically choose an appropriate mask
};

struct ECQrCodeBmpParam {
	uint32_t dimension;                          ///< BMP resolution.Because the graph is square, you only need to specify a value.
	uint32_t ftColor;                            ///< Foreground color.Color of data section
	uint32_t bkColor;                            ///< Background color.Graphic background color
};

enum ECQrCodeType
{
    QR_CODE_TYPE_URL = 0x0,                     ///< generate qr code url
    QR_CODE_TYPE_BMP_FILE = 0x01,               ///< generate qr code bmp file
    QR_CODE_TYPE_BMP_DATA = 0x02                ///< generate qr code bmp data. Or operation can generate files and data at the same time.
};

enum ECErrorStatusCode
{
	EC_ERROR_NO_ERROR = 0,
	EC_ERROR_TIME_OUT = -1
};

/**
* @enum ECTransportType
*
* @brief transport type
*
*/
enum ECTransportType
{
	EC_TRANSPORT_ANDROID_USB_ADB = 0,            ///< android usb adb, system screen
	EC_TRANSPORT_ANDROID_USB_AOA,                ///< android usb aoa, app screen
	EC_TRANSPORT_ANDROID_WIFI,                   ///< android wifi, app screen
	EC_TRANSPORT_IOS_USB_EAP,                    ///< iphone usb eap, app screen
	EC_TRANSPORT_IOS_USB_MUX,                    ///< iphone usb mux, app screen
	EC_TRANSPORT_IOS_USB_AIRPLAY,                ///< iphone usb airplay, system screen
	EC_TRANSPORT_IOS_WIFI_APP,                   ///< iphone wifi app, app screen
	EC_TRANSPORT_IOS_WIFI_AIRPLAY,               ///< iphone wifi airplay, system screen
	EC_TRANSPORT_IOS_USB_LIGHTNING,              ///< iphone usb lightning connect(闪连)
	EC_TRANSPORT_MIRACAST,                       ///< wifi display
	EC_TRANSPORT_DLNA,                           ///< DLNA
	EC_TRANSPORT_BLE,							 ///< BLE
	EC_TRANSPORT_BTRFCOMM,                       ///< BT Rfcomm
	EC_TRANSPORT_MAX,                            ///< reserve
};

struct ECDevice
{
	ECTransportType transportType;
	char serial[128];
	char phoneIp[64];
};

enum ECTransportInfo
{
	EC_TRANSPORT_INFO_SPEECH_ENGINE = 1,				///< see enum ECSpeechEngineType
	EC_TRANSPORT_INFO_DISABLE_APP_VR = 2,               ///< see enum ECTransportCommConfig, value:0(enable),1 or others (disable),
};

/* value of ECTransportInfo key=EC_TRANSPORT_INFO_SPEECH_ENGINE */
enum ECSpeechEngineType
{
	EC_SPEECH_ENGINE_TXZ = 1,						///<  TXZ speech engine
	EC_SPEECH_ENGINE_IFLYTEK = 2,					///<  IFLYTEK speech engine free version
	EC_SPEECH_ENGINE_IFLYTEK_ADVANCED = 2001,		///<  IFLYTEK speech engine advanced version
	EC_SPEECH_ENGINE_IFLYTEK_PRO = 2002,  			///<  IFLYTEK speech engine professional version
};

/* value of ECTransportInfo key=EC_TRANSPORT_INFO_DISABLE_VR or others */
enum ECTransportCommConfig
{
	EC_TRANSPORT_INFO_COMM_VALUE_0 = 0,
	EC_TRANSPORT_INFO_COMM_VALUE_1 = 1,
};

enum ECWifiStateAction
{
	EC_WIFI_STATE_CHANGED_ACTION = 0,		 ///<AP, STATION
	EC_WIFI_P2P_STATE_CHANGED_ACTION = 1,    ///< P2P
};

enum ECWifiState
{
	EC_WIFI_STATE_UNKNOWN = 0,
	EC_WIFI_STATE_ENABLE,              ///< The network is available
	EC_WIFI_STATE_DISABLE,			  ///< Network unavailable
	EC_WIFI_STATE_CONNECTED,		      ///< Network connected
	EC_WIFI_STATE_DISCONNECTED,        ///< Network disconnection
};

struct ECNetWorkInfo
{
	int32_t		state;                 ///<see ECWifiState
	string		phoneIp;			   ///<phone ip
	string		carIp;				   ///<car ip
};

enum ECDeviceState
{
	EC_DEVICE_STATE_EAP_ATTACH = 0,
	EC_DEVICE_STATE_EAP_DETACH = 1,
};

/**
*
* @enum ECGPSInfo
*
* @brief phone gps infomation
*/
struct ECGPSInfo {
	double altitude;
	double speed;
	double course;
	uint64_t time;
	double longitude;
	double latitude;
};


/**
*
* @enum ECAPPHUDSupportFunction
*
* @brief APP HUD support function.
*
* @see enableDownloadPhoneAppHud
*/
enum ECAPPHUDSupportFunction {
	EC_APP_HUD_SUPPORT_FUNCTION_DEFAULT = 0,                    ///< no support function
	EC_APP_HUD_SUPPORT_FUNCTION_ROAD_JUNCTIONO_PICTURE = 1,      ///< support show road junction picture.
	EC_APP_HUD_SUPPORT_FUNCTION_LANE_GUIDANCE_PICTURE = 2,      ///< support show lane guidance picture.
};

/*
* @struct ECHudRoadJunctionPictureInfo
*
* @brief  road junction picture info
*
* @see   IECCallback::onPhoneAppHUDRoadJunctionPicture
*/
struct ECHudRoadJunctionPictureInfo {
	int8_t   status;                   ///< picture status, 0 mean car hide the picture. 1 mean car show the picture.
	int32_t  format;                   ///< picture format. @see ECIconFormat
	vector<uint8_t>   pictureData;     ///< picture data.
};

/*
* @struct ECHudLaneGuidancePictureInfo
*
* @brief  lane guidance picture info
*
* @see   IECCallback::onPhoneAppHUDLaneGuidancePicture
*/
struct ECHudLaneGuidancePictureInfo {
	int8_t   status;                   ///< picture status, 0 mean car hide the picture. 1 mean car show the picture.
	int32_t  format;                   ///< picture format. @see ECIconFormat
	vector<uint8_t>   pictureData;     ///< picture data.
};

enum ECAppPageStatus {
	EC_APP_PAGE_STATUS_UNKNOW,      ///< 未知状态
	EC_APP_PAGE_STATUS_OPEN,        ///< 打开状态
	EC_APP_PAGE_STATUS_CLOSE,       ///< 关闭状态
};

enum ECBTCallDataUploadState {
	EC_BT_CALL_DATA_UPLOAD_STATE_BEGIN,         ///< 开始上传
	EC_BT_CALL_DATA_UPLOAD_STATE_UPLOADING,     ///< 正在上传
	EC_BT_CALL_DATA_UPLOAD_STATE_END,           ///< 上传结束
};


enum ECBTCallDataType {
	EC_BT_CALL_DATA_TYPE_PHONE_BOOK,        ///< 电话簿
	EC_BT_CALL_DATA_TYPE_CALL_RECORDS,      ///< 通话记录
};


enum ECBTCallRecordsType {
	EC_BT_CALL_RECORD_TYPE_IN,              ///< 呼入
	EC_BT_CALL_RECORD_TYPE_OUT,             ///< 呼出
	EC_BT_CALL_RECORD_TYPE_IN_MISSED,       ///< 呼入未接
	EC_BT_CALL_RECORD_TYPE_OUT_MISSED,      ///< 呼出未接通
};

struct ECBTCallData {
	string					name;          ///< 联系人名
	string					number;         ///< 联系人号码
	string					location;       ///< 号码归属地
	string					callDateTime;   ///< 通话记录日期时间, format "yyyy-MM-dd HH:mm:ss"
	uint32_t				callDuration;   ///< 通话记录时长
	ECBTCallRecordsType		callType;       ///< 通话记录类型
};

struct ECPhoneNotification {
	int16_t				appIconFormat;  ///< 图标格式  @see enum ECIconFormat
	vector<uint8_t>		appIconData;	///< 图标数据.
	string				appName;        ///< 发起通知程序名称
	string				title;			///< 通知标题
	string				context;		///< 通知内容
	string				dateTime;		///< 通知发起的日期时间， format：dd.MM.yyyy HH:mm:ss:zzz
};

}

#endif // ECSDKTYPES_H

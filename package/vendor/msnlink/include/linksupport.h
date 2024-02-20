#ifndef LINKSUPPORT_H
#define LINKSUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    LINK_TYPE_UNKOW = 0x0,
    LINK_TYPE_AIRPLAY = 0x1,
    LINK_TYPE_CARPLAY = 0x2,
    LINK_TYPE_ANDROIDAUTO = 0x3,
    LINK_TYPE_ANDROIDLINK = 0x4,
    LINK_TYPE_CARLIFE = 0x5,
    LINK_TYPE_DLNA = 0x6,
    LINK_TYPE_HICAR = 0x7,
    LINK_TYPE_END
}LinkType;

typedef enum {
    LINK_AUDIOFOCUS_REQUEST_GAIN = 1,
    LINK_AUDIOFOCUS_REQUEST_GAIN_TRANSIENT = 2,
    LINK_AUDIOFOCUS_REQUEST_GAIN_TRANSIENT_MAY_DUCK = 3,
    LINK_AUDIOFOCUS_REQUEST_RELEASE = 4,
}LinkSDKAudioFocusType;

typedef enum {
    LINK_AUDIOFOCUS_STATE_LOSS = 0,
    LINK_AUDIOFOCUS_STATE_GAIN = 1,
    LINK_AUDIOFOCUS_STATE_LOSS_TRANSIENT = 2,
    LINK_AUDIOFOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK = 3,
}LinkSDKAudioFocusState;

#define LINKSDK_VERSION "V3.5.0_1107"

#define LINKSDK_HAS_BITMASK(value, bitmask) ((value & (bitmask)) == bitmask)

/*----------------------Audio Stream Type Defines----------------------------------------*/
#define kAudioStreamType_MainAudio		100 // Main audio
#define kAudioStreamType_AltAudio		101 // Alt audio
#define kAudioStreamType_AuxAudio		102 // Aux audio

/*----------------------Audio Type Defines----------------------------------------*/
#define kAudioStreamAudioTypeInt32_Alert					0x1
#define kAudioStreamAudioTypeInt32_Compatibility			0x2
#define kAudioStreamAudioTypeInt32_Default				    0x3
#define kAudioStreamAudioTypeInt32_Media					0x4
#define kAudioStreamAudioTypeInt32_SpeechRecognition		0x5
#define kAudioStreamAudioTypeInt32_Telephony				0x6


/*----------------------Glob Sink Notify Defines---------------------------------------------------*/
/**
   * @brief Link SDK version
   * @param linktype [in] LINK_TYPE_UNKOWN
   * @param lparam [in] reverse
   * @param cparam [in] version string
   * @note call check_sdk_license api notify this msg
   */
#define SINK_NOTIFY_SDK_VERSION                             0x1

/**
   * @brief Link SDK UUID
   * @param linktype [in] LINK_TYPE_UNKOWN
   * @param lparam [in] UUID len
   * @param cparam [in] UUID string
   * @note call check_sdk_license api notify this msg
   */
#define SINK_NOTIFY_SDK_UUID                                0x2

/**
   * @brief USB connect state change, lparam is device type
   * @param linktype [in] LINK_TYPE_UNKOWN
   * @param lparam [in] 0:disconnect 1:iphone 2:android
   * @note for usb carplay
   */
#define SINK_NOTIFY_USB_CONNECT_STATE_CHANGE                0x3


/**
   * @brief SDK initialized successfully
   * @param linktype [in] LINK_TYPE_UNKOWN
   */
#define SINK_NOTIFY_SDK_INITED                              0x4

#define SINK_NOTIFY_SDK_LICENSE_EXTDATAS                    0x5

/**
   * @brief Activation license status report
   * @param linktype [in] LINK_TYPE_UNKOWN
   * @param lparam [in] remaining activation quantity, if value is -1 activation error
   * @param cparam [in] error text
   * @note @see activation_sdk_license function
   */
#define SINK_NOTIFY_SDK_LICENSE_ACTIVATION                  0x6


/**
   * @brief Link service started notify
   */
#define SINK_NOTIFY_SERVICE_STARTED                         0x100

/**
   * @brief Link service stoped notify
   */
#define SINK_NOTIFY_SERVICE_STOPED                          0x101

/**
   * @brief Phone is connecting state notify
   * @param lparam [in] 1:usb  2:wireless
   */
#define SINK_NOTIFY_PHONE_CONNECTING                        0x102

/**
   * @brief Phone is connected state notify
   * @param lparam [in] 1:usb  2:wireless
   */
#define SINK_NOTIFY_PHONE_CONNECTED                         0x103

/**
   * @brief Phone is disconnected state notify
   * @param lparam [in] 1:usb  2:wireless
   */
#define SINK_NOTIFY_PHONE_DISCONNECTED                      0x104

/**
   * @brief Phone connecting error notify
   * @param lparam [in] 1:usb  2:wireless
   * @param sparam [in] error code
   */
#define SINK_NOTIFY_PHONE_CONNECTION_FAILED                 0x105

/**
   * @brief Phone is ready for wait connect
   * @param lparam [in] 1:usb  2:wireless
   * @param sparam [in] ip address
   * @note  only wireless carlife support
   */
#define SINK_NOTIFY_PHONE_CONNECTION_READY                  0x106

/**
   * @brief Video content size
   * @param lparam [in] Video content size; 64bit data -> |16bit left margin|16bit top margin|16bit width|16bit height|
   * @note android auto video is a fixed size and needs to stretch the centent size to full screen
   */
#define SINK_NOTIFY_VIDEO_CONTENT_SIZE                      0x107


#define SINK_NOTIFY_GOTO_BACKGROUND                         0x108

#define SINK_NOTIFY_GOTO_FOREGROUND                         0x109

/**
   * @brief Video stream encoder size
   * @param lparam [in] 16bit width|16bit height
   * @note use for carplay, android auto
   */
#define SINK_NOTIFY_VIDEO_ENCODER_SIZE                      0x10A


/**
   * @brief Click home icon
   * @note for carplay, android auto
   */
#define SINK_NOTIFY_GOTO_HOME                               0x110

#define LINK_RESOURCE_MASK_SCREEN                     (1 << 0)
#define LINK_RESOURCE_MASK_AUDIO                      (1 << 1)
#define LINK_RESOURCE_MASK_VOICESPEECH                (1 << 2)
#define LINK_RESOURCE_MASK_TELPHONE                   (1 << 3)
#define LINK_RESOURCE_MASK_NAVI                       (1 << 4)
#define LINK_RESOURCE_MASK_VOICESPEECH_SPEEK          (1 << 5)

/**
   * @brief Link Resource modes changed
   * @param lparam [in] bit0:screen bit1:mainAudio bit2:voice speech bit3:telphone call bit4:navigation turns bit5:voice speaking
   */
#define SINK_NOTIFY_RESOURCE_MODES_CHANGE                   0x111

/**
   * @brief Bluetooth pairing request
   * @param lparam [in] reversing
   * @param cparam [in] bluetooth address
   * @note Pairing response to call action @see LINK_ACTION_BT_PAIRING_RESPONSE
   */
#define SINK_NOTIFY_BT_PAIRING_REQUEST                      0x112

/**
   * @brief MFI error notify
   * @param lparam [in] 1:Detect MFI chip failed 2:Read certificate failed 3:Create signature failed
   */
#define SINK_NOTIFY_MFI_ERROR                               0x113


/**
   * @brief Request audio focus
   * @param lparam [in] request audio focus type @LinkSDKAudioFocusType
   */
#define SINK_NOTIFY_REQUEST_AUDIOFOCUS                      0x114

/**
   * @brief Request duck media audio volume
   * @param lparam [in] 0:unduck 1:duck
   * @note carplay navi audio use duck notify
   */
#define SINK_NOTIFY_REQUEST_DUCK_MEDIA_VOLUME               0x115

/**
   * @brief Bluetooth auth result
   * @param lparam [in] status The authentication status
   * @param cparam [in] reverse
   * @note  status value list
   * STATUS_SUCCESS = 0
   * STATUS_BLUETOOTH_PAIRING_DELAYED = -10,
   * STATUS_BLUETOOTH_UNAVAILABLE = -11,
   * STATUS_BLUETOOTH_INVALID_ADDRESS = -12,
   * STATUS_BLUETOOTH_INVALID_PAIRING_METHOD = -13,
   * STATUS_BLUETOOTH_INVALID_AUTH_DATA = -14,
   * STATUS_BLUETOOTH_AUTH_DATA_MISMATCH = -15,
   * STATUS_BLUETOOTH_HFP_ANOTHER_CONNECTION = -16,
   * STATUS_BLUETOOTH_HFP_CONNECTION_FAILURE = -17,
   * @note  @LINK_ACTION_BT_PAIRING_AUTHDATA
   * @note  Once this function is called, the head unit must accept or reject the
   * authentication data for the current Bluetooth pairing request.
   *
   *
   * Whenever the head unit has called |BluetoothEndpoint::sendAuthData|, it
   * must not complete the pairing's authentication until this callback
   * function has been called. This is necessary to ensure that the head unit
   * has paired with the correct mobile device.
   *
   *
   * Successful authentication is indicated when a value of STATUS_SUCCESS is
   * passed to this function, and STATUS_BLUETOOTH_INVALID_AUTH_DATA or
   * STATUS_BLUETOOTH_AUTH_DATA_MISMATCH indicate common failure modes.
   * Any status other than STATUS_SUCCESS must be treated as an authentication
   * failure.
   *
   *
   * Any authentication result received within 60 seconds from the time the
   * head unit has called the |BluetoothEndpoint::onReadyForPairing| function
   * shall be handled normally by this function. When 60 seconds have passed,
   * the head unit must discard the authentication result, treating it as
   * though the mobile device has reported the authentication has failed.
   */
#define SINK_NOTIFY_BT_AUTH_RESULT                         0x116


/**
   * @brief Request Link Navigation focus notify
   * @param lparam [in] reverse
   * @param cparam [in] reverse
   * @note  Recv this notify must stop native navigation
   * @note  @see LINK_ACTION_SET_NAVIGATION_FOCUS
   */
#define SINK_NOTIFY_REQUEST_NAVIGATION_FOCUS               0x117


/**
   * @brief Phone call status change
   * @param lparam [in] reverse
   * @param cparam [in] json datas
   * @note  json string format : {
    "num_calls": 0,
    "signal_strength": 0,
    "calls": [{
        "state": 0,
        "caller_number": "",
        "caller_id": "",
        "caller_number_type": "",
        "call_duration_seconds": 0,
        "caller_thumbnail": ""
    }]
}
   */
#define SINK_NOTIFY_PHONE_CALL_STATUS_CHANGE               0x118

/**
   * @brief Connected phone infos notify
   * @param lparam [in] reverse
   * @param cparam [in] phone name
   */
#define SINK_NOTIFY_CONNECTED_PHONE_INFOS                  0x119

/**
   * @brief HiCar sdk send datas to bluetooth
   * @param lparam [in] reverse
   * @param cparam [in] AT command
   */
#define SINK_NOTIFY_HICAR_SEND_BTDATAS                     0x120

/**
   * @brief Recv phone date time
   * @param lparam [in] reverse
   * @param cparam [in] yyyy-MM-dd hh:mm:ss
   */
#define SINK_NOTIFY_RECV_PHONE_DATETIME                    0x121


/**
   * @brief Recv HiCar pin code
   * @param lparam [in] code len
   * @param cparam [in] pin code
   * @note len > 0 is succsessed
   */
#define SINK_NOTIFY_RECV_HICAR_PIN_CODE                    0x122

/**
   * @brief Recv HiCar QR code
   * @param lparam [in] url len
   * @param cparam [in] url datas
   * @note len > 0 is succsessed
   */
#define SINK_NOTIFY_RECV_HICAR_QR_CODE                     0x123


/**
   * @brief Wireless carplay iAP2 session connect state
   * @param lparam [in] 0:disconnected 1:connected
   * @param cparam [in] iAP2Link_t instance point
   * @note  use for carplay dongle
   */
#define SINK_NOTIFY_WLCP_IAP2_CONN_STATE                    0x200

/**
   * @brief Recv wireless carplay session iAP2 Message
   * @param lparam [in] (16 bit session type) | (16 bit message len)
   * @param cparam [in] message datas
   * @note  use for carplay dongle
   */
#define SINK_NOTIFY_WLCP_IAP2_RECV_MESSAGE                  0x201

/*----------------------DLNA Service Sink Notify Defines----------------------------------------*/
/**
   * @brief Strat play
   * @param lparam [in] reversing
   * @param cparam [in] reversing
   * @note The url is from @see SINK_NOTIFY_DLNA_PLAY_SETURL @SINK_NOTIFY_DLNA_PLAY_SETNEXTURL
   */
#define SINK_NOTIFY_DLNA_PLAY_START           0x300

/**
   * @brief Stop play
   * @param lparam [in] reversing
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_PLAY_STOP            0x301

/**
   * @brief Seek video
   * @param lparam [in] seek position (Second)
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_PLAY_SEEK            0x302

/**
   * @brief Pause video
   * @param lparam [in] reversing
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_PLAY_PAUSE            0x303

/**
   * @brief Set play url
   * @param lparam [in] reversing
   * @param cparam [in] play url
   */
#define SINK_NOTIFY_DLNA_PLAY_SETURL           0x304

/**
   * @brief Set next play url
   * @param lparam [in] reversing
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_PLAY_SETNEXTURL       0x305


/**
   * @brief Set volume
   * @param lparam [in] volume (0-100)
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_SET_VOLUME            0x307

/**
   * @brief Set next play url
   * @param lparam [in] 0:unmute 1:mute
   * @param cparam [in] reversing
   */
#define SINK_NOTIFY_DLNA_SET_MUTE              0x308





/*----------------------Glob Link Action Defines---------------------------------------------------*/
/**
   * @brief Reset display size
   * @param lparam [in] bit63-47:screen_height_mm|bit47-31:screen_width_mm|bit31-15:display_height|bit15-0:display_width
   * @note  The set display size must be restarted service to take effect
   */
#define LINK_ACTION_RESET_DISPLAY_SIZE                          0x3

/**
   * @brief Start connection
   * @param lparam [in] 1:usb 2:wireless 3:wireless carplay not start whit bt iap
   * @param sparam [in] phone bluetooth address for wireless carplay
   * @note  Only carplay, android auto, carlife support
   */
#define LINK_ACTION_START_PHONE_CONNECTION                      0x4

/**
   * @brief Stop connection
   * @param lparam [in] reverse
   * @note  Link service support list:carplay, android auto, carlife
   */
#define LINK_ACTION_STOP_PHONE_CONNECTION                       0x5


/**
   * @brief Video resource controller
   * @param lparam [in] 0:stop video 1:start video
   * @note Used for screen focus loss, only carplay,android auto, carlife support
   */
#define LINK_ACTION_VIDEO_CTRL                                  0x10

/**
   * @brief Audio resource controller
   * @param lparam [in] 0:stop audio 1:start audio
   * @note Used for media audio focus loss, only carplay,android auto, carlife support
   */
#define LINK_ACTION_AUDIO_CTRL                                  0x11

/**
   * @brief Set phone link day\ngiht mode
   * @param lparam [in] 0:day  1:night
   * @note only carplay\android auto support
   */
#define LINK_ACTION_CHANGE_DAY_NIGHT_MODE                       0x12

/**
   * @brief Request h264 keyframe, frame include sps + pps + idr
   * @param reverse
   * @note only carplay support
   */
#define LINK_ACTION_REQUEST_KEYFRAME                            0x13

/**
   * @brief Media audio focus change
   * @param lparam [in] bit23-16:streamType|bit15-8:audioType|bit7-0:focuseType
   * @note  only android platform support
   *        focusType:0:AUDIOFOCUS_LOSS 1:AUDIOFOCUS_GAIN 2:AUDIOFOCUS_LOSS_TRANSIENT 3:AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK
   */
#define LINK_ACTION_MEDIA_AUDIOFOCUS_CHANGE                     0x14

/**
   * @brief Set driver position
   * @param lparam [in] 0:left  1:right
   * @note only carplay\android auto support, next connection takes effect
   */
#define LINK_ACTION_CHANGE_DRIVER_POSITION                      0x15


/**
   * @brief Request reopen media output device
   * @note Request to reopen the audio output device when the audio output device changes
   */
#define LINK_ACTION_REQUEST_REOPEN_OUTPUT_DEVICE                0x16

/**
   * @brief Reset video max fps
   * @param lparam [in] 30 or 60 (value >= 60 ? 60 : 30)
   * @note The next connection takes effect
   */
#define LINK_ACTION_RESET_VIDEO_MAX_FPS                          0x17


#define NATIVE_RESOURCE_MASK_SCREEN                     (1 << 0)
#define NATIVE_RESOURCE_MASK_AUDIO                      (1 << 1)
#define NATIVE_RESOURCE_MASK_VOICESPEECH                (1 << 2)
#define NATIVE_RESOURCE_MASK_TELPHONE                   (1 << 3)
#define NATIVE_RESOURCE_MASK_NAVI                       (1 << 4)
#define NATIVE_RESOURCE_MASK_VOICESPEECH_SPEEK          (1 << 5)
#define NATIVE_RESOURCE_MASK_REVERSING                  (1 << 6)
#define NATIVE_RESOURCE_MASK_AUDIOOFF                   (1 << 7)
#define NATIVE_RESOURCE_MASK_HANDBREAK                  (1 << 8)
#define NATIVE_RESOURCE_MASK_BTMUSIC                    (1 << 9)
#define NATIVE_RESOURCE_MASK_AUDIO_LOSTTRANSIENT        (1 << 23)

/**
   * @brief Native Resource modes changed
   * @param lparam [in] bit0:screen bit1:mainAudio bit2:voice speech bit3:telphone call bit4:navigation turns bit5:voice tts speaking bit6:car reversing bit7:audio off bit8:handbreak bit9:bt music
   */
#define LINK_ACTION_NATIVE_RESOURCE_MODES_CHANGE                 0x18

/**
   * @brief Disable CarPlay\AndroidAuto audio output
   * @param lparam [in] 0:enable 1:disable
   * @note if disable audio output, link audio output to phone or bluetooth
   */
#define LINK_ACTION_DISABLE_AUDIO_OUTPUT                         0x19

/**
   * @brief Request reopen mic record device
   * @note Request to reopen the mic record device when the mic record device changes
   */
#define LINK_ACTION_REQUEST_REOPEN_MICRECORD_DEVICE              0x1A


/**
   * @brief Set the phone address for Bluetooth connection
   * @param sparam [in] phone bluetooth address, if param is empty clear connected phone address
   */
#define LINK_ACTION_SET_CONNECTED_BT_ADDRESS                     0x1B

/**
   * @brief Set device name
   * @param sparam [in] device name, max len 64 byte
   * @note  Set device name for all service
   */
#define LINK_ACTION_SET_DEVICE_NAME                              0x100

/**
   * @brief Set OEM label
   * @param sparam [in] OEM label, max len 64 byte
   * @note  Set OEM label for return icon display name
   */
#define LINK_ACTION_SET_OEM_LABEL                                0x101

/**
   * @brief Set hotspot name
   * @param sparam [in] hotspot name, max len 64 byte
   * @note  Set hotspot name for wireless carplay,auto connection
   */
#define LINK_ACTION_SET_HOTSPOT_NAME                             0x102

/**
   * @brief Set hotspot password
   * @param sparam [in] hotspot password, max len 32 byte
   * @note  Set hotspot password for wireless carplay,auto connection
   */
#define LINK_ACTION_SET_HOTSPOT_PSK                              0x103


/**
   * @brief enable carplay auido
   * @param lparam [in] 1:enable 0:disable
   * @note  must restart carplay service to take effect
   */
#define LINK_ACTION_ENABLE_CP_AUDIO                              0x104

/**
   * @brief remove carplay paired device
   * @param sparam [in] phone bt address, address format 1c:1b:0d:7f:92:1e
   * @note If sparam is NULL or empty string, it will remove all paired devices
   */
#define LINK_ACTION_REMOVE_CP_PAIREDDEV                            0x105


/**
   * @brief report gps location datas to phone
   * @param lparam [in] (latitudeE7 << 32) | longitudeE7
   * @param sparam [in] accuracyE3=xx&altitudeE2=xx&speedE3=xx&bearingE6=xx
   * @note latitudeE7 Latitude value [-90.0, +90.0] multiplied by 1e7
   * @note longitudeE7 Longitude value [-180.0, +180.0) multiplied by 1e7
   * @note android auto sparam If there are no corresponding parameters, do not set the key value, xx is the value
   * @note android auto example, the value 3.1415 becomes 31415000 in E7 notation and can be represented as an integer
   * @note carplay lparam is 0, sparam is GPRMC string($GPRMC,........)
   */
#define LINK_ACTION_REPORT_GPS_LOCATION                            0x106


/**
   * @brief report gps satellite data
   * @param lparam [in] reverse
   * @param sparam [in] $GPGSV string list
   * @note only used for android auto
   */
#define LINK_ACTION_REPORT_GPS_STATELLITE_STATUS                   0x107

/**
   * @brief Response bt pairing request @see SINK_NOTIFY_BT_PAIRING_REQUEST
   * @param lparam [in] (status << 32) | alreadyPaired
   * @param cparam [in] reverse
   * @note  status Can be |STATUS_BLUETOOTH_UNAVAILABLE| or |STATUS_SUCCESS| or |STATUS_BLUETOOTH_PAIRING_DELAYED|.
   *        alreadyPaired Whether the head unit has link key (pairing information). Ignored if |status| is not |STATUS_SUCCESS|.
   *        alreadyPaired |1| if and only if the head unit already has link key (pairing information) for the phone (In other words, if the head unit thinks it's paired with the phone)
   */
#define LINK_ACTION_BT_PAIRING_RESPONSE                            0x117

/**
   * @brief Send bt pairing auth data
   * @param lparam [in] pairingMethod
   * @param cparam [in] The authentication data (For example, a numeric passkey or a PIN).
   * @note pairingMethod @see SINK_NOTIFY_BT_PAIRING_REQUEST
   */
#define LINK_ACTION_BT_PAIRING_AUTHDATA                            0x118

/**
   * @brief Set navigation focus
   * @param lparam [in] 1:native navigation 2:link navigation
   * @param cparam [in] reverse
   * @note @see SINK_NOTIFY_REQUEST_NAVIGATION_FOCUS
   */
#define LINK_ACTION_SET_NAVIGATION_FOCUS                           0x119

/**
   * @brief Report driving status
   * @param lparam [in] status
   * @param cparam [in] reverse
   * @note status value list:
   *   DRIVE_STATUS_UNRESTRICTED = 0,
   *   DRIVE_STATUS_NO_VIDEO = 1,
   *   DRIVE_STATUS_NO_KEYBOARD_INPUT = 2,
   *   DRIVE_STATUS_NO_VOICE_INPUT = 4,
   *   DRIVE_STATUS_NO_CONFIG = 8,
   *   DRIVE_STATUS_LIMIT_MESSAGE_LEN = 16
   */
#define LINK_ACTION_REPORT_DRIVING_STATUS                          0x120


/**
   * @brief Report parking brake status
   * @param lparam [in] 0:parking brake is not engaged 1:parking brake is engaged
   * @param cparam [in] reverse
   */
#define LINK_ACTION_REPORT_PARKINGBRAKE_STATUS                     0x121


/**
   * @brief Reports an action associated with current phone status
   * @param lparam [in] action
   * @param cparam [in] caller_number + '\n' + caller_id
   * @note caller_number The number of the call to act upon. Can be empty.
   * @note caller_id The caller id to act upon. Can be empty.
   * @note action values:
   * InstrumentClusterInput_InstrumentClusterAction_UP = 1,
   * InstrumentClusterInput_InstrumentClusterAction_DOWN = 2,
   * InstrumentClusterInput_InstrumentClusterAction_LEFT = 3,
   * InstrumentClusterInput_InstrumentClusterAction_RIGHT = 4,
   * InstrumentClusterInput_InstrumentClusterAction_ENTER = 5,
   * InstrumentClusterInput_InstrumentClusterAction_BACK = 6,
   * InstrumentClusterInput_InstrumentClusterAction_CALL = 7
   */
#define LINK_ACTION_REPORT_PHONE_STATUS                            0x122


/**
   * @brief Use this action to report which gear the vehicle is in. The value of this
   * sensor may be used to determine which UI elements can be interacted with
   * and which ones get locked out. Additionally, these values might be used
   * informationally
   * @param lparam [in] gear
   * @param cparam [in] reverse
   * @note action values:
   * enum Gear : int {
   *    GEAR_NEUTRAL = 0,
   *    GEAR_1 = 1,
   *    GEAR_2 = 2,
   *    GEAR_3 = 3,
   *    GEAR_4 = 4,
   *    GEAR_5 = 5,
   *    GEAR_6 = 6,
   *    GEAR_7 = 7,
   *    GEAR_8 = 8,
   *    GEAR_9 = 9,
   *    GEAR_10 = 10,
   *    GEAR_DRIVE = 100,
   *    GEAR_PARK = 101,
   *    GEAR_REVERSE = 102
   * };
   */
#define LINK_ACTION_REPORT_GEAR_DATA                               0x123

/**
   * @brief Use this action to report data about wheel speed and steering angle.
   * @param lparam [in] (steeringAngleE1 << 8) | hasSteeringAngle
   * @param cparam [in] wheelSpeedE3 An array of wheel speeds in m/s multiplied by 1e3. Start from the front left and go clockwise.
   * @note hasSteeringAngle(byte 8bits) Is the steeringAngleE1 param valid
   * @note steeringAngleE1(int 32bits) Steering angle in tenths of a degree multiplied by 10. Left is negative.
   * @note wheelSpeedE3 values are separated by '|', such as "value1|value2|value3|value4"
   */
#define LINK_ACTION_REPORT_WHEEL_DATAS                             0x124


/**
   * @brief Use this function to report the current speed of the vehicle. The value reported here might be used in dead reckoning the position of the vehicle in the event of a GPS signal loss.
   * @param lparam [in] bit50 hasCruiseEngaged|bit49 cruiseEngaged|bit48 hasCruiseSetSpeed|bit32-47 cruiseSetSpeed|bit31-0 speedE3
   * @param cparam [in] $PASCD NEMA strings, if is empty string use lparam speedE3 value to make PASCD NEMA string
   * @note speedE3 The speed in m/s signed velocity multiplied by 1e3.
   * @note hasCruiseEngaged True if cruise engaged paramater is valid.
   * @note cruiseEngaged Whether or not cruise controll is engaged.
   * @note hasCruiseSetSpeed True if cruise set speed paramter is valid.
   * @note cruiseSetSpeed The speed the cruise control is set at, in m/s.
   * @note cparam used for carplay
   */
#define LINK_ACTION_REPORT_SPEED_DATAS                             0x125

/**
   * @brief Set audio focus state to android auto
   * @param lparam [in] focus state
   * @param cparam [in] reverse
   * @note only used for android auto
   * @note Audo focus state values:
   * AUDIO_FOCUS_STATE_GAIN = 1,
   * AUDIO_FOCUS_STATE_GAIN_TRANSIENT = 2,
   * AUDIO_FOCUS_STATE_LOSS = 3,
   * AUDIO_FOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK = 4,
   * AUDIO_FOCUS_STATE_LOSS_TRANSIENT = 5,
   * AUDIO_FOCUS_STATE_GAIN_MEDIA_ONLY = 6,
   * AUDIO_FOCUS_STATE_GAIN_TRANSIENT_GUIDANCE_ONLY = 7
   */
#define LINK_ACTION_SETAAUTO_AUDIO_FOCUS                           0x126

/**
   * @brief Set ui appearance
   * @param lparam [in] (AppearanceSetting << 16)|AppearanceMode
   * @param cparam [in] reverse
   * @note only used for carplay
   * AppearanceMode values 0:AppearanceMode_Light 1:AppearanceMode_Dark
   * AppearanceSetting values:0:AppearanceSetting_Automatic	1:AppearanceSetting_UserChoice 2:AppearanceSetting_Always
   */
#define LINK_ACTION_SET_UI_APPEARANCE                              0x127

/**
   * @brief Set map appearance
   * @param lparam [in] (AppearanceSetting << 16)|AppearanceMode
   * @param cparam [in] reverse
   * @note only used for carplay
   * AppearanceMode values 0:AppearanceMode_Light 1:AppearanceMode_Dark
   * AppearanceSetting values:0:AppearanceSetting_Automatic	1:AppearanceSetting_UserChoice 2:AppearanceSetting_Always
   */
#define LINK_ACTION_SET_MAP_APPEARANCE                             0x128


/**
   * @brief Enable HiCar ble broadcast
   * @param lparam [in] 0:stop hicar adv  1:start hicar adv
   * @param cparam [in] reverse
   */
#define LINK_ACTION_HICAR_ENABLE_ADV                                0x129

/**
   * @brief Recv bluetooth br datas send to HiCarSDK
   * @param lparam [in] reverse
   * @param cparam [in] AT command
   */
#define LINK_ACTION_HICAR_RECV_BRDATAS                              0x12A

/**
   * @brief Get HiCar QRCord
   * @param lparam [in] reverse
   * @param cparam [in] reverse
   */
#define LINK_ACTION_HICAR_GET_QR_CODE                               0x12B


/**
   * @brief Update played media infos
   * @param lparam [in] media total duration (unit second)
   * @param sparam [in] media title
   * @note  When recv SINK_NOTIFY_DLNA_PLAY_START call this action sync media info
   */
#define LINK_ACTION_DLNA_UPDATE_MEDIAINFO                        0x200

/**
   * @brief Update played position
   * @param lparam [in] media played time (unit second)
   * @note  If is playing state, call this action every second
   */
#define LINK_ACTION_DLNA_UPDATE_POSITION                         0x201


/**
   * @brief Update playe state
   * @param lparam [in] play state(0:stop 1:playing 2:pause, 3:play end)
   * @note  When recv SINK_NOTIFY_PLAY_START\STOP\PAUSE call this action to sync play state
   */
#define LINK_ACTION_DLNA_UPDATE_PLAYSTATE                        0x202

/**
   * @brief Update volume
   * @param lparam [in] volume (0-100)
   * @note  When recv SINK_NOTIFY_DLNA_SET_VOLUME call this action to sync volume
   */
#define LINK_ACTION_DLNA_UPDATE_VOLUME                           0x203

/**
   * @brief Update mute state
   * @param lparam [in] mute state(0:unmute 1:mute)
   * @note  When recv SINK_NOTIFY_DLNA_SET_MUTE call this action to sync mute state
   */
#define LINK_ACTION_DLNA_UPDATE_MUTE                             0x204

/*----------------------KeyCode Defines----------------------------------------*/
#define KEY_CODE_KNOB_LEFT                                      0x100
#define KEY_CODE_KNOB_RIGHT                                     0x101
#define KEY_CODE_LEFT                                           0x102
#define KEY_CODE_RIGHT                                          0x103
#define KEY_CODE_UP                                             0x104
#define KEY_CODE_DOWN                                           0x105

#define KEY_CODE_ENTER                                          0x120
#define KEY_CODE_BACK                                           0x121
#define KEY_CODE_HOME                                           0x122

#define KEY_CODE_MEDIA_PLAY                                     0x150
#define KEY_CODE_MEDIA_PAUSE                                    0x151
#define KEY_CODE_MEDIA_TOGGLE_PLAY_PAUSE                        0x152
#define KEY_CODE_MEDIA_NEXT                                     0x153
#define KEY_CODE_MEDIA_PREV                                     0x154

#define KEY_CODE_TEL_ACCEPT                                     0x200
#define KEY_CODE_TEL_REJECT                                     0x201
#define KEY_CODE_TEL_TOGGLE_ACCEPT_REJECT                       0x202
#define KEY_CODE_TEL_MUTE_MIC                                   0x203

#define KEY_CODE_VOICE                                          0x250
#define KEY_CODE_MAP                                            0x251
#define KEY_CODE_MUSIC                                          0x252
#define KEY_CODE_PHONE                                          0x253
#define KEY_CODE_VOICE_LONGPRESS                                0x254   // for android auto long press asr


/**
   * @brief Video start callback
   */
typedef void (* sink_video_start_cb)(LinkType type);

/**
   * @brief Video frame recv callback
   * @param datas [in] h264 video frame datas
   * @param len[in] frame data len
   * @param idrFrame[in] is idr frame(has sps nal and pps nal)
   */
typedef void (* sink_video_play_cb)(LinkType type, void * datas, int len, bool idrFrame);

/**
   * @brief Video stop callback
   */
typedef void (* sink_video_stop_cb)(LinkType type);

/**
   * @brief Audio start callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param rate [in] audio sample rate
   * @param format[in] audio format usually is 16 (PCM_FORMAT_S16_LE)
   * @param channel[in] audio channel
   */
typedef void (* sink_audio_start_cb)(LinkType type, int streamType, int audioType, int rate, int format, int channel);

/**
   * @brief Audio frame recv callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param datas [in] pcm frame datas
   * @param len[in] frame data len (is not frame size)
   */
typedef void (* sink_audio_play_cb)(LinkType type, int streamType, int audioType, void * datas, int len);

/**
   * @brief Audio stop callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   */
typedef void (* sink_audio_stop_cb)(LinkType type, int streamType, int audioType);

/**
   * @brief mic input start callback
   * @param rate [in] audio sample rate
   * @param format[in] audio format usually is 16 (PCM_FORMAT_S16_LE)
   * @param channel[in] audio channel
   * @note use @see send_microphone_datas to write mic datas
   */
typedef void (* sink_micinput_start_cb)(LinkType type, int rate, int format, int channel);

/**
   * @brief mic input stop callback
   */
typedef void (* sink_micinput_stop_cb)(LinkType type);

/**
   * @brief Link state notify
   * @param linkType[in] @see LinkType
   * @param type[in] @see SINK_NOTIFY_XXX define
   * @param lparam [in] reserved, action param @see SINK_NOTIFY_XXX
   * @param sparam [in] reserved, action param @see SINK_NOTIFY_XXX
   */
typedef void (* sink_notify_cb)(LinkType linkType, int eventType, int64_t lparam, const char * sparam);

typedef struct {
    sink_video_start_cb video_start;
    sink_video_play_cb  video_play;
    sink_video_stop_cb  video_stop;

    sink_audio_start_cb audio_start;
    sink_audio_play_cb  audio_play;
    sink_audio_stop_cb  audio_stop;

    sink_micinput_start_cb micinput_start;
    sink_micinput_stop_cb  micinput_stop;

    sink_notify_cb      sink_notify;

    void *              sink_context;    // reserved
} link_player_sink;

/**
   * @brief check sdk license
   * @param license [in] license data
   * @param len [in] license data len
   * @return 0:sucessed
   */
int check_sdk_license(const char * license, int len);

/**
   * @brief activation link sdk license use network
   * @param sign [in] user sign
   * @return 0:success to call
   * @note activation status wait for @see SINK_NOTIFY_SDK_LICENSE_ACTIVATION
   */
int activation_sdk_license(const char * sign);

/**
   * @brief init link sdk
   * @param sink [in] @see link_player_sink
   * @param btAddress [in] bluetooth adddress, format 1c:1b:0d:7f:92:1e
   * @param width[in] display video output width
   * @param height[in] display video output height
   * @param widthMM[in] screen physical width unit MM
   * @param heightMM[in] screen physical height unit MM
   * @return 0:sucessed
   */
int init_link_sdk(link_player_sink sink, const char * btAddress, int width, int height, int widthMM, int heightMM);

int uninit_link_sdk();

/**
   * @brief start link service
   * @param type [in] @see LinkType
   * @note Asynchronous implementation, the startup is successful only after receiving the @see SINK_NOTIFY_SERVICE_STARTED notification
   * @return 0:post start service message sucessed
   */
int start_link_service(LinkType type);

/**
   * @brief stop link service
   * @param type [in] @see LinkType
   * @note Asynchronous implementation, the startup is successful only after receiving the @see SINK_NOTIFY_SERVICE_STOPED notification
   * @return 0:post stop service message sucessed
   */
int stop_link_service(LinkType type);

/**
   * @brief request link key event
   * @param type [in] @see LinkType
   * @param keycode [in] @see KeyCode defines
   * @return 0:sucessed
   */
int request_link_keycode(LinkType type, int keycode, bool isPressed);

/**
   * @brief request link touch event
   * @param type [in] @see LinkType
   * @param x [in] x position
   * @param y [in] y position
   * @return 0:sucessed
   */
int request_link_touchevent(LinkType type, bool isPressed, int x, int y);

/**
   * @brief request link action
   * @param type [in] @see LinkType
   * @param action [in] @see LINK_ACTION_XXXX
   * @param lparam [in] reserved, action param @see LINK_ACTION_XXXX
   * @param sparam [in] reserved, action param @see LINK_ACTION_XXXX
   * @return 0:sucessed
   */
int request_link_action(LinkType type, int action, int64_t lparam, const char * sparam);

/**
   * @brief send microphone datas to link
   * @param type [in] @see LinkType
   * @param datas [in] pcm frame datas
   * @param len[in] frame data len (is not frame size)
   * @note called every 10 milliseconds, pcm format @see sink_micinput_start_cb
   * @return 0:sucessed
   */
int send_microphone_datas(LinkType type, void * datas, int len);
#ifdef __cplusplus
}
#endif

#endif // LINKSUPPORT_H

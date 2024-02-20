#ifndef ECSDKAPPMANAGER_H
#define ECSDKAPPMANAGER_H

#include "ECSDKTypes.h"


namespace ECSDKFrameWork {


/**
 * @brief The APPManagerListener class
 *
 * You need to implement this interface to get some information about the mobile app running
 *
 */
class EC_DLL_EXPORT IECAPPManagerListener
{
public:
    /**
     * @brief ~APPManagerListener
     */
    virtual ~IECAPPManagerListener();

    /**
    * @brief  Called when EasyConnected status changed.
    *
    * @param  status  The changed EasyConnected message.
    */
	virtual void        onECStatusMessage(ECStatusMessage status) {};

    /**
    * @brief  Called when the phone app sends down HUD information.
    *
    * @param  data  HUD information.
    */
	virtual void        onPhoneAppHUD(const ECNavigationHudInfo& data) {};

	/**
	* @brief  Called when the phone app sends down HUD Road Junction Picture.
	* @param data
	*/
	virtual void	    onPhoneAppHUDRoadJunctionPicture(const ECHudRoadJunctionPictureInfo& data) {};

	/*
	* @brief  Called when phone app tell the music info.
	*
	* @param  data The information of music.
	*/
	virtual void		onPhoneAppMusicInfo(const ECAppMusicInfo& data) {};

    /**
    * @brief  Called when the phone app sends down some information.
    *
    * @param  data    Buffer of app information.
    *
    * @param  length  Buffer length.
    *
    * @note  data is json string, the fields includes os, osVersion and ip.
    *        Called when IECSDKListener::onSdkConnectStatus connect succeed.
    */
	virtual void        onPhoneAppInfo(const string& info) {};

	/**
	* @brief  Called when ECSDK wants car to do call operations(dial or hang up) via Bluetooth.
	*
	* @param  type    Operation type.
	*
	* @param  name    The person's name of corresponding number.
	*
	* @param  number  Phone numbers.
	*
	* @note   Phone app is not able to dial or hang up automatically due to the latest system access limitation,
	*         however, car is able to do it via Bluetooth. Therefore, ECSDK moves the call operations
	*         to car, which can dial or hang up when this method is called.
	*/
	virtual void        onCallAction(ECCallType type, const string& name, const string& number) {};

	/**
	* @brief onCarCmdNotified
	* @param carCmd
	*
	* \note The result of speech recognition on the phone.
	* This interface will not return the corresponding result until you call ECSDKAudioManager::registerCarCmds
	*/
	virtual void		onCarCmdNotified(const ECCarCmd& carCmd) {};

	/**
	* @brief  Called when phone app request the HU to start input.
	*
	* @param info relevant parameters about the input.
	*/
	virtual void        onInputStart(const ECInputInfo& info) {};

	/**
	* @brief  Called when phone app request the HU to cancel input.
	*/
	virtual void        onInputCancel() {};

	/**
	* @brief  Called when phone app tell the selection of input.
	*/
	virtual void        onInputSelection(int start, int stop) {};

	/**
	* @brief  Called when phone app tell the text of input.
	*/
	virtual void        onInputText(const char* text) {};

	/**
	* @brief  Called when phone app send the text of VR or TTS.
	*/
	virtual void        onVRTextReceived(const ECVRTextInfo& info) {};

	/**
	* @brief  Called when phone app tell the page list.
	*
	* @param  pages Array of the struct ECPageInfo.
	*
	*/
	virtual void        onPageListReceived(const vector<ECPageInfo>& pages) {};

	/**
	* @brief  Called when phone app tell the icons.
	*
	* @param  icons Array of the struct ECIconInfo.
	*
	*/
	virtual void        onPageIconReceived(const vector<ECIconInfo> icons) {};

	/**
	* @brief  Called when phone app tell weather.
	*
	* @param  data of weather information.
	*
	* @note   data is a json string.
	*/
	virtual void        onWeatherReceived(const string& data) {};

	/**
	* @brief  Called when phone app tell vr tips.
	*
	* @param  data  of tips information.
	*
	* @note   data is a json string.
	*/
	virtual void        onVRTipsReceived(const string& data) {};

	/**
	* @brief Called when mobile phone has a notification message.
	* @param notification
	*/
	virtual void        onPhoneNotification(const ECPhoneNotification& notification) {};

	/**
	* @brief Called when the phone app sends down HUD lane guidance Picture.
	* @param data
	*/
	virtual void        onPhoneAppHUDLaneGuidancePicture(const ECHudLaneGuidancePictureInfo& data) {};
};


/**
 * @brief The ECSDKAppMutual class
 *
 * This module provides an interface to interact with the mobile app,
 * which you can use to operate the app display page, driving mode, etc
 */
class EC_DLL_EXPORT ECSDKAPPManager
{
public:
    /**
     * @brief getInstance
     * @return
     */
    static ECSDKAPPManager*    getInstance();

    /**
     * @brief initialize
     * @param listener
     * @return
     */
    virtual bool        initialize(IECAPPManagerListener* listener = nullptr) = 0;

	/**
	* @brief release
	* @return
	*/
	virtual void		release() = 0;

    /**
    * @brief  Stop phone navigation.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note   ECSDKFramework suggests to use only one navigation between phone and car.
    *         If car start navigation, use this method to stop phone
    *         navigation, opposite to APPManagerListener::onECStatusMessage.
    *
    * @see    APPManagerListener::onECStatusMessage
    */
    virtual int32_t     stopPhoneNavigation() = 0;

    /**
    * @brief  Open app page.
    *
    * @param  page Specifying which page of app.
    *
    * @return ECSDK_OK on success, others on fail.
    */
    virtual int32_t     openAppPage(ECAppPage page) = 0;

    /**
    * @brief  Send car status info to phone.
    *
    * @param  carStatus The status type of car.
    *
    * @param  value     The value of the corresponding status type.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    */
    virtual int32_t		sendCarStatus(ECCarStatusType carStatus, ECCarStatusValue value) = 0;

    /**
    * @brief  upload statistics information to connected phone.
    *
    * @param  key key with which the specified value is to be associated
    *
    * @param  value value to be associated with the specified key
    *
    * @return ECSDK_OK on success, others on fail.
    *
    */
    virtual int32_t     uploadStatistics(const string& key, const string& value) = 0;

    /**
    * @brief  Enable downloading phone navigation HUD information.
    *
	* @param supportFunction APP HUD support function, @see ECAPPHUDSupportFunction
	*
    * @return ECSDK_OK on success, others on fail.
    *
    * @note  If car wants to display HUD information in its screen, it can ask phone app to send down
    *        HUD information.
    *
    * @see   APPManagerListener::onPhoneAppHUD
    */
    virtual int32_t     enableDownloadPhoneAppHud(uint32_t supportFunction = 0) = 0;

    /**
    * @brief  Disable downloading phone navigation HUD. Opposite to enableDownloadPhoneAppHud.
    *
    * @see    enableDownloadPhoneAppAudio
    */
    virtual void        disableDownloadPhoneAppHud() = 0;

	/**
	* @brief  Send car's blue tooth information to connected phone.
	*
	* @param  name The name of car's bluetooth.
	*
	* @param  adddress The adddress of car's bluetooth.
	*
	* @param  pin The pin code of car's bluetooth.
	*
	* @return ECSDK_OK on success, others on fail.
	*
	*/
	virtual int32_t     sendCarBluetooth(const string& name, const string& adddress, const string& pin) = 0;

	/**
	* @brief  Set bluetooth mac address of car and connected phone.    .
	*
	* @param  carBtMac   Bluetooth mac address of car.
	*
	* @param  phoneBtMac Bluetooth mac address of phone.
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note   This interface shall be called only when car was connected to phone via bluetooth.
	*         THIS IS A RESERVED METHOD. PLEASE IGNORE IT NOW.
	*/
	virtual int32_t     setConnectedBTAddress(const string& carBtName, const string& carBtMac, 
											const string& phoneBtName, const string& phoneBtMac) = 0;

	/**
	* @brief  Stop phone voice recognition(VR).
	*
	* @return ECSDK_OK on success, others on fail.
	*
	*/
	virtual int32_t     stopPhoneVR() = 0;

	/**
	* @brief This method make phone app play the tts audio of car's text.
	*
	* @param text  The text of tts.
	*
	* @param level The priority of tts, the value will be from 1 to 10, bigger value means higher priority
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note  This method will send the text to connected phone, then the tts audio of specified text will be played.
	*/
	virtual int32_t     playCarTTS(const string& text, uint32_t level) = 0;

	/**
	* @brief  Specifying commands to identify by VR.
	*
	* @param  carCmds  The command array.
	*
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note   This method specify which commands can be identified by VR.
	*         Then IECAudioManagerListener::onCarCmdNotified would be trigggered if a
	*         voice control command was recognized by VR.
	*/
	virtual int32_t     registerCarCmds(const vector<ECCarCmd>& cmdList, ECCarCmdEFFECTType type = EC_CAR_CMD_TYPE_EFFECTIVE_GLOBAL) = 0;

     /**
     * @brief This method will register a series similary sounding words to phone's app.
     * 
     * @param data string.
     * 
     * @return ECSDK_OK on success, others on fail.
     */
	virtual int32_t  	registerSimilarSoundingWords(const string& data) = 0;

	/**
	* @brief This method send the text of HU's input to phone app.
	*
	* @param text The text of input
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note  This method will send the text to connected phone.
	*/
	virtual int32_t     sendInputText(const string& text) = 0;

	/**
	* @brief This method send the action of HU's input to phone app.
	*
	* @param actionId The action of input
	*
	* @param keyCode The keyCode of input
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note  This method will send the action and keyCode to connected phone.
	*/
	virtual int32_t     sendInputAction(int32_t actionId, int32_t keyCode) = 0;

	/**
	* @brief This method send the selection of HU's input to phone app.
	*
	* @param start The start index of the selection.
	*
	* @param stop The stop index of the selection.
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note  This method will send the selection to connected phone.
	*/
	virtual int32_t     sendInputSelection(int32_t start, int32_t stop) = 0;

	/**
	* @brief This method will query the page list from connected phone's app.
	*        IECAPPManagerListener::onPageListReceived will tell the result of the query.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t      queryPageList() = 0;

	/**
	* @brief This method will query the icons of specified pages from connected phone's app.
	*        IECAPPManagerListener::onPageIconReceived will tell the result of the query.
	*
	* @param pages Array of the specified pages.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t      queryPageIcon(const vector<int32_t>& pages) = 0;

	/**
	* @brief This method will query weather via the connected phone's app.
	*        IECAPPManagerListener::onWeatherReceived will tell the result of the query.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t      queryWeather() = 0;

	/**
	* @brief This method will query vr tips via the connected phone's app.
	*        IECAPPManagerListener::onVRTipsReceived will tell the result of the query.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t      queryVRTips() = 0;

	/**
	* @brief This method will upload gps information to connected phone's app.
	*        IECAPPManagerListener::onECStatusMessage with status EC_STATUS_MESSAGE_ENABLE_UPLOAD_GPS_INFO
	*        and EC_STATUS_MESSAGE_DISABLE_UPLOAD_GPS_INFO will enable and disable the upload of gps infomation.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t      uploadGPSInfo(const string& data) = 0;

	/**
	* @brief  Upload car's night mode status to phone.
	*
	* @param  isNightModeOn  Whether or not night mode is turned on.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t     uploadNightModeStatus(bool isNightModeOn) = 0;

	/**
	* @brief This method will upload car bt call dato to phone app
	* @param type call records ro phone book.
	* @param uploadState upload state.
	* @param data Array of ECBTCallData.
	* @param len The length of the array.
	* @return EC_OK on success, others on fail.
	*/
	virtual int32_t     uploadBTCallData(ECBTCallDataType type, ECBTCallDataUploadState uploadState, const vector<ECBTCallData>& data) = 0;

	/**
	* @brief enable phone send system message notification
	* @param enable true:enable false:disable
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t     requestPhoneNotification(bool enable) = 0;

protected:
    ECSDKAPPManager();
    virtual ~ECSDKAPPManager();
};



}


#endif // ECSDKAPPMANAGER_H

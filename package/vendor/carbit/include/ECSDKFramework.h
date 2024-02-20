/*! \file ECSDKFramework.h */

#pragma once

#include "ECSDKTypes.h"
#define EC_VERSION_STRING "1.1.19.1"


namespace ECSDKFrameWork {

/**
 * @brief The IECSDKListener class
 *
 * @note This interface provides information about the connection process,
 * the connection type, and so on
 */
class EC_DLL_EXPORT IECSDKListener
{
public:
    /*!
     * \brief Virtual destructor.
     */
    virtual ~IECSDKListener();

    /**
     * @brief onSdkConnectStatus
     * @param status
     */
    virtual void    onSdkConnectStatus(ECSDKConnectedStatus status, ECSDKConnectedType type) = 0;

    /**
     * @brief onLicenseAuthFail
     * @param errCode
     * @param errMsg
     */
    virtual void    onLicenseAuthFail(int32_t errCode, const string& errMsg) = 0;

    /**
     * @brief onLicenseAuthSuccess
     * @param code
     * @param msg
     */
    virtual	void    onLicenseAuthSuccess(int32_t code, const string& msg) = 0;

};

/**
* @class  IECAccessDevice
*
* @brief  An interface class to implement special usb device access.
*/
class EC_DLL_EXPORT IECAccessDevice
{
public:
    virtual  ~IECAccessDevice();

    /**
    * @brief  Open usb device.
    *
    * @return Zero on success, others on fail.
    */
    virtual int32_t     open() = 0;

    /**
    * @brief  Read data from usb devices.
    *
    * @param  data    Read buffer.
    *
    * @param  length  Read size.
    *
    * @return The size of data read in bytes, zero if currently no read data available, or negative number on fail.
    */
    virtual int32_t     read(void *data, uint32_t length) = 0;

    /**
    * @brief  Write data into usb devices.
    *
    * @param  data    Write buffer.
    *
    * @param  length  Write size.
    *
    * @return The size of data written in bytes, negative number on fail.
    */
    virtual int32_t     write(void *data, uint32_t length) = 0;

    /**
    * @brief  Close usb device. Opposite to open.
    *
    * @see    open
    */
    virtual void        close() = 0;
};

/**
* @class  ECSDKConfig
*
* @brief Configure the functional interfaces of the ECSDKFramework.
*/
class ECSDKConfigPrivate;
class EC_DLL_EXPORT ECSDKConfig
{
public:
	explicit ECSDKConfig(const string& uuid, const string& version, const string& writableDir);
	explicit ECSDKConfig();
	 ~ECSDKConfig();

	 void setUUID(const string& uuid);
	 void setVersion(const string& version);
	 void setWorkSpace(const string& writableDir);
	
	 void setCommonConfig(const string& cfgName, const string& cfgValue);
	 void setCommonConfig(const string& cfgName, uint32_t cfgValue);
	 void setCommonConfig(const string& cfgName, bool cfgValue);

     bool getCommonConfig(const string& cfgName, string& cfgValue);
     bool getCommonConfig(const string& cfgName, uint32_t& cfgValue);
     bool getCommonConfig(const string& cfgName, bool& cfgValue);

	 inline ECSDKConfigPrivate* d_func() {
		return reinterpret_cast<ECSDKConfigPrivate*>(d_ptr);
	 }
private:
	ECSDKConfig(const ECSDKConfig &) = delete;
	ECSDKConfig &operator=(const ECSDKConfig &) = delete;
	ECSDKConfigPrivate* d_ptr;

};



/*!
 * \brief A macro used to reference ECSDKFramework instance.
 *
 * User can use this to simplify function call.
 */
#define ecSdk (ECSDKFrameWork::ECSDKFramework::getInstance())

/*!
 * \brief The ECSDKFramework class
 */
class EC_DLL_EXPORT ECSDKFramework
{
public:
    /*!
     * \brief Singleton function.
     *
     * \return The instance of derived class.
     */
    static ECSDKFramework* getInstance();

    /*!
    * @brief  Initialize ECSDKFramework environment.
    *         After ECSDKFramework was initialized, user can call ECSDKFramework::start to communicate with the
    *         connected phone. ECSDKFramework supports finding an connected phone(Android or iOS)
    *         via USB cable and WiFi.
    *
    * \param cfg
    *
    * \param pListener
    *
    * \param dev This parameter usually does not require an assignment, and when the user needs to
    * use the eap connection mode, a read-write interface needs to be provided
    * \return
    */
    virtual bool initialize(ECSDKConfig* cfg, IECSDKListener* pListener) = 0;

	/*!
	* \brief release
	* \return
	*/
	virtual void release() = 0;

    /*!
     * \brief setECSDKLogConfig
     * \param cfg
     */
    virtual void setECSDKLogConfig(const ECLogConfig& cfg) = 0;

    /*!
     * \brief Start this module.
     *
	 * @return ECSDK_OK on success, others on fail.
	 *
     * \note After calling this interface, the ECSDKFramework establishes a connection to the phone
     */
    virtual int32_t start() = 0;

    /*!
     * \brief Stop this module.
     *
     * \note Call this interface when you don't need to connect to your phone
     */
    virtual void stop() = 0;

    /*!
     * \brief Used to get version
     *
     * @return The ECSDKFramework version in string format, etc. "0.1.0.9.7.3".
     */
    virtual const string& getECVersion()  = 0;

	/*!
	* \brief Used to get version code
	*
	* @return The ECSDKFramework version code.
	*/
	virtual uint32_t getECVersionCode() = 0;

	/**
	* @brief  This method will enable ota network update use sandbox environment.
	*
	* @note   The default environment of ota network update is production.
	*/
	virtual int32_t     enableSandbox() = 0;


	/**
	* @brief  User need to call this interface when the network state of the vehicle changes.
	*
	* \param action
	*
	* \param netInfo
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t     notifyWifiStateChanged(ECWifiStateAction action, const ECNetWorkInfo& netInfo) = 0;

	/**
	* @brief  User need to call this interface when usb device state has changed.
	*
	* \param state  usb device current status.
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t		notifyDeviceStateChanged(ECDeviceState state) = 0;

	/**
	* @brief This method will configure some information to tranpsport with a phone.
	*
	* @param key referred to ECTransportInfo
	*
	* @param value the value to corresponding key.
	*
	* @note  Next connection takes effect.
	*        User just need to call this method once. The config will be always valid until next ECSDKFramework::initialize.
	*/
	virtual int32_t    configTransportInfo(int32_t key, int32_t value) = 0;
	

	/**
	* @brief  Bind the transport with a special device.
	*         After binding, ECSDKFramework can access the device via the provided IECAccessDevice.
	*
	* @param  type  The transport to be bound.
	*
	* @param  dev   The device access methods, see IECAccessDevice.
	*
	* @return EC_OK on success, others on fail.
	*
	* @note   Only platform-dependent transport(i.e EAP) needs to bind a USB device.
	*/
	virtual int32_t	   bindTransportDevice(ECTransportType type, IECAccessDevice *dev) = 0;

	/**
	* @brief  Unbind device for the given transport. Opposite to bindTransportDevice.
	*
	* @param  type  The transport to be unbound.
	*
	* @see    ECSDKFramework::bindTransportDevice
	*/
	virtual void       unbindTransportDevice(ECTransportType type) = 0;

	virtual int32_t	   bindTransportWifiDevice(ECTransportType type, const string& phoneIp) = 0;

protected:
    /*!
     * \brief Default constructor.
     */
    ECSDKFramework();
    /*!
     * \brief Destructor.
     */
    virtual ~ECSDKFramework();
};

}

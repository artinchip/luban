#ifndef ECSDKTOOLKIT_H
#define ECSDKTOOLKIT_H


#include "ECSDKTypes.h"


namespace ECSDKFrameWork {

/**
 * @brief The ExtrasManageListener class
 */

class EC_DLL_EXPORT IECToolKitListener
{
public:
    /**
     * @brief ~ExtrasManageListener
     */
    virtual ~IECToolKitListener();

    /**
     * @brief onQueryTime
     *
     * @param  gmtTime   GMT(UTC) time in milliseconds.
     *
     * @param  localTime Local time in milliseconds.
	 *
	 * @param  timeZone  Time zone,  etc. "Asia/Shanghai".
	 *
	 * @param dateTime: "27.07.2022 9:45:06.120", 手机当前日期时间。format：dd.MM.yyy hh:mm:ss.zzz
	 *
     */
	virtual void        onQueryTime(uint64_t gmtTime, uint64_t localTime, const string& timeZone, const string& dateTime) {};

    /**
     * @brief onQueryGPS
     *
     * @param  status  Whether GPS information is valid, true means valid.
     *
     * @param  longitude GPS longitude.
     *
     * @param  latitude  GPS latitude.
     */
	virtual void        onQueryGPS(bool status, const ECGPSInfo& info) {};
    
	/**
    * @brief  Called when bulk data is received(RESERVED).
    *
    * @param  data    Buffer of bulk data.
    *
    * @param  length  Buffer length.
    *
    * @note   THIS IS A RESERVED METHOD. PLEASE IGNORE IT NOW.
    */
	virtual void        onBulkDataReceived(const void *data, uint32_t length) {};

	/**
	* @brief  Called when forwardToPhone result.
	*
	* @param  forwardInfo    the forward result.
	*
	*/
	virtual void		onForwardToPhone(const ECForwardInfo& forwardInfo) {};

	/**
	* @brief  Called when GenerateQRCodeUrl result.
	*
	* @param  url    The url string.
	*
	*/
	virtual void		onGenerateQRCodeUrl(const string& url, const uint8_t* data, const uint32_t dataLen) {};

	/**
	* @brief Called when phone need control car @see sendPhoneControlCarResult.
	* @param data Buffer of request information.
	* @param length Buffer length.
	*/
	virtual void		onPhoneControlCarRequest(const string& cmd) {};
};

/**
* @class  ECCustomProtocol
*
* @brief  An abstract class to send and receive custom data between HU and phone.
*
* @note   An instantiated object of this abstract class can be gained by ECSDKToolKit::getInstance()->getCustomProtocol().
*/
class EC_DLL_EXPORT ECCustomProtocol
{
public:
	struct ECCustomData
	{
		uint32_t cmdType;                        ///< the cmd type of custom data
		uint32_t reqSeq;                         ///< the request sequence of current send or reply.
		uint32_t rspSeq;                         ///< the response sequence of current send or reply.
		void*    data;                           ///< the data buffer.
		uint32_t length;                         ///< the length of data buffer.

		ECCustomData()
		{
			cmdType = 0;
			reqSeq = 0;
			rspSeq = 0;
			data = nullptr;
			length = 0;
		}
	};

	/**
	* @class  ICustomDataResponse
	*
	* @brief  An interface class to implement response for a send with reply.
	*/
	class ICustomDataResponse
	{
	public:
		/**
		* @brief Called when a reply from phone was received.
		*
		* @param data The custom data of reply from phone.
		*
		* @note the rspSeq of data will be non-zero, which was equal to the reqSeq of one send.
		*/
		virtual void onReceive(const ECCustomData& data) {};

		/**
		*  @brief Called When an error occurs for current send.
		*
		*  @param reqSeq The real reqSeq of current send.
		*
		*  @param errNo The error num.
		*                -1  failed to allocate memory.
		*                -2  failed to send to phone.
		*                -3  the version of phone app does not support the function of custom protocol.
		*  @param error The error description.
		*/
		virtual void onError(uint32_t reqSeq, int32_t errorNo, const string& error) {};

		/**
		*  @brief Called When the specified timeout time is reached.
		*
		*  @param reqSeq The real reqSeq of current send.
		*/
		virtual void onTimeout(uint32_t reqSeq) {};
	};

	/**
	* @class  ICustomDataReceiver
	*
	* @brief  An interface class to implement receive of custom data from phone.
	*/
	class ICustomDataReceiver
	{
	public:
		/**
		* @brief Called When custom data from phone was received.
		*
		* @param data The custom data of phone.
		*
		* @note the rspSeq of data will be zero.
		*/
		virtual void onReceive(const ECCustomData& data) {};
	};

	/**
	* @brief  set a receiver to handle receive of custom data from phone.
	*
	* @param receiver The ICustomDataReceiver object.
	*/
	virtual int32_t registerCustomDataReceiver(ICustomDataReceiver* receiver) = 0;

	/**
	* @brief send custom data to phone.
	*
	* @param data The custom data.
	*
	* @param timeout Timeout for corresponding reply to current send in seconds. An value of -1 indicates no timeout.
	*
	* @param responseCallback The ICustomDataResponse object.
	*
	* @note  If the send has reply, then responseCallback cann't be null.
	*
	*        If the send has no reply, then responseCallback should be null. ECCustomProtocol::sendCustomData(data) can be used.
	*
	*        User has no need to set reqSeq and rspSeq of data. After the method is invoked, the reqSeq and rspSeq of data is set to the actual value used.
	*/
	virtual int32_t sendCustomData(ECCustomData& data, int32_t timeout = -1, ICustomDataResponse* responseCallback = nullptr) = 0;

	/**
	* @brief reply custom data to phone.
	*
	* @param data The custom data.
	*
	* @param reqSeq The reqSeq of the custom data received. It will be as rspSeq of current send to phone.
	*
	* @note User has no need to set reqSeq and rspSeq of data. After the method is invoked, the reqSeq and rspSeq of data is set to the actual value used.
	*/
	virtual int32_t replyCustomData(ECCustomData& data, uint32_t reqSeq) = 0;
};

/*
 * @brief The ECSDKExtrasManage class
 */
class EC_DLL_EXPORT ECSDKToolKit
{
public:
    /**
     * @brief getInstance
     * @return
     */
    static ECSDKToolKit *getInstance();

    /**
     * @brief initialize
     * @param pListener
     * @return
     */
    virtual bool        initialize(IECToolKitListener* listener = nullptr) = 0;

	/**
	* @brief release
	* @return
	*/
	virtual void		release() = 0;

    /**
    * @brief  Open ftp server which forwards the local ports on HUD to the ports on the connected phone.
    *
    * @param  userName Specifying the username of ftp.
    *
    * @param  pwd Specifying the password of ftp.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note   This interface shall be called only after
    * IECSDKListener::onSdkConnectStatus::EC_CONNECT_PHONE_SUCCEED
    *
    */
    virtual int32_t     openFtpServer(const string& userName, const string& pwd) = 0;

    /**
    * @brief  Send an request to phone for time information.
    *
    * @return ECSDK_OK on success, others on fail.
    */
    virtual	int32_t     queryTime() = 0;

    /**
    * @brief  Send an request to phone for GPS information.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note   The query result to see ExtrasManageListener::onQueryGPS.
    */
    virtual	int32_t     queryGPS() = 0;

    /**
    * @brief  Send bulk data to phone(RESERVED).
    *
    * @param  data    Buffer of bulk data.
    *
    * @param  length  Buffer length.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note  THIS IS A RESERVED METHOD. PLEASE IGNORE IT NOW.
    */
    virtual int32_t     sendBulkDataToPhone(const void *data, uint32_t length) = 0;

	/**
	* @brief This method can set up forwarding of HU's port to connected phone's port when connection is via usb.
	*
	* @param  in Which tell the local port and remote port which will be mapped.
	*
	* @return ECSDK_OK on success, others on fail.
	*
	* @note   If the connection is via wifi, ip in forwardInfo is the connected phone's ip,
	*         the localPort in forwardInfo is equal to the phonePort.
	*
	*         If the connection is via usb, ip in forwardInfo will be 127.0.0.1,
	*         the localPort is the real port of HU forwarded to connected phone.
	*			
	*		  IECToolKitListener::onForwardToPhone will tell the result of forwardTophone.
	*/
	virtual int32_t      forwardToPhone(const ECForwardInfo& in) = 0;

	
	virtual ECCustomProtocol* getCustomProtocol() = 0;

	/**
	* @brief This method will get a url string for qr code.
	*
	* @param info The ssid and pwd of info will be encoded to url.
	* @param type Specify the type of build. reference ECQrCodeType.
	* @param fileName Specify the generated file name
	* @param qrParam Parameters required to generate QRcode
	* @param bmpParam Parameters required for generating BMP pictures
	*
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual  int32_t	  generateQRCodeUrl(const ECQRInfo& info, const int32_t type=0, const char* fileName=NULL, const ECQrCodeParam* qrParam=NULL,const ECQrCodeBmpParam* bmpParam=NULL) = 0;

	/**
	* @brief This methiod will seed result of  Phone control car request @see onPhoneControlCarRequest.
	* @param data pointer to a string.
	* @param length the length of the string.
	* @return ECSDK_OK on success, others on fail.
	*/
	virtual int32_t	      sendPhoneControlCarResult(const string& result) = 0;
protected:
    ECSDKToolKit();
    virtual ~ECSDKToolKit();
};

}

#endif // ECSDKTOOLKIT_H

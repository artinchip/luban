#ifndef ECSDKTOUCHKEYMANAGER_H
#define ECSDKTOUCHKEYMANAGER_H

#include "ECSDKTypes.h"


namespace ECSDKFrameWork {

/**
 * @brief The ECSDKTouchKeyEventManager class
 */
class EC_DLL_EXPORT ECSDKTouchKeyManager
{
public:
    /**
     * @brief getInstance
     * @return
     */
    static ECSDKTouchKeyManager*   getInstance();

    /**
    * @brief  Send touch event to phone.
    *
    * @param  touch  Touch parameters.
    *
    * @param  type   Touch type.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note   Touch event does NOT work without mirror connection.
    */
    virtual int32_t     sendTouchEvent(const ECTouchEventData& touch, ECTouchEventType type) = 0;

    /**
    * @brief  when you press button on steering wheel, send corresponding button event to phone.
    *
    * @param  btnCode  key code of the Button .
    *
    * @param  type  Event type.
    *
    * @return ECSDK_OK on success, others on fail.
    *
    * @note   Button event does NOT work without phone app.
    */
    virtual int32_t     sendBtnEvent(ECBtnCode btnCode, ECBtnEventType type) = 0;

	/**
	* @brief  send button code to phone.
	*
	* @param  btnCode  key code of the Button .
	*
	* @param  type  Event type.
	*
	* @param  channel Which can specify the namespace of the code. Zero means standard, others means custom.
	*                 When channel is zero, the value of channel can refer to ECBtnCode.
	*
	* @return EC_OK on success, others on fail.
	*
	* @note   Button event does NOT work without phone app.
	*/
	virtual int32_t     sendBtnEvent(int32_t btnCode, ECBtnEventType type, int32_t channel) = 0;

protected:
	ECSDKTouchKeyManager();
    virtual ~ECSDKTouchKeyManager();
};


}

#endif // ECSDKTOUCHKEYMANAGER_H

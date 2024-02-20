#ifndef ECSDKOTAMANAGER_H
#define ECSDKOTAMANAGER_H


#include "ECSDKTypes.h"


namespace ECSDKFrameWork {


class EC_DLL_EXPORT IECOTAManagerListener
{
public:
    virtual             ~IECOTAManagerListener();

    /**
	* @brief  Called when checkOTAUpdate was called, it will tell the result of checkOTAUpdate.
	*
	* @param  downloadableSoftwares  A array of ECOTAUpdateSoftware, which is downloadable software.
	*	*
	* @param  downloadedSoftwares A array of ECOTAUpdateSoftware, which is downloaded software.
	*
	*/
    virtual void        onOTAUpdateCheckResult(const vector<ECOTAUpdateSoftware>& downloadableSoftwares, const vector<ECOTAUpdateSoftware>& downloadedSoftwares) = 0;

	/*
	* @brief  Called when remote downloadable software has been downloaded to phone.
	*
	* @param  downloadableSoftwares It pointer to a array of ECOTAUpdateSoftware, which has been in phone, can be downloaded from phone to HU.
	*
	* @param  downloadableLength  The length of the downloadable array.
	*/
	virtual void        onOTAUpdateRequestDownload(const vector<ECOTAUpdateSoftware>& downloadableSoftwares) {};

	/**
	* @brief Called when startOTAUpdate is called, it will notify the progress of downloading.
	*
	* @param downloadingSoftwareId The id of the downloading software.
	*
	* @param progress The progress of the downloading software,which is a percentage.
	*
	* @param softwareLeftTime The left time of the downloading software.
	*
	* @param otaLeftTime The left time of all the specified software by startOTAUpdate.
	*/
    virtual void        onOTAUpdateProgress(const string& downloadingSoftwareId, float progress, uint32_t softwareLeftTime, uint32_t otaLeftTime) = 0;

	/**
	* @brief Called when startOTAUpdate is called, it will notify software is downloaded.
	*
	* @param downloadedSoftwareId The id of the downloaded software.
	*
	* @param md5Path The md5 file path.
	*
	* @param packagePath The software path.
	*
	* @param iconPath The icon path.
	*
	* @param leftSoftwareNum The amount of software remaining to be downloaded.
	*/
    virtual void        onOTAUpdateCompleted(const string& downloadedSoftwareId, const string& md5Path, const string& packagePath, const string& iconPath, uint32_t leftSoftwareNum)  = 0;

    /**
    * @brief  Called when checkOTAUpdate or startOTAUpdate failed.
    *
    * @param  errCode error code, see ECOTAUpdateErrorCode.
    *
    * @param  softwarId  the id of software.
    */
    virtual void        onOTAUpdateError(int32_t errCode, const string& softwareId) = 0;
};


class EC_DLL_EXPORT ECSDKOTAManager
{
public:
    /*!
     * \brief Singleton function.
     *
     * \return The instance of derived class.
     *
     */
    static ECSDKOTAManager *getInstance();


    /*!
     * \brief initialize
     * \param listener
     * \param cfg
     * \return
     */
    virtual bool        initialize(IECOTAManagerListener* listener, const ECOTAConfig& cfg) = 0;
	
	/**
	* @brief release
	* @return
	*/
	virtual void		release() = 0;

    /**
    * @brief  This method will trigger a check to ota update.
    *
    * @param  language Which language's description of software will be gained when HU's network is used for ota update.
    *
    * @param  mode     Specify which way to check ota update.
    *
    * @return ECSDK_OK on success, others on fail.
    *
	* @note   IECOTAManagerListener::onOTAUpdateCheckResult will tell the result.
    */
    virtual int32_t     checkOTAUpdate(ECLanguage language, ECOTAUpdateCheckMode mode = EC_OTA_CHECK_VIA_DEFAULT) = 0;


    /**
    * @brief  This method will download specified softwares sequentially.
    *
    * @param  softwareIds The array save the software id.
    *
    * @return ECSDK_OK on success, others on fail.
    *
	* @note   The software id would gained from IECOTAManagerListener::onOTAUpdateCheckResult
	*		  after ECSDKOTAManager::checkOTAUpdate was called.
	*         The software id also can be gained from IECOTAManagerListener::onOTAUpdateRequestDownload.
	*         If softwareIds length is zero , all downloadable software will be downloaded.
	*         The method can download remote software to phone or download phone's software to HU.
    */
    virtual int32_t     startOTAUpdate(const vector<string>& softwareIds) = 0;

    /**
    * @brief  This method will stop downloading softwares.
    *
    * @note   OTA update doesn't support breakpoint continuation. The software which haven't been completed
    *         would be removed.
    */
    virtual void        stopOTAUpdate() = 0;

protected:
    ECSDKOTAManager();
    virtual ~ECSDKOTAManager();
};


}

#endif // ECSDKOTAMANAGER_H

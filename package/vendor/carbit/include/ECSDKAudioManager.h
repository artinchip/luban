#ifndef ECSDKAUDIOMANAGER_H
#define ECSDKAUDIOMANAGER_H

#include "ECSDKTypes.h"



namespace ECSDKFrameWork {

/*!
 * \brief The base class of audio player object.
 *
 * A audio player is used to play audio data from our phone app.
 * You need to inherit and implement each interface
 *
 */
class EC_DLL_EXPORT IECAudioPlayer
{
public:
    /*!
     * \brief Virtual destructor.
     */
    virtual ~IECAudioPlayer();

    /*!
     * \brief Used to start the audio player.
     *
     * \param type Audio types, including intercom, navigation, VR, music
     *
     * \param info Format of audio data
     *
     * \note This interface is called when the phone clicks to play music,
     * and you may need to initialize the audio decoder on this interface.
     */
    virtual void start(ECAudioType type, const ECAudioInfo& info) = 0;

    /*!
     * \brief Used to stop the audio player.
     *
     * \param type Audio types, including intercom, navigation, VR, music
     *
     * \note This interface is called back when the phone clicks to stop playing music.
     */
    virtual void stop(ECAudioType type) = 0;

    /*!
     * \brief Used to play the audio data.
     *
     * \note
     * - Before call this, you must ensure the audio player has been initialized successfully.\n
     * - A pure virtual function derived class must implement it.
     */
    virtual void play(ECAudioType type, const void* data, uint32_t len) = 0;

    /*!
     * \brief Used to set audio's volume.
     *
     * \param type Audio types, including intercom, navigation, VR, music
     *
     * \note A pure virtual function derived class must implement it.
     */
    virtual void setVolume(ECAudioType type, uint32_t vol) = 0;

};


/*!
 * \brief The base class of audio recorder object.
 *
 * A audio recorder is used to record by using the system microphone and upload audio data to our phone app.
 *
 */
class EC_DLL_EXPORT IECAudioRecorder
{
public:
    /*!
     * \brief Virtual destructor.
     */
    virtual ~IECAudioRecorder();
    /*!
     * \brief Used to start the audio recorder.
     *
     * \param info Format of audio data
     *
     * \note When you tap the phone's microphone, this interface is called back,
     * and you get to initialize the recording device here
     */
    virtual void start(const ECAudioInfo& info) = 0;

	/**
	* @brief Used to provide recording data.
    *
    * @param data The object that holds the data
	*
	* @return 0 on successful operation, -1 on the microphone equipment is not ready, -2 on unknown error
	*
	* @note You need to implement this interface to provide the recording data.When the ECSDKFramework calls start, 
	* a thread is started to invoke the interface to retrieve the recording data.
	*
	*/
	virtual int32_t record(string& data) = 0;

    /*!
     * \brief Used to stop the audio recorder.
     *
     * \note A pure virtual function derived class must implement it.
     */
    virtual void stop() = 0;


};

/**
 * @brief The ECSDKAudioPlayer class
 */
class EC_DLL_EXPORT ECSDKAudioManager
{
public:
    /**
     * @brief getInstance
     *
     * @return ECSDK_OK on success, others on fail.
     */
    static ECSDKAudioManager *getInstance();

    /**
     * @brief initialize
     * @param AideoPlayerObject
     * @return ECSDK_OK on success, others on fail.
     */
    virtual bool        initialize(IECAudioPlayer* audeoPlayerObject, IECAudioRecorder* audioRecorder) = 0;

	/**
	* @brief release
    *
    * @return ECSDK_OK on success, others on fail.
	*/
	virtual void		release() = 0;

    /**
     * @brief enableDownloadPhoneAppAudio 
     *
     * @param supportType : The type of audio to be transmitted, for example EC_AUDIO_TYPE_TTS | EC_AUDIO_TYPE_VR
     *
     * @return ECSDK_OK on success, others on fail.
     */
    virtual int32_t     enableDownloadPhoneAppAudio(uint32_t supportType, bool autoChangeToBT = true) = 0;

    /**
     * @brief disableDownloadPhoneAppAudio
     *
     * @return ECSDK_OK on success, others on fail.
     */
    virtual int32_t     disableDownloadPhoneAppAudio() = 0;


	/**
	* @brief  Enable downloading system audio form phone.Current avaliable on iPhone Lighting  Mode;
	*
	* @return EC_OK on success, others on fail.
	*
	* @note   The audio type of the callback is EC_AUDIO_TYPE_MUSIC.
	*
	*/
	virtual int32_t     enableDownloadSystemAudio() = 0;

	/**
	* @brief  Disable downloading system audio.
	*
	* @see    enableDownloadSystemAudio
	*/
	virtual void        disableDownloadSystemAudio() = 0;

	/**
	* @brief  Sets the caching time for audio data to be sent down from the phone. By default, it is not cached.
	*
	*/
	virtual void		setAudioCacheTime(uint32_t cacheTime) = 0;

	/**
	* @param enableDelay   Whether to enable delay stop 
	*
	* @param additionTime   addition delay time
	*
	* @brief  Whether to turn on to increase the stop delay based on the playback time of the data volume.
	*
	*/
	virtual void		enableAudioDelay(bool enableDelay, uint32_t additionTime = 0) = 0;

	/**
	* @brief This method will config Speech Enhancement parameters.
	*
	* @param key referred to EC_SE_PARAM_KEY_
	*
	* @param value the value to corresponding key.
	*
	* @note  call this method before ECSDKFramework::start, then information with the config
	*        will be send to connected phone when IECAudioRecorder::start trigered.
	*        User just need to call this method once. The config will be always valid until next ECSDKFramework::start.
	*/
	virtual int32_t    configSEParam(const string& key, const string& value) = 0;

protected:
    ECSDKAudioManager ();
    virtual ~ECSDKAudioManager();
};


}

#endif // ECSDKAUDIOMANAGER_H

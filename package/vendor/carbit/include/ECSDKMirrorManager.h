#ifndef ECSDKMIRRORMANAGER_H
#define ECSDKMIRRORMANAGER_H

#include "ECSDKTypes.h"


namespace ECSDKFrameWork {

/*!
 * \brief The base class of video player object.
 *
 * A video player is used to play a video.
 *
 */
class EC_DLL_EXPORT IECVideoPlayer
{
public:
    /*!
     * \brief Virtual destructor.
     */
    virtual ~IECVideoPlayer();

    /*!
     * \brief start the video.
     *
     * \param width
     *
     * \param height
     *
     * \note When the user calls the startMIrror interface, the phone returns the recorded data,
     * and ECSDKFramewok calls the interface, where the user may need to initialize the decoder
     * and display the render window
     */
    virtual void start(int32_t width, int32_t height) = 0;

    /*!
     * \brief Stop the video.
     *
     * \note The ECSDKFramework calls this interface to tell the user to stop decoding when
     * for some reason, such as when the screen is disconnected
     */
    virtual void stop() = 0;

    /*!
     * \brief Used to play the mirror data.
     *
     * \param[in] data mirror data.
     * \param[in] len data length.
     *
     */
    virtual void play(const void *data, int32_t len) = 0;

};

/**
 * @brief The IECVideoManagerListener class
 */
class EC_DLL_EXPORT IECMirrorManagerListener
{
public:
    /**
     * @brief onMirrorStatus
     * @param status
     */
    virtual void onMirrorStatus(ECSDKMirrorStatus status) = 0;

    /**
     * @brief onRealMirrorSizeChanged
     * @param realWidth
     * @param realHeight
     *
     * \note The actual size of the projection screen does not equal the size of the video stream in some cases.
     * The surrounding area is filled with black. This message calls back the actual size of the projection screen
     */
    virtual void onMirrorInfoChanged(ECVideoInfo info) {};
};

/**
 * @brief The ECSDKVideoPlayer class
 */
class EC_DLL_EXPORT ECSDKMirrorManager
{

public:
    /**
     * @brief getInstance
     * @return
     */
    static ECSDKMirrorManager *getInstance();

    /**
     * @brief initialize
     * @param VideoPlayerObject
     * @param listener
     * @return
     */
    virtual bool    initialize(IECVideoPlayer* VideoPlayerObject, IECMirrorManagerListener* listener) = 0;

	/**
	* @brief release
	* @return
	*/
	virtual void	release() = 0;

    /**
     * @brief setMirrorConfig
     * @param cfg
     * @return
     */
    virtual int32_t setMirrorConfig(const ECMirrorConfig& cfg) = 0;

    /**
     * @brief startMirror
     * @return
     */
    virtual int32_t startMirror() = 0;

    /**
     * @brief stopMirror
     */
    virtual void    stopMirror() = 0;

	virtual void	pauseMirror() = 0;

	virtual void	resumeMirror() = 0;

protected:
    ECSDKMirrorManager ();
    virtual ~ECSDKMirrorManager();
};


}

#endif // ECSDKMIRRORMANAGER_H

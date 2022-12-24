#include <mutex>

#include "RaspicamSrc/raspicam.h"
#include "cameraCapture.h"

#ifdef R1080
#define ImageWidth  1920
#define ImageHeight 1080
#else   // !R1080
#define ImageWidth  1280
#define ImageHeight 720
#endif // !R1080
#define ImageSize   (ImageWidth * ImageHeight)

class PiCamera : public raspicam::RaspiCam
{
    int m_width;
    int m_height;

public:
	void Init(int width, int height, int frameRate, int shutterSpeed, int iso)
	{
		m_width		= width;
		m_height	= height;

	    setWidth ( width );
	    setHeight ( height );
	    setFrameRate(frameRate);
	    setShutterSpeed(shutterSpeed);
	    setISO(iso);

	    CommitParams();

	    open();
	}

	void CommitParams()
	{
	    setExposure(raspicam::RASPICAM_EXPOSURE_AUTO);   //RASPICAM_EXPOSURE_OFF); //RASPICAM_EXPOSURE_AUTO);
//	    setShutterSpeed(0);
//	    setISO(800);
	    setBrightness(50);
	    setSaturation(100);
	    setHorizontalFlip(false);
	    setContrast(0);

	    setAWB(raspicam::RASPICAM_AWB_FLUORESCENT);
	}
} PiCamera;

#define ShutterSpeedScale   38

int initializeCamera(int shutterSpeed, int iso)
{
#ifdef R1080
	PiCamera.Init(1920, 1080, 90, shutterSpeed * 38, iso);
#else   // !R1080
	PiCamera.Init(1280, 720, 90, shutterSpeed * 38, iso);
#endif // !R1080

//	Camera.StartCapture();

    return(0);
}

int captureImage(unsigned char * out_buffer, int out_buffer_size)
{
    if (out_buffer_size != ImageSize)
    {
        printf("captureImage: Invalid buffer size: %d\n", out_buffer_size);
        return(1);
    }

    PiCamera.retrieve (out_buffer);

    return(0);
}

std::mutex mutex;

int setShutterSpeed(int speed)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.setShutterSpeed(speed * 38);
    }
    printf("setShutterSpeed: %d\n", speed);

    return(0);
}

void setGain(int gain)   // ISO
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.setISO(gain);
    }
    printf("setGain: %d\n", gain);
}

#ifndef INNOMAKER
void grabImage()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.grab();
    }
}

void setContrast(int contrast)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.setContrast(contrast);
    }

    printf("setContrast: %d\n", contrast);
}

void setAutoExposure(bool autoExp)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.setExposure(autoExp ? raspicam::RASPICAM_EXPOSURE_AUTO : raspicam::RASPICAM_EXPOSURE_OFF);
    }

    printf("setAutoExposure: %d\n", autoExp);
}

void setBrightness(int brightness)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        PiCamera.setBrightness(brightness);
    }

    printf("setBrightness: %d\n", brightness);
}
#endif // !INNOMAKER

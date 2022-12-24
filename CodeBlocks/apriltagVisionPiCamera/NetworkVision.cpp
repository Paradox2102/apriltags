#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
//#include <VG/openvg.h>
#include <thread>
#include <mutex>
#include <string.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <termios.h>
#include <cstring>      // Needed for memset
#include <sys/socket.h> // Needed for the socket functions
#include <netdb.h>      // Needed for the socket functions
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <jpeglib.h>
#include <atomic>

#define GPIOx
#ifdef GPIO

#define RELAY
#include <wiringPi.h>
#ifdef RELAY
#define LED_ON	HIGH
#define LED_OFF	LOW
#else	// !RELAY
#define LED_ON	LOW
#define LED_OFF	HIGH
#endif	// !RELAY

#endif	// GPIO


#include "RaspicamSrc/raspicam.h"
#include "ProcessImage.h"

using namespace std;

#ifdef UseGraphics
extern "C"
{
#include "./Graphics/shapes.h"
}
#endif	// UseGraphics

int CaptureWidth = 640;
int CaptureHeight = 480;
int FrameRate = 30;
bool g_light = false;
int g_profile = 0;


HSVFilters filters;
int g_maxRegions	= 1;

#define MinH	(filters.m_filters[filters.m_color].m_minH)
#define MaxH	(filters.m_filters[filters.m_color].m_maxH)
#define MinS	(filters.m_filters[filters.m_color].m_minS)
#define MaxS	(filters.m_filters[filters.m_color].m_maxS)
#define MinV	(filters.m_filters[filters.m_color].m_minV)
#define MaxV	(filters.m_filters[filters.m_color].m_maxV)

#define TimeOffset	0;	//1566846027200LL;

double GetTimer()
{
	timespec	ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return((double) ts.tv_sec + ((double) ts.tv_nsec / 1000000000.0));
}

int64_t GetTimeMs()
{
	timespec	ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return (((int64_t) ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)) - TimeOffset;
}

void sleepMs(int ms)
{
	struct timespec tim, tim2;

	tim.tv_sec = ms / 1000;
	tim.tv_nsec = (ms % 1000) * 1000000L;

	nanosleep(&tim , &tim2);
}

int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

int getkey(void)
{
  if (kbhit())
  {
	  return(getchar());
  }

  return(0);
}

#define DefaultImageCacheSize	120	//50	//10

class ImageCacheFrame
{
public:
	int				m_frame	= -1;
	unsigned char * m_pFrame = 0;

	~ImageCacheFrame()
	{
		if (m_pFrame)
		{
			delete[] m_pFrame;
		}
	}
};

class ImageCache
{
public:
	ImageCacheFrame	*	m_pFrames	= 0;
	int					m_nFrames	= 0;
	int					m_curFrame	= 0;
	int					m_frameSize = 0;
	mutex 				m_mutex;

public:
	~ImageCache()
	{
		if (m_pFrames)
		{
			delete[] m_pFrames;
		}
	}

	void SetCacheSize(int count, int frameSize)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_pFrames)
		{
			delete[] m_pFrames;
		}

		m_pFrames	= new ImageCacheFrame[count];
		m_nFrames	= count;
		m_curFrame	= 0;
		m_frameSize	= frameSize;

		for (int i = 0 ; i < count ; i++)
		{
			m_pFrames[i].m_frame = -1;
			m_pFrames[i].m_pFrame = new unsigned char[frameSize];
		}
	}

	void GetCurrentFrame(unsigned char * pFrame)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		memmove(pFrame, m_pFrames[m_curFrame].m_pFrame, m_frameSize);
	}

	bool m_lockImageCache = false;		// Prevent frames from being added when doing a dump

	void PutFrame(int frameNo, unsigned char * pFrame)
	{
		if (!m_lockImageCache)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			m_curFrame	= (m_curFrame + 1) % m_nFrames;

			memmove(m_pFrames[m_curFrame].m_pFrame, pFrame, m_frameSize);
			m_pFrames[m_curFrame].m_frame = frameNo;
		}
	}

	bool GetFrame(int frameNo, unsigned char * pFrame)
	{
		bool ret = false;
		std::lock_guard<std::mutex> lock(m_mutex);

		for (int i = 0 ; i < m_nFrames ; i++)
		{
			if (frameNo == m_pFrames[i].m_frame)
			{
				memmove(pFrame, m_pFrames[i].m_pFrame, m_frameSize);
				ret	= true;
				break;
			}
		}

		return(ret);
	}
};

class Camera : public raspicam::RaspiCam
{
public:
	int m_framesLost = 0;
private:
	int m_brightness	= 50;
	int m_saturation	= 100;
	int m_iso			= 800;
	int m_width			= 0;
	int	m_height		= 0;
	int m_fpsCount		= 500;
	double m_processTime = 0;
//	double m_grabTime = 0;
	float m_FPS			= 0;
	ImageRegions * m_pRegions = 0;
	unsigned int m_frameCount	= 0;
	unsigned int m_lastFrame	= 0;
	unsigned int m_dataSize	= 0;
	unsigned int m_curFrame = 0;
	unsigned char * m_pData	= 0;
	unsigned char * m_pProcessData = 0;
	thread * m_pThread = 0;
	thread * m_pProcessThread = 0;
	mutex m_dataMutex;
	mutex m_grabMutex;
	mutex m_processMutex;
	mutex m_processDataMutex;
	double m_time;
	bool m_processImage = true;
	int m_sampleX	= 0;
	int m_sampleY 	= 0;
	int m_sampleR;
	int m_sampleG;
	int m_sampleB;
	float m_redBalance = 1.0;
	float m_blueBalance = 1.0;
	bool m_autoColorBalance = true;
	int m_exposureComp = 0;
	int m_shutterSpeed = 0;	//5000;
	int m_contrast = 0;
	bool m_autoExposure = true;
	bool m_saveNextImage = false;
	std::atomic<int> m_imagePreSaveCount;
	std::atomic<int> m_imagePostSaveCount;
	ImageCache m_imageCache;
	bool m_threadRun = true;
	int64_t m_grabTime = 0;
	int64_t m_captureTime = 0;


	static void ThreadStart(Camera * pCamera)
	{
		pCamera->ThreadRun();
	}

	static void ProcessStart(Camera * pCamera)
	{
		pCamera->ProcessRun();
	}


	void ThreadRun()
	{
	    open();

	    sleep(3);

	    m_dataSize = getImageBufferSize(); // Header + Image Data + Padding
	    m_pData = new unsigned char[m_dataSize];

	    m_imageCache.SetCacheSize(DefaultImageCacheSize, m_dataSize);

#ifdef TIME_PROCESS
		grab();
		retrieve(m_pData);

		double time = GetTimer();

		RECT b;
		int i;

		for (i = 0 ; i < 100 ; i++)
		{
			ProcessImage(m_pData, m_width, m_height, m_width * 3, &b, false, BlobPosition_Left);
		}

		printf("process fps = %f\n", i / (GetTimer() - time));

		time	= GetTimer();
#endif	// TIME_PROCESS

	    m_time = GetTimer();

	    while (m_threadRun)
	    {
	    	{
	    		std::lock_guard<std::mutex> lock(m_grabMutex);
				grab();
	    	}


			{
				std::lock_guard<std::mutex> lock(m_dataMutex);
				retrieve (m_pData);

				m_frameCount++;
				m_grabTime = GetTimeMs();
			}
	    }

	    printf("Grab thread exit\n");
	}

	void SaveImage(int frameNo, const char * pType, unsigned char * pFrame)
	{
		char path[256];

		snprintf(path, sizeof(path), "/home/pi/Images/Image%04d-%s.ppm", frameNo, pType);
		SaveImageToFile (path, pFrame);

		printf("Frame saved: %s\n", path);
	}

#define LOGREGIONS
#ifdef LOGREGIONS
	FILE * logFp = 0;
	double startTime;
	int logNo = 0;

public:
	void StartLog()
	{
		if (logFp == 0)
		{
			EndLog();
		}

		char file[32];

		printf("Creating log file\n");

		snprintf(file, sizeof(file), "visionlog%d.csv", logNo++);

		logFp = fopen(file, "w");
		startTime = GetTimer();

		if (logFp == 0)
		{
			printf("Cannot create log file\n");
		}
	}

	void EndLog()
	{
		if (logFp != 0)
		{
			fclose(logFp);
			logFp = 0;
		}
	}

private:
	void LogRegions(ImageRegions * pRegions, int frameNo)
	{
		if (logFp != 0)
		{
			long time = (int) ((GetTimer() - startTime) * 1000);

			if (pRegions->m_nRegions > 0)
			{
				fprintf(logFp, "%ld,%d,%d,%d,%d,%d\n", time, frameNo,
															pRegions->m_pRegions[0].m_bounds.left,
															pRegions->m_pRegions[0].m_bounds.top,
															pRegions->m_pRegions[0].m_bounds.right,
															pRegions->m_pRegions[0].m_bounds.bottom);
			}
			else
			{
				fprintf(logFp, "%ld: no regions\n", time);
			}

			fflush(logFp);
		}
	}
#endif	// LOGREGIONS


#define LOGMOVEMENTx
#ifdef LOGMOVEMENT
	int logMovementX = 0;
	int logMovementY = 0;
	int logMovementCount = 0;
	int logMovementState = 0;

	void DumpImages()
	{
		int start = (m_imageCache.m_curFrame  + 1) % m_imageCache.m_nFrames;
		int idx = start;
		char path[512];

		do
		{
			ImageCacheFrame * pFrame = &m_imageCache.m_pFrames[idx];

			sprintf(path, "ImageDump%04d.ppm", pFrame->m_frame);

			SaveImageToFile (path, pFrame->m_pFrame);

			printf("%d frame dummped\n", pFrame->m_frame);

			idx = (idx + 1) % m_imageCache.m_nFrames;
		} while (idx != start);

		exit(0);
	}

	void LogMovement(ImageRegions * pRegions, int frameNo)
	{
		if (frameNo > m_imageCache.m_nFrames && pRegions->m_nRegions > 0)
		{
			if (logMovementState == 0)
			{
				logMovementState = 1;
				logMovementX = pRegions->m_pRegions[0].m_bounds.left;
				logMovementY = pRegions->m_pRegions[0].m_bounds.top;
			}

			if (logMovementState == 1)
			{
				int dx = abs(pRegions->m_pRegions[0].m_bounds.left - logMovementX);
				int dy = abs(pRegions->m_pRegions[0].m_bounds.top - logMovementY);

				if (dx > 10)
				{
					logMovementState = 2;
					printf("Movement detected: %d", dx);
				}
			}

			if (logMovementState == 2)
			{
				if (logMovementCount++ >= m_imageCache.m_nFrames - 5)
				{
					DumpImages();
				}
			}
		}
	}
#endif

	void ProcessRun()
	{
    	int nFramesProcessed = 0;
    	int firstFrame = 0;
    	int prevFrame = -1;

		while (m_threadRun)
		{
			int sampleR;
			int sampleG;
			int sampleB;
			int lastFrame;
			int64_t grabTime;

			bool newFrame = false;

			ImageRegions * pRegions = 0;

			{
				std::lock_guard<std::mutex> lock(m_processMutex);

				{
					std::lock_guard<std::mutex> lock(m_dataMutex);

					if (m_lastFrame != m_frameCount)
					{
						if (!m_pProcessData)
						{
							m_pProcessData = new unsigned char[m_dataSize];
						}

						memmove(m_pProcessData, m_pData, m_dataSize);
						newFrame	= true;
						lastFrame = m_frameCount;
						grabTime = m_grabTime;
					}

				}

				if (newFrame)
				{
					m_saveNextImage = false;

					unsigned char * pPixel = m_pProcessData + (((m_sampleY * m_width) + m_sampleX) * 3);

					if ((prevFrame != -1) && (lastFrame != (prevFrame + 1)))
					{
						m_framesLost += (lastFrame - prevFrame - 1);
						printf("Lost frame: %d %d %d\n", prevFrame, lastFrame, m_framesLost);
					}
					prevFrame = lastFrame;

					nFramesProcessed++;

					sampleR	= RED(pPixel);
					sampleG	= GREEN(pPixel);
					sampleB	= BLUE(pPixel);

					if (m_imagePreSaveCount)
					{
						SaveImage(lastFrame, "r", m_pProcessData);

						m_imagePreSaveCount--;
					}

					pRegions	= new ImageRegions(g_maxRegions);

					ProcessImage(m_pProcessData, m_width, m_height, m_width * 3, m_processImage, pRegions);
					pRegions->Reflect(m_width);

#ifdef LOGREGIONS
					LogRegions(pRegions, lastFrame);
#endif	// LOGREGIONS
					if (pRegions->m_nRegions == 0)
					{
//						printf("No regions\n");
					}

					PutFrame(lastFrame, m_pProcessData);

#ifdef LOGMOVEMENT
					LogMovement(pRegions, lastFrame);
#endif	// LOGMOVEMENT

					if (m_imagePostSaveCount)
					{
						SaveImage(lastFrame, "p", m_pProcessData);

						m_imagePostSaveCount--;
					}

					if (nFramesProcessed == m_fpsCount)
					{
						double	dt	= GetTimer() - m_time;
						int nGrabbed = lastFrame - firstFrame;

						m_FPS = nFramesProcessed / dt;

						printf("FPS = %0.2f, GPS = %0.2f, proc = %d, grabbed = %d\n", m_FPS, nGrabbed / dt, nFramesProcessed, nGrabbed);

						m_time = GetTimer();
						nFramesProcessed = 0;
						firstFrame = lastFrame;
					}
				}
			}

			if (newFrame)
			{
				std::lock_guard<std::mutex> lock(m_processDataMutex);

				m_sampleR		= sampleR;
				m_sampleG		= sampleG;
				m_sampleB		= sampleB;

				delete m_pRegions;
				m_pRegions		= pRegions;
				m_captureTime	= grabTime;
				pRegions		= 0;

				m_lastFrame		= lastFrame;
			}
			else
			{
			  sleepMs(5);
			}
		}

		printf("Process thread exit\n");
	}

public:
	void DumpImages(int count)
	{
		m_imageCache.m_lockImageCache = true;

		if (count >= m_imageCache.m_nFrames)
		{
			count = m_imageCache.m_nFrames - 1;
		}

		int start = m_imageCache.m_curFrame - count;

		if (start < 0)
		{
			start += m_imageCache.m_nFrames;
		}

		int idx = start;
		char path[512];

		while (true)
		{
			ImageCacheFrame * pFrame = &m_imageCache.m_pFrames[idx];

			sprintf(path, "ImageDump%04d.ppm", pFrame->m_frame);

			SaveImageToFile (path, pFrame->m_pFrame);

			printf("%d: %d frame dummped\n", idx, pFrame->m_frame);

			if (idx == m_imageCache.m_curFrame)
			{
				break;
			}

			idx = (idx + 1) % m_imageCache.m_nFrames;

		}

		m_imageCache.m_lockImageCache = false;
	}


	Camera()
	{
		m_imagePreSaveCount	= 0;
		m_imagePostSaveCount = 0;
	}

	void Stop()
	{
		m_threadRun	= false;
	}

	void SetCacheSize(int size)
	{
		m_imageCache.SetCacheSize(size, m_dataSize);
	}

	void ResetFrameCount()
	{
		std::lock_guard<std::mutex> lock(m_dataMutex);

		m_frameCount	= 0;
	}

	void PutFrame(int frameNo, unsigned char * pFrame)
	{
		m_imageCache.PutFrame(frameNo, pFrame);
	}

	bool GetFrame(int frameNo, unsigned char * pFrame)
	{
		return(m_imageCache.GetFrame(frameNo, pFrame));
	}

	void SaveFrameNo(int frameNo)
	{
		unsigned char * pFrame = new unsigned char[m_dataSize];

		if (GetFrame(frameNo, pFrame))
		{
			SaveImage(frameNo, "f", pFrame);
		}
		else
		{
			printf("Frame %d not found\n", frameNo);
		}

		delete[] pFrame;
	}

	void SetImageSaveCount(int preCount, int postCount)
	{
		m_imagePreSaveCount	= preCount;
		m_imagePostSaveCount = postCount;
	}

	void SaveNextImage()
	{
		m_saveNextImage	= true;
	}

	void GetSamplePixel(int& red, int& green, int& blue)
	{
		std::lock_guard<std::mutex> lock(m_dataMutex);

		red	= m_sampleR;
		green = m_sampleG;
		blue = m_sampleB;
	}

	void SetSamplePixel(int x, int y)
	{
		std::lock_guard<std::mutex> lock(m_dataMutex);

		if (x < 0)
		{
			x	= 0;
		}
		else if (x >= m_width)
		{
			x	= m_width - 1;
		}

		if (y < 0)
		{
			y	= 0;
		}
		else if (y >= m_height)
		{
			y	= m_height - 1;
		}

		m_sampleX	= x;
		m_sampleY	= y;
	}

	void CommitAutoColorBalance()
	{
	    if (m_autoColorBalance)
	    {
	    	setAWB(raspicam::RASPICAM_AWB_FLUORESCENT);
	    }
	    else
	    {
	    	setAWB(raspicam::RASPICAM_AWB_OFF);
	    	setAWB_RB(m_redBalance, m_blueBalance);
	    }
	}

	void Init(int width, int height, int frameRate)
	{
		m_width		= width;
		m_height	= height;

	    setWidth ( width );
	    setHeight ( height );
	    setFrameRate(frameRate);

	    CommitParams();
	}

	void CommitParams()
	{
	    setExposure(m_autoExposure ? raspicam::RASPICAM_EXPOSURE_AUTO : raspicam::RASPICAM_EXPOSURE_OFF);
	    setShutterSpeed(m_shutterSpeed);
	    setISO(m_iso);
	    setBrightness(m_brightness);
	    setSaturation(m_saturation);
	    setHorizontalFlip(true);
	    setContrast(m_contrast);

	    CommitAutoColorBalance();

	}

	void StartCapture()
	{
		if (!m_pThread)
		{
			m_pThread = new std::thread(ThreadStart, this);
		}

		if (!m_pProcessThread)
		{
			m_pProcessThread = new std::thread(ProcessStart, this);
		}
	}

	int GetFrameSize(int& width, int& height)
	{
		width	= m_width;
		height	= m_height;

		return(m_dataSize);
	}

	void GetFrame(unsigned char * pData)
	{
		if (m_dataSize)
		{
			std::lock_guard<std::mutex> lock(m_processMutex);

			memcpy(pData, m_pProcessData, m_dataSize);
		}
	}

	ImageRegions * GetFrameRegions(int * pLastFrame, int64_t * pCaptureTime)
	{
		std::lock_guard<std::mutex> lock(m_processDataMutex);

		*pLastFrame	= m_lastFrame;
		*pCaptureTime = m_captureTime;


		if (m_pRegions)
		{
			return(m_pRegions->Clone());
		}

		return(0);
	}

	bool GetProcessImage()
	{
		return(m_processImage);
	}

	void SetProcessImage(bool process)
	{
		std::lock_guard<std::mutex> lock(m_dataMutex);

		m_processImage	= process;
	}

	int GetBrightness()
	{
		return(m_brightness);
	}

	float GetRedBalance()
	{
		return(m_redBalance);
	}

	float GetBlueBalance()
	{
		return(m_blueBalance);
	}

	int GetAutoColorBalance()
	{
		return(m_autoColorBalance);
	}

	int GetAutoExposure()
	{
		return(m_autoExposure);
	}

	int GetExposureComp()
	{
		return(m_exposureComp);
	}

	int GetContrast()
	{
		return(m_contrast);
	}

	int GetShutterSpeed()
	{
		return(m_shutterSpeed);
	}

	float GetFPS()
	{
		return(m_FPS);
	}

	/*
	 *  Must be called before Init()
	 */
	void SetBrightness(int brightness)
	{
		m_brightness = brightness;

	}

	void SetSaturation(int saturation)
	{
		m_saturation = saturation;
	}

	void SetBlueBalance(float value)
	{
		m_blueBalance	= value;
	}

	void SetRedBalance(float value)
	{
		m_redBalance	= value;
	}

	void SetAutoColorBalance(int value)
	{
		m_autoColorBalance	= value;
	}

	void SetAutoExposure(int value)
	{
		m_autoExposure = value;
	}

	void SetShutterSpeed(int value)
	{
		m_shutterSpeed	= value;
	}

	void SetISO(int iso)
	{
		m_iso	= iso;
	}

	void SetContrast(int contrast)
	{
		m_contrast	= contrast;
	}

	int IncrementBrightness(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_brightness	+= amount;

		if (m_brightness < 0)
		{
			m_brightness	= 0;
		}
		else if (m_brightness > 100)
		{
			m_brightness	= 100;
		}

		setBrightness(m_brightness);

		return(m_brightness);
	}

	int GetSaturation()
	{
		return(m_saturation);
	}

	int IncrementSaturation(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_saturation	+= amount;

		if (m_saturation < -100)
		{
			m_saturation	= -100;
		}
		else if (m_saturation > 100)
		{
			m_saturation	= 100;
		}

		setSaturation(m_saturation);

		return(m_saturation);
	}

	int IncrementExposureComp(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_exposureComp	+= amount;

		if (m_exposureComp < -10)
		{
			m_exposureComp	= -10;
		}
		else if (m_exposureComp > 10)
		{
			m_exposureComp	= 10;
		}

		setExposureCompensation(m_exposureComp);

		return(m_exposureComp);
	}

	int IncrementShutterSpeed(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_shutterSpeed	+= amount;

		if (m_shutterSpeed < 0)
		{
			m_shutterSpeed	= 0;
		}

		setShutterSpeed(m_shutterSpeed);

		return(m_shutterSpeed);
	}

	void ToggleAutoExposure()
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_autoExposure	= !m_autoExposure;
		if (m_autoExposure)
		{
			setExposure(raspicam::RASPICAM_EXPOSURE_AUTO);
			printf("Auto exposure on\n");
		}
		else
		{
			setExposure(raspicam::RASPICAM_EXPOSURE_OFF);
			printf("Auto exposure off\n");
		}
	}

	void ToggleAutoColorBalance()
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_autoColorBalance	= !m_autoColorBalance;

		CommitAutoColorBalance();
	}

	int IncrementContrast(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_contrast	+= amount;

		if (m_contrast < -100)
		{
			m_contrast	= -100;
		}
		else if (m_contrast > 100)
		{
			m_contrast	= 100;
		}

		setContrast(m_contrast);

		return(m_contrast);
	}

	float IncrementBlueBalance(float amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_blueBalance += amount;

		if (m_blueBalance > 1.0)
		{
			m_blueBalance	= 1.0;
		}
		else if (m_blueBalance < 0)
		{
			m_blueBalance	= 0;
		}

		if (!m_autoColorBalance)
		{
			setAWB_RB (m_redBalance, m_blueBalance );//range is 0-1.
		}

        return(m_blueBalance);

	}

	float IncrementRedBalance(float amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_redBalance += amount;

		if (m_redBalance > 1.0)
		{
			m_redBalance	= 1.0;
		}
		else if (m_redBalance < 0.0)
		{
			m_redBalance	= 0.0;
		}

		if (!m_autoColorBalance)
		{
			setAWB_RB (m_redBalance, m_redBalance );//range is 0-1.
		}

        return(m_redBalance);

	}


	int GetISO()
	{
		return(m_iso);
	}

	int IncrementISO(int amount)
	{
		std::lock_guard<std::mutex> lock(m_grabMutex);

		m_iso	+= amount;

		if (m_iso < 100)
		{
			m_iso	= 100;
		}
		else if (m_iso > 800)
		{
			m_iso	= 800;
		}

		setISO(m_iso);

		return(m_iso);
	}

	int IncrementHSV(int& type, int amount)
	{
		std::lock_guard<std::mutex> lock(m_dataMutex);

		type	+= amount;

		if (type < 0)
		{
			type = 0;
		}
		else if (type > 255)
		{
			type = 255;
		}

		CreateFilterTable(&filters);

		return(type);
	}

	void SaveImageToFile (char * filepath,unsigned char *data)
	{
	    std::ofstream outFile ( filepath, std::ios::binary );
	    if ( getFormat()==raspicam::RASPICAM_FORMAT_BGR ||  getFormat()==raspicam::RASPICAM_FORMAT_RGB ) {
	        outFile<<"P6\n";
	    } else if ( getFormat()==raspicam::RASPICAM_FORMAT_GRAY ) {
	        outFile<<"P5\n";
	    } else if ( getFormat()==raspicam::RASPICAM_FORMAT_YUV420 ) { //made up format
	        outFile<<"P7\n";
	    }
	    printf("SaveImageToFile: width = %d, height = %d\n", getWidth(), getHeight());
	    outFile << getWidth() << " " << getHeight() << " 255\n";
	    outFile.write ( ( char* ) data, getImageBufferSize() );
	}


	void SaveImage()
	{
		char file[512];
		unsigned char * pData = (unsigned char *) malloc(m_dataSize);

		GetFrame(pData);

		snprintf(file, sizeof(file), "image%04d.ppm", m_frameCount);
		printf("Writing: %s\n", file);

		SaveImageToFile(file, pData);

		free(pData);
	}

} Camera;

#define ERROR -1
#define PortNo	5800

class SocketServer
{
//public:
	int m_serverSocket;
	int m_connectedSocket;
	std::thread * m_pThread = 0;
	mutex m_mutex;

	static void StartThread(SocketServer * pServer)
	{
		try
		{
			pServer->Run();
		}
		catch (...)
		{
			printf("StartThread catch\n");
		}
	}


public:
	SocketServer()
	{
		m_serverSocket		= ERROR;
		m_connectedSocket	= ERROR;
	}

	virtual void Run() = 0;

	bool listen(int port)
	{
		struct sockaddr_in serverAddr;
		int sockAddrSize = sizeof(serverAddr);
		memset(&serverAddr, 0, sockAddrSize);

		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if ((m_serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == ERROR)
		{
			printf("Error creating server socket: %d", errno);
			return(false);
		}

		// Set the TCP socket so that it can be reused if it is in the wait state.
		int reuseAddr = 1;
		setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(reuseAddr));

		int one = 1;
		setsockopt(m_serverSocket, SOL_TCP, TCP_NODELAY, &one, sizeof(one));

		// Bind socket to local address.
		if (bind(m_serverSocket, (struct sockaddr *)&serverAddr, sockAddrSize) == ERROR)
		{
			::close(m_serverSocket);
			m_serverSocket = ERROR;
			printf("Could not bind server socket: %d", errno);
			return(false);
		}

		if (::listen(m_serverSocket, 1) == ERROR)
		{
			::close(m_serverSocket);
			m_serverSocket	= ERROR;
			printf("Could not listen on server socket: %d", errno);
			return(false);
		}

		return(true);
	}

	bool accept()
	{
		if (m_serverSocket == ERROR)
		{
			return(ERROR);
		}

		struct sockaddr clientAddr;
		memset(&clientAddr, 0, sizeof(struct sockaddr));
		unsigned int clientAddrSize = sizeof(clientAddr);
		m_connectedSocket = ::accept(m_serverSocket, &clientAddr, &clientAddrSize);

		if (m_connectedSocket == ERROR)
			return false;

		return(true);
	}

	int read(void * pBuf, int size)
	{
		int socket;

		{
			std::lock_guard<std::mutex> lock(m_mutex);

			socket = m_connectedSocket;

		}

		if (socket == ERROR)
		{
			return(ERROR);
		}

		return(::read(socket, pBuf, size));
	}

	int write(void *pBuf, int size)
	{
		int socket;

		{
			std::lock_guard<std::mutex> lock(m_mutex);

			socket = m_connectedSocket;

		}

		if (socket == ERROR)
		{
			return(ERROR);
		}

		return(::write(socket, pBuf, size));
	}

	void Disconnect()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_connectedSocket != ERROR)
		{
			printf("ImageServer: Disconnect\n");

			::shutdown(m_connectedSocket,SHUT_RDWR);
			::close(m_connectedSocket);

			m_connectedSocket = ERROR;
		}

	}

	void StartServer()
	{
		if (!m_pThread)
		{
			m_pThread = new thread(StartThread, this);
		}
	}

	void StopServer()
	{
		delete m_pThread;
		m_pThread	= 0;
	}
};

int ImageLinePos	= 100;
int ImageCenterPos	= 320;



#ifdef UseGraphics
class Graphics
{
public:
	int m_width		= 0;
	int	m_height	= 0;
	bool m_init		= false;

	void Clear()
	{
		Start(m_width, m_height);				// Start the picture
		VGfloat color[4] = { 255, 255, 255, 0 };
		vgSetfv(VG_CLEAR_COLOR, 4, color);
		vgClear(0, 0, m_width, m_height);

	}

	void Init()
	{
		if (!m_init)
		{
			init(&m_width, &m_height);			// Graphics initialization

			Clear();

			m_init	= true;
		}
	}

	unsigned char * ConvertRGBtoRGBA(unsigned char * pData, int width, int height)
	{
		int					x;
		int					y;
		unsigned char	*	pRGBAData	= (unsigned char *) malloc(width * height * 4);

		for (y = 0 ; y < height ; y++)
		{
			unsigned char	*	d 	= pRGBAData + ((height - y) * width * 4);
			unsigned char	*	s	= pData + (y * width * 3);

			for (x = 0 ; x < width ; x++, d -= 4, s += 3)
			{
				d[-4]	= 255;
				d[-3]	= s[2];
				d[-2]	= s[1];
				d[-1]	= s[0];
			}
		}

		return(pRGBAData);
	}


	VGImage CreateImageFromData(unsigned char * pData, int width, int height)
	{

		/*
		 * Convert to ABGR
		 */
		unsigned char	*	pRGBAData	= ConvertRGBtoRGBA(pData, width, height);	//(unsigned char *) malloc(width * height * 4);

		VGImage	img = vgCreateImage(VG_sRGBA_8888, width, height, VG_IMAGE_QUALITY_BETTER);
		vgImageSubData(img, pRGBAData, width * 4, VG_sRGBA_8888, 0, 0, width, height);

		free(pRGBAData);

		return(img);
	}


	void DisplayFrame(unsigned char * pData, int width, int height)
	{
		Init();

		VGImage img = CreateImageFromData(pData, width, height);

		Start(width, height);
		vgSetPixels(m_width - width, 0, img, 0, 0, width, height);
		End();

		vgDestroyImage(img);

	}
} graphics;
#endif	// UseGraphics

//#define CameraParamFile	"/home/pi/cameraparam"

class Commands
{
	enum Command
	{
		Command_None,
		Command_CameraBrightness,
		Command_CameraSaturation,
		Command_CameraISO,
		Command_MinHue,
		Command_MaxHue,
		Command_MinValue,
		Command_MaxValue,
		Command_MinSaturation,
		Command_MaxSaturation,
		Command_TargetLine,
		Command_BlueBalance,
		Command_RedBalance,
		Command_ExposureComp,
	} CurCommand = Command_None;

	bool showRectangle  = false;
	bool showImage		= false;

public:
	char * GetParamFile()
	{
		static char path[128];

		snprintf(path, sizeof(path), "/home/pi/cameraparam%d.txt", g_profile);

		return(path);
	}

	void ReadParams()
	{
		FILE	*	fp = fopen(GetParamFile(), "r");


		if (fp)
		{
			char line[512];
			int	 value;
			int	 color;
			int	 min;
			int	 max;
			float fValue;

			while (fgets(line, sizeof(line), fp))
			{
				switch (line[0])
				{
				case 'B':
					if (sscanf(line, "B %d", &value) == 1)
					{
						Camera.SetBrightness(value);
					}
					break;

				case 'c':
					if (sscanf(line, "c %d", &value) == 1)
					{
						Camera.SetSaturation(value);
					}
					break;

				case 'C':
					if (sscanf(line, "C %d", &value) == 1)
					{
						Camera.SetContrast(value);
					}
					break;

				case 'I':
					if (sscanf(line, "I %d", &value) == 1)
					{
						Camera.SetISO(value);
					}
					break;

				case 'H':
					sscanf(line, "H %d %d %d", &color, &min, &max);

					if ((color >= 0) && (color < DIM(filters.m_filters)))
					{
						filters.m_filters[color].m_minH	= min;
						filters.m_filters[color].m_maxH = max;
					}
					break;

				case 'S':
					sscanf(line, "S %d %d %d", &color, &min, &max);

					if ((color >= 0) && (color < DIM(filters.m_filters)))
					{
						filters.m_filters[color].m_minS	= min;
						filters.m_filters[color].m_maxS = max;
					}
					break;

				case 'V':
					sscanf(line, "V %d %d %d", &color, &min, &max);

					if ((color >= 0) && (color < DIM(filters.m_filters)))
					{
						filters.m_filters[color].m_minV	= min;
						filters.m_filters[color].m_maxV = max;
					}
					break;

				case 'L':
					sscanf(line, "L %d", &ImageLinePos);
					break;

				case 'z':
					sscanf(line, "z %d", &ImageCenterPos);
					break;

				case 'r':
					sscanf(line, "r %f", &fValue);
					Camera.SetRedBalance(fValue);
					break;

				case 'b':
					sscanf(line, "b %f", &fValue);
					Camera.SetBlueBalance(fValue);
					break;

				case 'a':
					sscanf(line, "a %d", &value);
					Camera.SetAutoColorBalance(value);
					break;

				case 'm':
					sscanf(line, "m %d", &value);
					MinArea	= value;
					break;

				case 'E':
					sscanf(line, "E %d", &value);
					Camera.SetShutterSpeed(value);
					break;

				case 'A':
					sscanf(line, "A %d", &value);
					Camera.SetAutoExposure(value);
					break;

				case 'R':
					sscanf(line, "R %d", &value);
					g_maxRegions	= value;
					break;
				}
			}
		}
	}

	void WriteParams()
	{
		FILE	*	fp = fopen(GetParamFile(), "w");

		if (fp)
		{
			for (int c = 0 ; c < DIM(filters.m_filters) ; c++)
			{
				if (filters.m_filters[c].m_maxV)
				{
					fprintf(fp, "H %d %d %d\n", c, filters.m_filters[c].m_minH, filters.m_filters[c].m_maxH);
					fprintf(fp, "S %d %d %d\n", c, filters.m_filters[c].m_minS, filters.m_filters[c].m_maxS);
					fprintf(fp, "V %d %d %d\n", c, filters.m_filters[c].m_minV, filters.m_filters[c].m_maxV);
				}
			}
			fprintf(fp, "B %d\n", Camera.GetBrightness());
			fprintf(fp, "c %d\n", Camera.GetSaturation());
			fprintf(fp, "I %d\n", Camera.GetISO());
			fprintf(fp, "L %d\n", ImageLinePos);
			fprintf(fp, "r %f\n", Camera.GetRedBalance());
			fprintf(fp, "b %f\n", Camera.GetBlueBalance());
			fprintf(fp, "a %d\n", Camera.GetAutoColorBalance());
			fprintf(fp, "m %d\n", MinArea);
			fprintf(fp, "E %d\n", Camera.GetShutterSpeed());
			fprintf(fp, "C %d\n", Camera.GetContrast());
			fprintf(fp, "A %d\n", Camera.GetAutoExposure());
			fprintf(fp, "z %d\n", ImageCenterPos);
			fprintf(fp, "R %d\n", g_maxRegions);

			fclose(fp);

			printf("Camera parameters written\n");
		}
		else
		{
			printf("Cannot write cameraparam-test.txt\n");
		}
	}

	int IncrementTargetLine(int amount)
	{
		ImageLinePos	+= amount;
		if (ImageLinePos < 0)
		{
			ImageLinePos = 0;
		}
		else if (ImageLinePos > 398)
		{
			ImageLinePos = 398;
		}

		return(ImageLinePos);
	}

	int IncrementCenterPos(int amount)
	{
		ImageCenterPos	+= amount;
		if (ImageCenterPos < 0)
		{
			ImageCenterPos = 0;
		}
		else if (ImageCenterPos > CaptureWidth-1)
		{
			ImageCenterPos = CaptureWidth-1;
		}

		return(ImageCenterPos);
	}

private:
	void ChangeLevel(Command command, int amount)
	{
		int level;
		float fLevel;

		switch (command)
		{
		case Command_TargetLine:
			IncrementTargetLine(amount);
			printf("target line = %d\n", ImageLinePos);
			break;

		case Command_CameraBrightness:
			level	= Camera.IncrementBrightness(amount);
			printf("brightness = %d\n", level);
			break;

		case Command_CameraSaturation:
			level	= Camera.IncrementSaturation(amount);
			printf("saturation = %d\n", level);
			break;

		case Command_CameraISO:
			level	= Camera.IncrementISO(amount * 10);
			printf("ISO = %d\n", level);
			break;

		case Command_MinHue:
			level = Camera.IncrementHSV(MinH, amount);
			printf("MinH = %d, MaxH = %d\n", MinH, MaxH);
			break;

		case Command_MaxHue:
			level = Camera.IncrementHSV(MaxH, amount);
			printf("MinH = %d, MaxH = %d\n", MinH, MaxH);
			break;

		case Command_MinValue:
			level = Camera.IncrementHSV(MinV, amount);
			printf("MinV = %d, MaxV = %d\n", MinV, MaxV);
			break;

		case Command_MaxValue:
			level = Camera.IncrementHSV(MaxV, amount);
			printf("MinV = %d, MaxV = %d\n", MinV, MaxV);
			break;

		case Command_MinSaturation:
			level = Camera.IncrementHSV(MinS, amount);
			printf("MinS = %d, MaxS = %d\n", MinS, MaxS);
			break;

		case Command_MaxSaturation:
			level = Camera.IncrementHSV(MaxS, amount);
			printf("MinS = %d, MaxS = %d\n", MinS, MaxS);
			break;

		case Command_BlueBalance:
			fLevel = Camera.IncrementBlueBalance(amount * 0.01);
			printf("Blue balance = %f\n", fLevel);
			break;

		case Command_RedBalance:
			fLevel = Camera.IncrementRedBalance(amount * 0.01);
			printf("Red balance = %f\n", fLevel);
			break;

//		case Command_ExposureComp:
//			level = Camera.IncrementShutterSpeed(amount * 100);
//			printf("Shutter Speed = %d\n", level);
//			break;

		case Command_ExposureComp:
			level = Camera.IncrementExposureComp(amount);
			printf("Exposure Comp = %d\n", level);
			break;

		default:
			break;

		}
	}

public:
	bool ProcessCommand()
	{
		int ch;
		bool process;

		switch (ch = getkey())
		{
		case 'q':
			return(false);

		case 'w':
			WriteParams();
			break;

		case 'p':
			process	= !Camera.GetProcessImage();
			Camera.SetProcessImage(process);
			printf(process ? "Processing on\n" : "Processing off\n");
			break;

		case ' ':
			Camera.SaveImage();
			break;

		case 't':
			showRectangle = !showRectangle;
			printf(showRectangle ? "Show Rectangle\n" : "Hide Rectangle\n");
			break;

#ifdef UseGraphics
		case 'x':
			showImage = !showImage;
			if (!showImage)
			{
				graphics.Clear();
			}
			printf(showImage ? "Show Image" : "Hide Image");
			break;
#endif	// UseGraphics

		case 'l':
			printf("Move target line\n");
			CurCommand	= Command_TargetLine;
			break;

		case 'e':
			printf("Set Exposure Compensation\n");
			CurCommand = Command_ExposureComp;
			break;

		case 'B':
			printf("Set Camera Blue Balance\n");
			CurCommand = Command_BlueBalance;
			break;

		case 'R':
			printf("Set Camera Red Balance\n");
			CurCommand = Command_RedBalance;
			break;

		case 'b':
			printf("Set Camera Brightness\n");
			CurCommand	= Command_CameraBrightness;
			break;

		case 'c':
			printf("Set Camera Saturation\n");
			CurCommand = Command_CameraSaturation;
			break;

		case 'h':
			printf("Set Min Hue (%d)\n", MinH);
			CurCommand = Command_MinHue;
			break;

		case 'H':
			printf("Set Max Hue (%d)\n", MaxH);
			CurCommand = Command_MaxHue;
			break;

		case 'v':
			printf("Set Min Value (%d)\n", MinV);
			CurCommand = Command_MinValue;
			break;

		case 'V':
			printf("Set Max Value (%d)\n", MaxV);
			CurCommand = Command_MaxValue;
			break;

		case 's':
			printf("Set Min Saturation (%d)", MinS);
			CurCommand = Command_MinSaturation;
			break;

		case 'S':
			printf("Set Max Saturation (%d)", MaxS);
			CurCommand = Command_MaxSaturation;
			break;

		case 'i':
			printf("Set Camera ISO\n");
			CurCommand = Command_CameraISO;
			break;

		case 'E':
			printf("Set Exposure Comp\n");
			CurCommand = Command_ExposureComp;
			break;

		case '+':
			ChangeLevel(CurCommand, 10);
			break;

		case '=':
			ChangeLevel(CurCommand, 1);
			break;

		case '-':
			ChangeLevel(CurCommand, -1);
			break;

		case '_':
			ChangeLevel(CurCommand, -10);
			break;

		case 'd':
			printf("Dumping images\n");
			Camera.DumpImages(DefaultImageCacheSize-1);
			break;
#ifdef GPIO
		case 'L':
			g_light = !g_light;
			digitalWrite (0, g_light ? LED_ON : LED_OFF);
			digitalWrite (1, g_light ? LED_ON : LED_OFF);
			printf("Toggle light: %d\n", g_light);
			break;
#endif	// GPIO

		case 0:
			break;

		default:
			printf("ch = %d\n", ch);
		}

		return(true);
	}

	bool ShowRectangle()
	{
		return(showRectangle);
	}

	bool ShowImage()
	{
		return(showImage);
	}
} Commands;

class ImageProcessingServer : public SocketServer
{
	bool m_connected = false;
	std::thread   * m_pRecvThread = 0;
	std::thread	  * m_pWatchDogThread = 0;

	static void StartRecvThread(ImageProcessingServer * pServer)
	{
		try
		{
			pServer->RunRecv();
		}
		catch (...)
		{
			printf("StartRecvThread catch\n");
		}
	}

	int kKeepAliveTimeout = 5000;

	static void StartWatchDogThread(ImageProcessingServer * pServer)
	{
		pServer->WatchDog();
	}

	void WatchDog()
	{
		printf("ImageServer: WatchDog started\n");

		while (m_connected)
		{
			sleepMs(1000);

			printf("ImageProcessingServer: WatchDog: %lld\n", m_keepAliveTime);

			if (m_keepAliveTime + kKeepAliveTimeout < GetTimeMs())
			{
				printf("WatchDog timeout\n");

				m_connected = false;

				Disconnect();
			}
		}

		printf("ImageServer: WatchDog ended\n");

	}

	void StartThreads()
	{
		m_pRecvThread = new thread(StartRecvThread, this);
		m_pWatchDogThread = new thread(StartWatchDogThread, this);
	}


public:
	void StopServerx()
	{
		delete m_pRecvThread;
		m_pRecvThread	= 0;

		SocketServer::StopServer();
	}

	void PingResponse()
	{
		printf("PingResponse\n");

		write((void *) "p\n", 2);
	}

	int64_t T1;
	int64_t T1P;
	int64_t T2;
	int64_t T2P;
	int64_t syncTimeOffset = 0;

	void SyncCommand(char * pArg)
	{
//		printf("SyncCommand: %s\n", pArg);

		if (pArg[0] == '1')
		{
			if (sscanf(pArg+1, "%Ld", &T1) == 1)
			{
				T2 =
				T1P = GetTimeMs();

				write((void *) "T\n", 2);
			}
			else
			{
				printf("ProcessSync: Invalid time string\n");
			}
		}
		else
		{
			if (sscanf(pArg+1, "%Ld", &T2P) == 1)
			{
				syncTimeOffset = (T1P - T1 + T2 - T2P) / 2;

				printf("ProcessSync: offset = %Ld\n", syncTimeOffset);
			}
			else
			{
				printf("ProcessSync: Invalid time string\n");
			}
		}
	}

	void SetProfile(int profile)
	{
		g_profile = profile;
		printf("Setting profile to %d\n", g_profile);
		Commands.ReadParams();
		CreateFilterTable(&filters);
		Camera.CommitParams();
	}

	void SetLight(int on)
	{
#ifdef GPIO
		g_light = on;
		printf("SetLight: %d\n", on);

		digitalWrite (0, (on & 1) ? LED_ON : LED_OFF);
		digitalWrite(1, (on & 2) ? LED_ON : LED_OFF);
#endif // GPIO
	}

#define kKeepAliveTimeout	5000

	int64_t m_keepAliveTime = 0;

	void RunRecv()
	{
		printf("ImageProcessingServer receiver started\n");

		while (m_connected)
		{
			int  len;
			char * p;
			char command[512];

			if ((len = read(command, sizeof(command) - 1)) == ERROR)
			{
				break;
			}

			if (len > 0)
			{
				command[len]	= 0;

				if ((command[0] != 'T') && (command[0] != '\n') && (command[0] != '\r'))
				{
					printf("command: %s\n", command);
				}

				p	= command;

				while (*p)
				{
					int arg1  = 0;
					int arg2	= 0;

					switch (p[0])
					{
					case 'L':
						if (sscanf(p + 1, "%d", &arg1) == 1)
						{
							SetLight(arg1);
							printf("Set Light: %d\n", arg1);
						}
						break;

					case 'P':
						if (sscanf(p + 1, "%d", &arg1) == 1)
						{
							SetProfile(arg1);
						}
						break;

					case 'w':
						sscanf(p + 1, "%d%d", &arg1, &arg2);

						printf("Save images %d pre and %d post\n", arg1, arg2);
						Camera.SetImageSaveCount(arg1, arg2);
						break;

					case 'f':
						sscanf(p + 1, "%d", &arg1);
						printf("Save frame %d\n", arg1);
						Camera.SaveFrameNo(arg1);
						break;

					case 'r':
						Camera.ResetFrameCount();
						printf("Reset frame count\n");
						break;

					case 'c':
						sscanf(p + 1, "%d", &arg1);
						Camera.SetCacheSize(arg1);
						printf("Set cache size: %d\n", arg1);
						break;

					case 'd':
						if (sscanf(p + 1, "%d", &arg1) == 1)
						{
							printf("Dumping last %d images\n", arg1);
							Camera.DumpImages(arg1);
						}
						break;

					case 'l':
						if (p[1] == 's')
						{
							Camera.StartLog();
						}
						else
						{
							Camera.EndLog();
						}
						break;

					case 'T':
						SyncCommand(command + 1);
						break;

					case 'p':	// ping
						PingResponse();
						break;

					case 'k':	// keep alive
						m_keepAliveTime = GetTimeMs();
						printf("ProcessingServer:: Keep Alive\n");
						break;
					}

					while (*p && (*p != '\n'))
					{
						p++;
					}

					if (*p)
					{
						p++;
					}
				}
			}
		}

		printf("ImageProcessingServer receiver exit\n");
	}

	void Run()
	{
		printf("ImageProcessingServer started\n");

		signal(SIGPIPE, SIG_IGN);

		try
		{
			if (listen(PortNo))
			{
				while (true)
				{
					printf("Waiting for processing connection\n");

					if (accept())
					{
						int frame = -1;

						printf("Connected\n");

						m_connected = true;

						StartThreads();

						ImageRegions	*	pRegions = 0;

						m_keepAliveTime = GetTimeMs();

						while (true)
						{
							int	nFrame;
							int64_t captureTime;

							sleepMs(5);

							pRegions = Camera.GetFrameRegions(&nFrame, &captureTime);

							if (nFrame != frame)
							{
								char msg[256];

//								printf("New Frame\n");

								snprintf(msg, sizeof(msg), "F %d %d %d %d %d %d %Ld %d %d\n",
																	nFrame, ImageLinePos, ImageCenterPos, CaptureWidth, CaptureHeight,
																	Camera.m_framesLost, captureTime - syncTimeOffset, (int) (GetTimeMs() - captureTime), g_profile);
								if (write((void *) msg, strlen(msg)) == ERROR)
								{
									printf("write failed\n");
									break;
								}

								if (pRegions)
								{
									for (int i = 0 ; i < pRegions->m_nRegions ; i++)
									{
										snprintf(msg, sizeof(msg), "R %d %d %d %d %d %d %d\n",
												pRegions->m_pRegions[i].m_color,
												pRegions->m_pRegions[i].m_bounds.left,
												pRegions->m_pRegions[i].m_bounds.top,
												pRegions->m_pRegions[i].m_bounds.right,
												pRegions->m_pRegions[i].m_bounds.bottom,
												pRegions->m_pRegions[i].m_topLeft,
												pRegions->m_pRegions[i].m_topRight);

										if (write((void *) msg, strlen(msg)) == ERROR)
										{
											printf("write failed\n");
											break;
										}
									}
								}

								if (write((void *) "E\n", 2) == ERROR)
								{
									printf("write failed\n");
									break;
								}

								frame = nFrame;
							}

							if (pRegions)
							{
								delete pRegions;
								pRegions	= 0;
							}

							if ((GetTimeMs() - m_keepAliveTime) > kKeepAliveTimeout)
							{
								printf("ProcessingServer: Keep Alive Timeout\n");
								break;
							}
						}

						if (pRegions)
						{
							delete pRegions;
							pRegions	= 0;
						}

						m_connected = false;

						Disconnect();

//						::shutdown(m_connectedSocket,SHUT_RDWR);
//						::close(m_connectedSocket);
//						m_connectedSocket = ERROR;

						printf("Processing Connection Lost\n");
					}
				}
			}
		}
		catch (...)
		{

		}
	}
} NetworkServer;

#define IMAGE_SERVER
#ifdef IMAGE_SERVER

struct ImageHeader
{
	unsigned long	imageSize;
	short			width;
	short			height;
	short			minH;
	short			maxH;
	short			minS;
	short			maxS;
	short			minV;
	short			maxV;
	short			cameraBrightness;
	short			cameraSaturation;
	short			cameraISO;
	short			targetPosition;
	short			pixelX;
	short			pixelY;
	short			pixelRed;
	short			pixelBlue;
	short			pixelGreen;
	short			pixelHue;
	short			pixelSat;
	short			pixelVal;
	float			camRedBalance;
	float			camBlueBalance;
	short			camAutoColorBalance;
	short			camExposureComp;
	short			camContrast;
	short			minArea;
	short			camShutterSpeed;
	short			maxRegions;
	float			fps;
	short			showFiltered;
	short			zoom;
	short			camAutoExposure;
	short			centerLine;
	short			color;
	short			framesLost;
	short			nRegions;
	short			profile;
};

class ImageServer : public SocketServer
{
	const int m_portNo = 5801;
	int m_frame = -1;
	unsigned char * m_pFrameData = 0;
	int				m_frameDataSize = 0;
	int				m_frameWidth = 0;
	int				m_frameHeight = 0;
	int				m_pixelX = 0;
	int				m_pixelY = 0;
	int				m_hue = 0;
	int				m_sat = 0;
	int				m_val = 0;
	int				m_red = 0;
	int				m_blue = 0;
	int				m_green = 0;
	std::thread   * m_pRecvThread = 0;
	std::thread	  * m_pWatchDogThread = 0;
	int 			m_scale = 2;
	bool			m_connected = false;

	static void StartRecvThread(ImageServer * pServer)
	{
		try
		{
			pServer->RunRecv();
		}
		catch (...)
		{
			printf("StartRecvThread catch\n");
		}
	}

	static void StartWatchDogThread(ImageServer * pServer)
	{
		pServer->WatchDog();
	}

	void WatchDog()
	{
		printf("ImageServer: WatchDog started\n");

		while (m_connected)
		{
			sleepMs(1000);

			printf("ImageServer: WatchDog\n");

			if (m_keepAliveTime + kKeepAliveTimeout < GetTimeMs())
			{
				printf("WatchDog timeout\n");

				m_connected = false;

				Disconnect();
			}
		}

		printf("ImageServer: WatchDog ended\n");

	}

	unsigned char * CreateJpeg(unsigned char * pFrame, int width, int height, int scale, long& length)
	{
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
//		double time = GetTimer();

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);

		unsigned char * pDest = 0;
		unsigned long nDest = 0;

		jpeg_mem_dest(&cinfo, &pDest, &nDest);

		cinfo.image_width = width; 	/* image width and height, in pixels */
		cinfo.image_height = height;
		cinfo.input_components = 3;	/* # of color components per pixel */
		cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

		jpeg_set_defaults(&cinfo);
		/* Make optional parameter settings here */

		cinfo.scale_num = 1;
		cinfo.scale_denom = scale;

		jpeg_start_compress(&cinfo, TRUE);

		JSAMPROW pRow;
		JSAMPARRAY line = &pRow;

		for (int y = 0 ; y < height ; y++)
		{
			pRow	= pFrame + (y * width * 3);

			jpeg_write_scanlines(&cinfo, line, 1);
		}

		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);

		length	= nDest;

		return(pDest);
	}

	void StartThreads()
	{
		m_pRecvThread = new thread(StartRecvThread, this);
		m_pWatchDogThread = new thread(StartWatchDogThread, this);
	}

	void SetProfile(int profile)
	{
		g_profile = profile;
		printf("Setting profile to %d\n", g_profile);
		Commands.ReadParams();
		CreateFilterTable(&filters);
		Camera.CommitParams();
	}

	void RunRecv()
	{
		printf("ImageServer receiver started\n");

		while (m_connected)
		{
			int  len;
			int  amount;
			int  level;
			char command[512];

			if ((len = read(command, sizeof(command) - 1)) == ERROR)
			{
				break;
			}

			command[len]	= 0;

			if (command[0] != '\r' && command[0] != '\n')
			{
				printf("%d: '%s'\n", len, command);
			}

			amount	= 0;
			sscanf(command + 1, "%d", &amount);

			switch (command[0])
			{
			case 'f':
				filters.SetColor(amount);
				break;

			case 'w':
				Commands.WriteParams();
				break;

			case 'P':
				SetProfile(amount);
				break;

			case 'T':
//				SyncCommand(command + 1);
				Camera.SaveNextImage();
				break;

			case 'p':
				level	= !Camera.GetProcessImage();
				Camera.SetProcessImage(level);
				printf(level ? "Processing on\n" : "Processing off\n");
				break;

			case 'h':
				Camera.IncrementHSV(MinH, amount);
				printf("MinH = %d, MaxH = %d\n", MinH, MaxH);
				break;

			case 'H':
				Camera.IncrementHSV(MaxH, amount);
				printf("MinH = %d, MaxH = %d\n", MinH, MaxH);
				break;

			case 's':
				Camera.IncrementHSV(MinS, amount);
				printf("MinS = %d, MaxS = %d\n", MinS, MaxS);
				break;

			case 'S':
				Camera.IncrementHSV(MaxS, amount);
				printf("MinS = %d, MaxS = %d\n", MinS, MaxS);
				break;

			case 'v':
				Camera.IncrementHSV(MinV, amount);
				printf("MinV = %d, MaxV = %d\n", MinV, MaxV);
				break;

			case 'V':
				Camera.IncrementHSV(MaxV, amount);
				printf("MinV = %d, MaxV = %d\n", MinV, MaxV);
				break;

			case 'b':
				level = Camera.IncrementBrightness(amount);
				printf("Camera Brightness = %d\n", level);
				break;

			case 'C':
				level = Camera.IncrementContrast(amount);
				printf("Camera Contrast = %d\n", level);
				break;

			case 'c':
				level = Camera.IncrementSaturation(amount);
				printf("Camera Saturation = %d\n", level);
				break;

			case 'i':
				level = Camera.IncrementISO(amount);
				printf("Camera ISO = %d\n", level);
				break;

			case 'l':
				level = Commands.IncrementTargetLine(amount);
				printf("target line = %d\n", level);
				break;

			case 'z':
				level = Commands.IncrementCenterPos(amount);
				printf("center line = %d\n", level);
				break;

			case 'r':
				m_scale = (m_scale == 1) ? 2 : 1;
				printf("Scale = %d\n", m_scale);
				break;

			case 't':
				sscanf(command + 1, "%d %d", &m_pixelX, &m_pixelY);
				m_pixelX = CaptureWidth-1 - m_pixelX;
				Camera.SetSamplePixel(m_pixelX, m_pixelY);
				printf("Set target pixel: (%d,%d)", m_pixelX, m_pixelY);
				break;

			case 'R':
				{
					int maxRegions = g_maxRegions + amount;
					if (maxRegions < 1)
					{
						maxRegions = 1;
					}
					g_maxRegions	= maxRegions;
				}
				break;

			case 'E':
				sscanf(command + 1, "%d", &level);
				level = Camera.IncrementShutterSpeed(level);
				printf("Shutterspeed = %d", level);
				break;

			case 'm':
				sscanf(command + 1, "%d", &level);
				MinArea	+= level;

				if (MinArea < 0)
				{
					MinArea = 0;
				}
				printf("Min Area = %d", MinArea);
				break;

			case 'a':
				Camera.ToggleAutoColorBalance();
				break;

			case 'A':
				Camera.ToggleAutoExposure();
				break;

			case 'k':		// Keep alive
				printf("ImageServer: Keep Alive\n");
				m_keepAliveTime = GetTimeMs();
				break;

#ifdef GPIO
			case 'L':
				g_light = !g_light;
				digitalWrite (0, g_light ? LED_ON : LED_OFF);
				digitalWrite (1, g_light ? LED_ON : LED_OFF);
				printf("Toggle light: %d\n", g_light);
				break;
#endif	// GPIO


			}

		}

		printf("ImageServer receiver exit\n");
	}


public:
	void StopServer()
	{
		delete m_pRecvThread;
		m_pRecvThread	= 0;

		SocketServer::StopServer();
	}

	void GetPixelHSV()
	{
		hsv	hsv;
		rgb rgb;

		Camera.GetSamplePixel(m_red, m_green, m_blue);

		rgb.r	= ((double) m_red) / 255.0;
		rgb.b	= ((double) m_blue) / 255.0;
		rgb.g	= ((double) m_green) / 255.0;

		hsv = rgb2hsv(rgb);

		if (hsv.h == NAN)
		{
			m_hue	= -1;
		}
		else
		{
			m_hue	= (hsv.h * 255) / 360;
		}
		m_sat	= (hsv.s * 255);
		m_val	= (hsv.v * 255);
	}

	struct RegionPoint
	{
		short	x;
		short	y;
	};

	struct RegionRect
	{
		short	left;
		short	top;
		short	right;
		short	bottom;
	};

	struct RegionData
	{
		short		color;
		short		topLeft;
		short		topRight;
		short		pad;
		RegionRect	bounds;
	};

	int64_t m_keepAliveTime = 0;

	void Run()
	{
		printf("ImageServer started\n");

		signal(SIGPIPE, SIG_IGN);

		sleepMs(5000);

		if (listen(m_portNo))
		{
			while (true)
			{
				printf("Waiting for image connection\n");

				if (accept())
				{
					ImageRegions * pRegions = 0;

					m_keepAliveTime = GetTimeMs();

					printf("Connected\n");

					m_connected	= true;

					StartThreads();

					while (true)
					{
						int	nFrame;
						int64_t time;

						sleepMs(100);

						delete pRegions;
						pRegions = Camera.GetFrameRegions(&nFrame, &time);

						if (nFrame != m_frame)
						{
//							printf("ImageServer: Sending Frame\n");

							if (!m_pFrameData)
							{
								if ((m_frameDataSize = Camera.GetFrameSize(m_frameWidth, m_frameHeight)))
								{
									m_pFrameData = (unsigned char *) malloc(m_frameDataSize);
								}
							}

							if (m_pFrameData)
							{
								unsigned char * pJpeg;
								long			size;
								unsigned long	key	= 0xaa55aa55;

								Camera.GetFrame(m_pFrameData);

								GetPixelHSV();

								pJpeg = CreateJpeg(m_pFrameData, m_frameWidth, m_frameHeight, m_scale, size);

								if (write(&key, sizeof(key)) == ERROR)
								{
									printf("write of key failed\n");
									break;
								}

								ImageHeader	hdr;
								unsigned long hdrSize = sizeof(hdr);

								hdr.imageSize	= size;
								hdr.width		= CaptureWidth;
								hdr.height		= CaptureHeight;
								hdr.minH		= MinH;
								hdr.maxH		= MaxH;
								hdr.minS		= MinS;
								hdr.maxS		= MaxS;
								hdr.minV		= MinV;
								hdr.maxV		= MaxV;
								hdr.cameraBrightness	= Camera.GetBrightness();
								hdr.cameraSaturation	= Camera.GetSaturation();
								hdr.cameraISO			= Camera.GetISO();
								hdr.targetPosition		= ImageLinePos;
								hdr.centerLine			= ImageCenterPos;
								hdr.pixelX		= m_pixelX;
								hdr.pixelY		= m_pixelY;
								hdr.pixelRed	= m_red;
								hdr.pixelBlue	= m_blue;
								hdr.pixelGreen	= m_green;
								hdr.pixelHue	= m_hue;
								hdr.pixelSat	= m_sat;
								hdr.pixelVal	= m_val;
								hdr.camRedBalance = Camera.GetRedBalance();
								hdr.camBlueBalance = Camera.GetBlueBalance();
								hdr.camAutoColorBalance = Camera.GetAutoColorBalance();
								hdr.camExposureComp = Camera.GetExposureComp();
								hdr.camContrast = Camera.GetContrast();
								hdr.minArea = MinArea;
								hdr.camShutterSpeed = Camera.GetShutterSpeed();
								hdr.maxRegions = g_maxRegions;
								hdr.fps = Camera.GetFPS();
								hdr.showFiltered = Camera.GetProcessImage();
								hdr.zoom = m_scale == 2;
								hdr.camAutoExposure = Camera.GetAutoExposure();
								hdr.color = filters.m_color;
								hdr.nRegions = 0;
								hdr.framesLost = Camera.m_framesLost;
								hdr.profile = g_profile;

								int regionSize = 0;
								RegionData * pRegionData = 0;

								if (pRegions)
								{
									regionSize	= pRegions->m_nRegions * sizeof(RegionData);

									hdrSize	+= regionSize;

									pRegionData = new RegionData[pRegions->m_nRegions];

									for (int i = 0 ; i < pRegions->m_nRegions ; i++)
									{
										pRegionData[i].color	= pRegions->m_pRegions[i].m_color;
										pRegionData[i].topLeft	= pRegions->m_pRegions[i].m_topLeft;
										pRegionData[i].topRight	= pRegions->m_pRegions[i].m_topRight;
										pRegionData[i].pad		= 0;
										pRegionData[i].bounds.left		= pRegions->m_pRegions[i].m_bounds.left;
										pRegionData[i].bounds.top		= pRegions->m_pRegions[i].m_bounds.top;
										pRegionData[i].bounds.right		= pRegions->m_pRegions[i].m_bounds.right;
										pRegionData[i].bounds.bottom	= pRegions->m_pRegions[i].m_bounds.bottom;
									}
									hdr.nRegions	= pRegions->m_nRegions;
								}

								if (write(&hdrSize, sizeof(hdrSize)) == ERROR)
								{
									printf("write of header size failed\n");
									break;
								}

								if (write(&hdr, sizeof(hdr)) == ERROR)
								{
									printf("write of header failed\n");
									break;
								}

								if (pRegionData)
								{
									if (write(pRegionData, regionSize) == ERROR)
									{
										printf("write of regions failed\n");
										break;
									}

									delete pRegionData;
								}

								if (write(pJpeg, size) == ERROR)
								{
									printf("write of jpeg data failed\n");
									break;
								}

								free(pJpeg);

//								if ((GetTimeMs() - m_keepAliveTime) > kKeepAliveTimeout)
//								{
//									printf("ImageServer: Keep alive timeout\n");
//									break;
//								}
							}
						}
					}

					Disconnect();

//					::shutdown(m_connectedSocket,SHUT_RDWR);
//					::close(m_connectedSocket);

					delete pRegions;
					pRegions = 0;


					printf("Image Connection Lost\n");

					m_connected = false;
				}
			}
		}

	}
} ImageServer;
#endif	// IMAGE_SERVER

int main(int argc, char * argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

#ifdef GPIO
	  wiringPiSetup () ;
	  pinMode (0, OUTPUT) ;
	  pinMode (1, OUTPUT);
	  g_light = false;
	  digitalWrite (0, LED_OFF);
	  digitalWrite(1, LED_OFF);
#endif	// GPIO


	CaptureWidth = 640;
	CaptureHeight = 480;

	for (int i = 1 ; i < argc ; i++)
	{
		int rate;
		int size;

		printf("%d: %s\n", i, argv[i]);

		switch (argv[i][0])
		{
		case '-':
			switch (argv[i][1])
			{
			case 's':
				if (sscanf(argv[i]+2, "%d", &size) == 1)
				{
					switch (size)
					{
					case 0:
						CaptureWidth = 320;
						CaptureHeight = 240;
						break;

					case 1:
						CaptureWidth = 640;
						CaptureHeight = 480;
						break;

					case 2:
						CaptureWidth = 1280;
						CaptureHeight = 720;
						break;

					}

				}
				break;

			case 'f':
				if (sscanf(argv[i]+2, "%d", &rate) == 1)
				{
					FrameRate = rate;
				}
				break;
			}
			break;
		}
	}

	printf("CaptureWidth = %d, CaptureHeight = %d, FrameRate = %d\n", CaptureWidth, CaptureHeight, FrameRate);

#ifdef UseGraphics
	graphics.Init();
#endif	// UseGraphics

//	Commands.ReadParams();
//	CreateFilterTable(&filters);

	Camera.Init(CaptureWidth, CaptureHeight, FrameRate);
	Camera.StartCapture();

	printf("Normal Settings\n");

	sleep(5);

	printf("Setting Profile 0\n");

	Commands.ReadParams();
	CreateFilterTable(&filters);
	Camera.CommitParams();

	NetworkServer.StartServer();
#ifdef IMAGE_SERVER
	ImageServer.StartServer();
#endif	// IMAGE_SERVER

	while (true)
	{
		if (!Commands.ProcessCommand())
		{
			printf("ProcessCommand returns false\n");
			break;
		}

		sleepMs(50);
	}

	Camera.Stop();

	sleep(1);

	printf("quit\n");

	return 0;
}

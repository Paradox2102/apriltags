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
#include <cstdio>
#include <linux/videodev2.h>

#include "ProcessApriltagsFast.h"
#include "cameraCapture.h"


using namespace std;

#define CaptureWidth 1280
#define CaptureHeight 800
#define CaptureSize (CaptureWidth * CaptureHeight)
int FrameRate = 30;
int g_maxRegions	= 20;
int g_profile = 0;
int g_shutterSpeed = 400;
int g_gain = 64;

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

unsigned char * LoadPPmBitmap(char * pFile, int& width, int& height)
{
	FILE		*	fp	= fopen(pFile, "rb");
	char			line[512];

	if (fp == NULL)
	{
		printf("Cannot open %s\n", pFile);
		return(NULL);
	}

	fgets(line, DIM(line), fp);

	if (!strcmp(line, "P5\n"))
	{
		int value = 0;

		fgets(line, DIM(line), fp);

		if (sscanf(line, "%d%d%d", &width, &height, &value) != 3)
		{
			if (sscanf(line, "%d", &width) == 1)
			{
				fgets(line, DIM(line), fp);

				if (sscanf(line, "%d", &height) == 1)
				{
					fgets(line, DIM(line), fp);

					sscanf(line, "%d", &value);
				}
			}
		}

		if (width && height && value)
		{
			fpos_t			pos1;
			fpos_t			pos2;
			int				size = width * height;
			unsigned char * pData;

			// fgetpos(fp, &pos1);
			// fseek(fp, 0, SEEK_END);
			// fgetpos(fp, &pos2);

			// size	= pos2 - pos1;

			// fseek(fp, pos1, SEEK_SET);

			// if (size == (width * height))
			{
				int ret;
				unsigned char * pPixels	= (unsigned char *) malloc(size);

				if ((ret = fread(pPixels, 1, size, fp)) == size)
				{
					return pPixels;
				}

				printf("Read fail: %d\n", ret);

				free(pPixels);
			}
		}
	}

	return(0);
}

#define BlackColor	110
#define MinWhite	4
#define MinBlack	32
#define MinSize		(4*8)

void testDetection(char * pFile)
{
	int width;
	int height;
	unsigned char * pImage = LoadPPmBitmap(pFile, width, height);

	if (pImage != 0)
	{
		std::list<ImageRegion*> * pRegions = processImageFast(pImage, width, height, BlackColor, MinWhite, MinBlack, MinSize);

		printf("count = %d\n", pRegions->size());
	}

	exit(-1);
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

unsigned char cameraData[CaptureSize];

class Camera 
{
public:
	int m_framesLost = 0;
//private:
	int m_brightness	= 50;
	int m_saturation	= 100;
	int m_iso			= 800;
	int m_width			= 0;
	int	m_height		= 0;
	int m_fpsCount		= 100;
	double m_processTime = 0;
//	double m_grabTime = 0;
	float m_FPS			= 0;
	std::list<ImageRegion*> * m_pRegions;
	unsigned int m_frameCount	= 0;
	unsigned int m_lastFrame	= 0;
	// unsigned int m_dataSize	= 0;
	unsigned int m_curFrame = 0;
	unsigned char * m_pData	= 0;
	// unsigned char * m_pProcessData = 0;
	thread * m_pThread = 0;
	thread * m_pProcessThread = 0;
	mutex m_mutex;
	// mutex m_dataMutex;
	// mutex m_grabMutex;
	// mutex m_processMutex;
	// mutex m_processDataMutex;
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
		printf("ProcessStart\n");
		pCamera->ProcessRun();
	}

	void ThreadRun()
	{
	    int ret;
		
		if ((ret = initializeCamera(g_shutterSpeed, g_gain)))
		{
			printf("Cannot initialize camera: ret=%d\n", ret);
			return;
		}

	    sleep(3);

	    m_pData = new unsigned char[CaptureSize];

	    m_imageCache.SetCacheSize(DefaultImageCacheSize, CaptureSize);

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

		int64_t nextTime = GetTimeMs();
		int64_t startTime = nextTime;

	    while (m_threadRun)
	    {
			if (GetTimeMs() >= nextTime)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				if ((ret = captureImage(cameraData, CaptureSize)))
				{
					printf("Capture fails: ret=%d\n", ret);
				}

				m_frameCount++;
				m_grabTime = GetTimeMs();
				nextTime += 1000 / FrameRate;
				// printf("grabTime =  %d\n", (int) (m_grabTime - startTime));
			}


			int dt = nextTime - GetTimeMs();

			if (dt > 0)
			{
				sleepMs(dt);
			}
	    }

	    printf("Grab thread exit\n");
	}

	void SetSpeed(int speed)
	{
		std::lock_guard<std::mutex> lock(m_mutex);	

		setShutterSpeed(speed);
	}

	void SaveImage(char * prefix, unsigned char * pData)
	{
		static int count = 0;

		char name[64];				
		snprintf(name, sizeof(name), "%s%03d.ppm", prefix, count++);
		SaveImageToFile(name, pData);
		printf("%s saved\n", name);
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
	// void LogRegions(ImageRegions * pRegions, int frameNo)
	// {
	// 	if (logFp != 0)
	// 	{
	// 		long time = (int) ((GetTimer() - startTime) * 1000);

	// 		if (pRegions->m_nRegions > 0)
	// 		{
	// 			// fprintf(logFp, "%ld,%d,%d,%d,%d,%d\n", time, frameNo,
	// 			// 											pRegions->m_pRegions[0].m_bounds.left,
	// 			// 											pRegions->m_pRegions[0].m_bounds.top,
	// 			// 											pRegions->m_pRegions[0].m_bounds.right,
	// 			// 											pRegions->m_pRegions[0].m_bounds.bottom);
	// 		}
	// 		else
	// 		{
	// 			fprintf(logFp, "%ld: no regions\n", time);
	// 		}

	// 		fflush(logFp);
	// 	}
	// }
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
		double time0 = GetTimer();
		static unsigned char imageData[CaptureSize];
		int64_t startTime = GetTimeMs();

		while (m_threadRun)
		{
			int64_t grabTime;

			bool newFrame = false;

			// printf("Process thread\n");
			// sleep(5);
			// continue;

			{
				std::lock_guard<std::mutex> lock(m_mutex);

				// printf("m_frameCount=%d\n",  m_frameCount);

				if (prevFrame != m_frameCount)
				{
					// printf("m_frameCount=%d\n", m_frameCount);
					
					if ((prevFrame != -1) && (m_frameCount != (prevFrame + 1)))
					{
						m_framesLost += (m_frameCount - prevFrame - 1);
						// printf("Lost frame: %d %d %d\n", prevFrame, lastFrame, m_framesLost);
					}
					prevFrame = m_frameCount;

					memmove(imageData, cameraData, CaptureSize);

					nFramesProcessed++;

					newFrame = true;
				}
			}

			// printf("Process thread: newFrame = %d\n", newFrame);

			if (newFrame)
			{
				// printf("m_frameCount = %d\n", m_frameCount);
				// sleepMs(100);
				int64_t time = GetTimeMs();
				// std::list<ImageRegion*> * pRegions = ProcessAprilTags(imageData, CaptureWidth, CaptureHeight);
				std::list<ImageRegion*> * pRegions = processImageFast(imageData, CaptureWidth, CaptureHeight,
															BlackColor, MinWhite, MinBlack, MinSize);
				// printf("dt=%d\n", (int) (GetTimeMs() - time));
				// printf("ProcessAprilTags returns\n");
				// printf("et=%d\n", (int) (time - startTime));

				// printf("pRegions->m_nRegions=%d\n", pRegions->m_nRegions);

				// if (pRegions->size() == 0)
				// {
				// 	printf("No regions found\n");
				// 	SaveImage("none", imageData);
				// }

				// if (pRegions->size() != 1)
				// {
				// 	printf("Not one\n");
				// 	SaveImage("notOne", imageData);
				// }

				// if (pRegions->size() < 2)
				// {
				// 	printf("Not two\n");
				// 	SaveImage("notTwo", imageData);
				// }

				{
					std::lock_guard<std::mutex> lock(m_mutex);

					if (m_pRegions)
					{
						deleteImageRegions(m_pRegions);
					}

					m_lastFrame = prevFrame;
					m_captureTime = 0;
					m_pRegions = pRegions;
				}

				if (nFramesProcessed == m_fpsCount)
				{
					double time = GetTimer();
					double	dt	= time - time0;
					int nGrabbed = prevFrame - firstFrame;

					m_FPS = nFramesProcessed / dt;

					printf("FPS = %0.2f, GPS = %0.2f, proc = %d, grabbed = %d, dt=%f\n", m_FPS, nGrabbed / dt, nFramesProcessed, nGrabbed, dt);

					time0 = time;
					nFramesProcessed = 0;
					firstFrame = prevFrame;
				}
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
		m_imageCache.SetCacheSize(size, CaptureSize);
	}

	void ResetFrameCount()
	{
		// std::lock_guard<std::mutex> lock(m_dataMutex);

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
		unsigned char * pFrame = new unsigned char[CaptureSize];

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

	void StartCapture()
	{
		printf("StartCapture\n");

		if (!m_pThread)
		{
			m_pThread = new std::thread(ThreadStart, this);
		}

		if (!m_pProcessThread)
		{
			m_pProcessThread = new std::thread(ProcessStart, this);
		}
	}

	void GetFrame(unsigned char * pData)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		memcpy(pData, cameraData, CaptureSize);
	}

	void GetScaledFrame(unsigned char * pData)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
	
		unsigned char * s = cameraData;

		for (int y = 0 ; y < CaptureHeight ; y += 2)
		{
			for (int x = 0 ; x < CaptureWidth ; x += 2, s += 2)
			{
				*pData++ = *s;
			}

			s += CaptureWidth;
		}
	}

	std::list<ImageRegion*> * GetFrameRegions(int * pLastFrame, int64_t * pCaptureTime)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		*pLastFrame	= m_lastFrame;
		*pCaptureTime = m_captureTime;

		if (m_pRegions)
		{
			std::list<ImageRegion*> * pRegions = new std::list<ImageRegion*>();

			for (ImageRegion * pRegion : *m_pRegions)
			{
				pRegions->push_back(new ImageRegion(pRegion));
			}

			return(pRegions);
		}

		return(0);	// MUSTFIX
	}

	int SetNewShutterSpeed(int speed)
	{
		g_shutterSpeed = speed;
		setShutterSpeed(g_shutterSpeed);
		// MUSTFIX

		// std::lock_guard<std::mutex> lock(m_grabMutex);

		// m_shutterSpeed	+= amount;

		// if (m_shutterSpeed < 0)
		// {
		// 	m_shutterSpeed	= 0;
		// }

		// setShutterSpeed(m_shutterSpeed);

		return(m_shutterSpeed);
	}

	void SaveImageToFile (char * filepath,unsigned char *data)
	{
	    std::ofstream outFile ( filepath, std::ios::binary );

	    outFile<<"P5\n";

	    // printf("SaveImageToFile: width = %d, height = %d\n", CaptureWidth, CaptureHeight);
	    outFile << CaptureWidth << " " << CaptureHeight << " 255\n";
	    outFile.write ( ( char* ) data, CaptureSize );
	}


	void SaveImage()
	{
		char file[512];
		static uint8_t data[CaptureSize];

		GetFrame(data);

		snprintf(file, sizeof(file), "image%04d.ppm", m_frameCount);
		printf("Writing: %s\n", file);

		SaveImageToFile(file, data);

	}

} Camera;

void SaveImage(char * prefix, unsigned char * pData)
{
	Camera.SaveImage(prefix, pData);
}

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

				case 'L':
					sscanf(line, "L %d", &ImageLinePos);
					break;

				case 'z':
					sscanf(line, "z %d", &ImageCenterPos);

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

			fprintf(fp, "L %d\n", ImageLinePos);

			// fprintf(fp, "m %d\n", MinArea);

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

		case ' ':
			Camera.SaveImage();
			break;

		case 'b':	// brighter
			g_shutterSpeed += 25;
			Camera.SetSpeed(g_shutterSpeed);
			break;

		case 'd':	// darker
			g_shutterSpeed -= 25;
			Camera.SetSpeed(g_shutterSpeed);
			break;

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
						std::list<ImageRegion*> * pRegions = 0;

						printf("Connected\n");

						m_connected = true;

						StartThreads();

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
									for (ImageRegion * pRegion : *pRegions)
									{
										// MUSTFIX
										// snprintf(msg, sizeof(msg), "R %d %d %d %d %d %d %d\n",
										// 		pRegions->m_pRegions[i].m_color,
										// 		pRegions->m_pRegions[i].m_bounds.left,
										// 		pRegions->m_pRegions[i].m_bounds.top,
										// 		pRegions->m_pRegions[i].m_bounds.right,
										// 		pRegions->m_pRegions[i].m_bounds.bottom,
										// 		pRegions->m_pRegions[i].m_topLeft,
										// 		pRegions->m_pRegions[i].m_topRight);
										msg[0] = 0;

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
								deleteImageRegions(pRegions);
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
							deleteImageRegions(pRegions);
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

#pragma pack(4)

struct ImageHeader
{
	unsigned long	imageSize;				//  0
	short			width;					//  4
	short			height;					//  6

	short			blackColor;				//  8
	short			minSize;				// 10
	short			minWhite;				// 12
	short			minBlack;				// 14
	short			maxSlope;				// 16
	short			maxParallel;			// 18
	short			maxAspect;				// 20
	short			shutterSpeed;			// 22
	short			gain;					// 24
	short			captureRate;			// 26
	short			fps;					// 28
	
	short			nRegions;				// 30
	// size 32

	// short			minH;
	// short			maxH;
	// short			minS;
	// short			maxS;
	// short			minV;
	// short			maxV;
	// short			cameraBrightness;
	// short			cameraSaturation;
	// short			cameraISO;
	// short			targetPosition;
	// short			pixelX;
	// short			pixelY;
	// short			pixelRed;
	// short			pixelBlue;
	// short			pixelGreen;
	// short			pixelHue;
	// short			pixelSat;
	// short			pixelVal;
	// float			camRedBalance;
	// float			camBlueBalance;
	// short			camAutoColorBalance;
	// short			camExposureComp;
	// short			camContrast;
	// short			minArea;
	// short			camShutterSpeed;
	// short			maxRegions;
	// float			fps;
	// short			showFiltered;
	// short			zoom;
	// short			camAutoExposure;
	// short			centerLine;
	// short			color;
	// short			framesLost;

	// short			profile;
};

class ImageServer : public SocketServer
{
	const int m_portNo = 5801;
	int m_frame = -1;
	// unsigned char * m_pFrameData = 0;
	// int				m_frameDataSize = 0;
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
		cinfo.input_components = 1;	/* # of color components per pixel */
		cinfo.in_color_space = JCS_GRAYSCALE; /* colorspace of input image */

		jpeg_set_defaults(&cinfo);
		/* Make optional parameter settings here */

		// cinfo.scale_num = 1;
		// cinfo.scale_denom = scale;	// MUSTFIX

		jpeg_start_compress(&cinfo, TRUE);

		JSAMPROW pRow;
		JSAMPARRAY line = &pRow;

		for (int y = 0 ; y < height ; y++)
		{
			pRow	= pFrame + (y * width);

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

	// void SetProfile(int profile)
	// {
	// 	g_profile = profile;
	// 	printf("Setting profile to %d\n", g_profile);
	// 	Commands.ReadParams();
	// 	CreateFilterTable(&filters);
	// 	Camera.CommitParams();
	// }

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
			// case 'f':
			// 	filters.SetColor(amount);
			// 	break;

			case 'w':
				Commands.WriteParams();
				break;

			// case 'P':
			// 	SetProfile(amount);
			// 	break;

			case 'T':
//				SyncCommand(command + 1);
				Camera.SaveNextImage();
				break;

			case 'z':
				level = Commands.IncrementCenterPos(amount);
				printf("center line = %d\n", level);
				break;

			case 'r':
				m_scale = (m_scale == 1) ? 2 : 1;
				printf("Scale = %d\n", m_scale);
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
				Camera.SetNewShutterSpeed(level);
				printf("Shutterspeed = %d", level);
				break;

			case 'k':		// Keep alive
				printf("ImageServer: Keep Alive\n");
				m_keepAliveTime = GetTimeMs();
				break;
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

	// void GetPixelHSV()
	// {
	// 	hsv	hsv;
	// 	rgb rgb;

	// 	Camera.GetSamplePixel(m_red, m_green, m_blue);

	// 	rgb.r	= ((double) m_red) / 255.0;
	// 	rgb.b	= ((double) m_blue) / 255.0;
	// 	rgb.g	= ((double) m_green) / 255.0;

	// 	hsv = rgb2hsv(rgb);

	// 	if (hsv.h == NAN)
	// 	{
	// 		m_hue	= -1;
	// 	}
	// 	else
	// 	{
	// 		m_hue	= (hsv.h * 255) / 360;
	// 	}
	// 	m_sat	= (hsv.s * 255);
	// 	m_val	= (hsv.v * 255);
	// }

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
		RegionPoint	corners[4];
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
					std::list<ImageRegion*> * pRegions = 0;

					m_keepAliveTime = GetTimeMs();

					printf("Connected\n");

					m_connected	= true;

					StartThreads();

					uint64_t t0 = GetTimeMs();

					while (true)
					{
						int	nFrame;
						int64_t time;

						sleepMs(100);

						// deleteImageRegions(pRegions);
						pRegions = Camera.GetFrameRegions(&nFrame, &time);

						if (nFrame != m_frame)
						{
							static unsigned char frameData[CaptureSize];
//							printf("ImageServer: Sending Frame\n");

							// if (!m_pFrameData)
							// {
							// 	if ((m_frameDataSize = Camera.GetFrameSize(m_frameWidth, m_frameHeight)))
							// 	{
							// 		m_pFrameData = (unsigned char *) malloc(m_frameDataSize);
							// 	}
							// }

							// if (m_pFrameData)
							{
								unsigned char * pJpeg;
								long			size;
								unsigned long	key	= 0xaa55aa55;
								
								uint64_t t1 = GetTimeMs();
								Camera.GetScaledFrame(frameData);

								// GetPixelHSV();

								// printf("send image %d\n", nFrame);

								pJpeg = CreateJpeg(frameData, CaptureWidth/2, CaptureHeight/2, 1, size);

								if (write(&key, sizeof(key)) == ERROR)
								{
									printf("write of key failed\n");
									break;
								}

								ImageHeader	hdr;
								unsigned long hdrSize = sizeof(hdr);

								memset(&hdr, 0, sizeof(hdr));

								hdr.imageSize	= size;
								hdr.width		= CaptureWidth;
								hdr.height		= CaptureHeight;

								hdr.blackColor	= BlackColor;					//  4+4
								hdr.minSize		= MinSize;						//  6+4
								hdr.minWhite	= MinWhite;						//  8+4
								hdr.minBlack	= MinBlack;						// 10+4
								hdr.maxSlope	= (short) (MaxSlope * 100);		// 12+4
								hdr.maxParallel	= (short) (MaxParallel * 100);	// 14+4
								hdr.maxAspect	= (short) (MaxAspect * 100);	// 16+4
								hdr.shutterSpeed = g_shutterSpeed;				// 18+4
								hdr.gain		= g_gain;						// 20+4
								hdr.captureRate = FrameRate;					// 22+4
								hdr.fps			= (short) (Camera.m_FPS * 10);
								hdr.nRegions 	= 0;							// 24+4
								// hdr.minH		= MinH;
								// hdr.maxH		= MaxH;
								// hdr.minS		= MinS;
								// hdr.maxS		= MaxS;
								// hdr.minV		= MinV;
								// hdr.maxV		= MaxV;
								// hdr.cameraBrightness	= Camera.GetBrightness();
								// hdr.cameraSaturation	= Camera.GetSaturation();
								// hdr.cameraISO			= Camera.GetISO();
								// hdr.targetPosition		= ImageLinePos;
								// hdr.centerLine			= ImageCenterPos;
								// hdr.pixelX		= m_pixelX;
								// hdr.pixelY		= m_pixelY;
								// hdr.pixelRed	= m_red;
								// hdr.pixelBlue	= m_blue;
								// hdr.pixelGreen	= m_green;
								// hdr.pixelHue	= m_hue;
								// hdr.pixelSat	= m_sat;
								// hdr.pixelVal	= m_val;
								// hdr.camRedBalance = Camera.GetRedBalance();
								// hdr.camBlueBalance = Camera.GetBlueBalance();
								// hdr.camAutoColorBalance = Camera.GetAutoColorBalance();
								// hdr.camExposureComp = Camera.GetExposureComp();
								// hdr.camContrast = Camera.GetContrast();
								// hdr.minArea = 0; //MinArea;
								// hdr.camShutterSpeed = Camera.GetShutterSpeed();
								// hdr.maxRegions = g_maxRegions;
								// hdr.fps = Camera.GetFPS();
								// hdr.showFiltered = Camera.GetProcessImage();
								// hdr.zoom = m_scale == 2;
								// hdr.camAutoExposure = Camera.GetAutoExposure();
								// hdr.color = filters.m_color;

								// hdr.framesLost = Camera.m_framesLost;
								// hdr.profile = g_profile;

								int regionSize = 0;
								RegionData * pRegionData = 0;

								if (pRegions)
								{
									regionSize	= pRegions->size() * sizeof(RegionData);

									hdrSize	+= regionSize;

									// printf("SendRegions: nRegions=%d\n", pRegions->m_nRegions);

									pRegionData = new RegionData[pRegions->size()];

									memset(pRegionData, 0, pRegions->size());

									int i = 0;
									for (ImageRegion * pRegion : *pRegions)
									{
										pRegionData[i].color	= pRegion->m_tag;
					
										for (int j = 0 ; j < 4 ; j++)
										{
											pRegionData[i].corners[j].x = pRegion->m_corners[j][0] * 10;
											pRegionData[i].corners[j].y = pRegion->m_corners[j][1] * 10;
										}
										// pRegionData[i].bounds.left		= pRegion->m_tag;
										// pRegionData[i].bounds.top		= pRegions->m_pRegions[i].m_bounds.top;
										// pRegionData[i].bounds.right		= pRegions->m_pRegions[i].m_bounds.right;
										// pRegionData[i].bounds.bottom	= pRegions->m_pRegions[i].m_bounds.bottom;
										i++;
									}
									hdr.nRegions	= pRegions->size();
								}

								if (write(&hdrSize, sizeof(hdrSize)) == ERROR)
								{
									printf("write of header size failed\n");
									break;
								}
								
								// printf("sizeof(hdr)=%d\n", sizeof(hdr));

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

								int64_t t = GetTimeMs();
								// printf("t0=%d,t1=%d,s=%d\n", (int) (t - t0), (int) (t - t1), size);



//								if ((GetTimeMs() - m_keepAliveTime) > kKeepAliveTimeout)
//								{
//									printf("ImageServer: Keep alive timeout\n");
//									break;
//								}
							}
						}
						deleteImageRegions(pRegions);
					}

					Disconnect();

//					::shutdown(m_connectedSocket,SHUT_RDWR);
//					::close(m_connectedSocket);

					deleteImageRegions(pRegions);
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

	char cwd[256];
   	printf("dir=%s\n", getcwd(cwd, sizeof(cwd)));

	// testDetection("one003.ppm");


	for (int i = 1 ; i < argc ; i++)
	{
		int v;


		printf("%d: %s\n", i, argv[i]);

		switch (argv[i][0])
		{
		case '-':
			switch (argv[i][1])
			{
			case 's':	// shutter speed
				if (sscanf(argv[i]+2, "%d", &v) == 1)
				{
					g_shutterSpeed = v;
				}
				break;

			case 'g':	// Gain
				if (sscanf(argv[i]+2, "%d", &v) == 1)
				{
					g_gain = v;
				}
				break;


			case 'f':
				if (sscanf(argv[i]+2, "%d", &v) == 1)
				{
					FrameRate = v;
				}
				break;
			}
			break;
		}
	}

	printf("Shuter Speed = %d, Gain = %d, Frame Rate = %d\n", g_shutterSpeed, g_gain, FrameRate);

#ifdef UseGraphics
	graphics.Init();
#endif	// UseGraphics

//	Commands.ReadParams();
//	CreateFilterTable(&filters);

	// Camera.Init(CaptureWidth, CaptureHeight, FrameRate);
	Camera.StartCapture();

	printf("Normal Settings\n");

	sleep(5);

	printf("Setting Profile 0\n");

	Commands.ReadParams();
	// CreateFilterTable(&filters);
	// Camera.CommitParams();

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

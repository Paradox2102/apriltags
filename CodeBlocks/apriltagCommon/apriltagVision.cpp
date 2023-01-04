/*
 *	  Copyright (C) 2022  John H. Gaby
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 3 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    Contact: robotics@gabysoft.com
 */

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
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
#include <cmath>

#include "ProcessApriltagsFast.h"
#include "cameraCapture.h"
#include "opencv.h"

/*************
 * 
 * This program is used to capture images and process them for Apriltags.
 *	It provides a server that provides the data for the Apriltags found to the RoboRio
 *	A second server provides is used to provide images to a computer which can view the
 *		images as well as control the camera and the image processing.
 *	The program consists of 4 threads:
 *		1 - The capture thread is responsible for capturing the images from the camera
 *		2 - The processing thread take the captured images and finds the Apriltags.
 *		3 - The data thread listens for a connection on port 5800 and supplies the Apriltag data to the RoboRio
 *		4 - The image thread listens for a connection on port 5801 and supplies the images to a computer
 *
 * 	In addition there is the main thread which monitors the keyboard for commands.
 * 
 **************/


using namespace std;

#ifdef INNOMAKER
#define CaptureWidth 1280
#define CaptureHeight 800
#define CaptureSize (CaptureWidth * CaptureHeight)
#else   // !INNOMAKER
#ifdef R1080
#define CaptureWidth    1920
#define CaptureHeight   1080
#else   // !R1080
#define CaptureWidth 1280
#define CaptureHeight 720
#endif // !R1080
#define CaptureSize (CaptureWidth * CaptureHeight)
#endif // !INNOMAKER

int FrameRate = 100;
int g_shutterSpeed = 300;
bool g_saveData = false;
double g_targetShutterSpeed = 400;
#ifdef INNOMAKER
int g_gain = 64;
#else   // !INNOMAKER
int g_gain = 800;       // ISO
#endif // INNOMAKER

ApriltagParams g_params;

#define TimeOffset	0;

// Return current time in seconds
double GetTimer()
{
	timespec	ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return((double) ts.tv_sec + ((double) ts.tv_nsec / 1000000000.0));
}

// Return current time is ms
int64_t GetTimeMs()
{
	timespec	ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return (((int64_t) ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)) - TimeOffset;
}

// Sleep for specified ms
void sleepMs(int ms)
{
	struct timespec tim, tim2;

	tim.tv_sec = ms / 1000;
	tim.tv_nsec = (ms % 1000) * 1000000L;

	nanosleep(&tim , &tim2);
}

// Test if there is a key pending
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

// Get key from keyboard (if any)
int getkey(void)
{
  if (kbhit())
  {
	  return(getchar());
  }

  return(0);
}

unsigned char cameraData[CaptureSize];  // buffer to hold most recent captured image

class Camera
{
public:
	int m_framesLost = 0;
	int m_brightness	= 50;
	int m_saturation	= 100;
	int m_iso			= 800;
	int m_width			= 0;
	int	m_height		= 0;
	int m_fpsCount		= 100;
	double m_processTime = 0;
	float m_FPS			= 0;
	std::list<ImageRegion*> * m_pRegions;
	int m_frameCount	= 0;
	int m_lastFrame	= 0;
	int m_blobCount;
	int m_avgWhite;
	double m_whiteBalance = 100;
#ifdef INNOMAKER
	double m_whiteTarget = 150;
#else   // !INNOMAKER
    double m_whiteTarget = 50;   // Brightness
	double m_contrast = 0;
	boolean m_autoExposure = true;
#endif // !INNOMAKER

	double m_alpha = 0.1;               // Used for auto white balance
	thread * m_pThread = 0;
	thread * m_pProcessThread = 0;
	mutex m_mutex;
	bool m_saveNextImage = false;
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

	bool m_fast = false;
	double m_fastWhiteTarget;

	void SetFastMode(bool fast)
	{
        std::lock_guard<std::mutex> lock(m_mutex);

		m_fast = fast;
	}

	/*
	 * This is the thread which is used to capture the images from the camera
	 */
	void ThreadRun()
	{
	    int ret;

		if ((ret = initializeCamera(g_shutterSpeed, g_gain)))
		{
			printf("Cannot initialize camera: ret=%d\n", ret);
			return;
		}

	    sleep(3);

		int64_t nextTime = GetTimeMs() * 10;    // Time for next frame grab

	    while (m_threadRun)
	    {
			if (GetTimeMs() >= (nextTime + 5) / 10)
			{
#ifndef INNOMAKER
                grabImage();
#endif // !INNOMAKER

				{
					std::lock_guard<std::mutex> lock(m_mutex);

					if ((ret = captureImage(cameraData, CaptureSize)))
					{
						printf("Capture fails: ret=%d\n", ret);
					}

					m_frameCount++;
					m_grabTime = GetTimeMs();
					nextTime += 10000 / FrameRate;  // Compute next grab time
				}
			}


			int dt = (int) ((nextTime + 5)/10 - GetTimeMs());

			// This bit of code is necessary in case the Pi updates it clock from the network
			if (abs(dt) > 1000)
			{
                nextTime = GetTimeMs()*10; // Adjust for time shift
			}

			if (dt > 0)
			{
				sleepMs(dt);
			}
			else
			{
                sleepMs(1);     // Sleep a minimun of 1 ms
			}
	    }

	    printf("Grab thread exit\n");
	}

	// Saves the specified image using the specified prefix
	void SaveImage(const char * prefix, unsigned char * pData)
	{
		static int count = 0;

		char name[64];
		snprintf(name, sizeof(name), "%s%03d.ppm", prefix, count++);
		SaveImageToFile(name, pData);
		printf("%s saved\n", name);
	}

	/*
	 *	This is the thread which process the image data and finds the Apriltags.
	 *		It can do this by either calling the original implementation 'ProcessAprilTags'
	 *		or by calling the experimental implementation 'ProcessAprilTagsFast'
	 */
	void ProcessRun()
	{
    	int nFramesProcessed = 0;
    	int firstFrame = 0;
    	int prevFrame = -1;
		double time0 = GetTimer();
		static unsigned char imageData[CaptureSize];

		while (m_threadRun)
		{
			int64_t captureTime;

			bool newFrame = false;
			bool fast;
			ApriltagParams params;

			{
				std::lock_guard<std::mutex> lock(m_mutex);

				if (prevFrame != m_frameCount)
				{
					if ((prevFrame != -1) && (m_frameCount != (prevFrame + 1)))
					{
						m_framesLost += (m_frameCount - prevFrame - 1);
					}
					prevFrame = m_frameCount;

					memmove(imageData, cameraData, CaptureSize);

					nFramesProcessed++;

					params = g_params;
					newFrame = true;
					captureTime = m_grabTime;
					fast = m_fast;
				}
			}

			if (newFrame)
			{
				int blobCount;
				int avgWhite;
				std::list<ImageRegion*> * pRegions;


				if (fast)
				{
					pRegions = processApriltagsFast(imageData, CaptureWidth, CaptureHeight, &params, &blobCount, &avgWhite);
				}
				else
				{
					pRegions = ProcessAprilTags(imageData, CaptureWidth, CaptureHeight);
				}

				if (g_saveData)
				{
                    SaveImage("image", imageData);
                    for (ImageRegion * pRegion : *pRegions)
                    {
                        printf("%d,%0.2f,%0.2f,%0.2f,%0.1f,%0.1f,%0.1f\n",  pRegion->m_tag, pRegion->m_rvec[0], pRegion->m_rvec[1], pRegion->m_rvec[2], pRegion->m_tvec[0], pRegion->m_tvec[1], pRegion->m_tvec[2]);
                    }
                    g_saveData = false;
				}

				{
					std::lock_guard<std::mutex> lock(m_mutex);

					if (m_pRegions)
					{
						deleteImageRegions(m_pRegions);
					}

					m_whiteBalance = (avgWhite * m_alpha) + (1 - m_alpha) * m_whiteBalance;

					m_lastFrame = prevFrame;
					m_captureTime = captureTime;
					m_pRegions = pRegions;
					m_blobCount = blobCount;
					m_avgWhite = avgWhite;
				}

#ifdef INNOMAKER
				// This adjust the shutter speed to match the white target. Note this is only use when in 'fast' mode
				if (fast && m_whiteTarget != 0)
				{
					int tol = 5;

					double delta = m_whiteBalance - m_whiteTarget;

					if (abs(delta) > tol)
					{
						g_targetShutterSpeed -= 0.1 * delta;
					}

					if (g_targetShutterSpeed > 855)
					{
						g_targetShutterSpeed = 855;
					}
					else if (g_targetShutterSpeed < 1)
					{
						g_targetShutterSpeed = 1;
					}

					if (((int) g_targetShutterSpeed) != g_shutterSpeed)
					{
						SetNewShutterSpeed((int) g_targetShutterSpeed);
					}
				}
#endif // INNOMAKER

				if (nFramesProcessed >= m_fpsCount)
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
	Camera()
	{
	}

	void Stop()
	{
		m_threadRun	= false;
	}

	void ResetFrameCount()
	{
		m_frameCount	= 0;
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

	void GetScaledFrame(unsigned char * pData, int scale)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		unsigned char * s = cameraData;

		for (int y = 0 ; y < CaptureHeight ; y += scale)
		{
            s = cameraData + (y * CaptureWidth);
			for (int x = 0 ; x < CaptureWidth ; x += scale, s += scale)
			{
				*pData++ = *s;
			}

//			s += CaptureWidth;
		}
	}

	std::list<ImageRegion*> * GetFrameRegions(int * pLastFrame, int64_t * pCaptureTime, int * pblobCount = 0, int * pAvgWhite = 0, int * pWhiteTarget = 0, int * pContrast = 0, bool * pAutoExp = 0)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		*pLastFrame	= m_lastFrame;
		*pCaptureTime = m_captureTime;

		if (pblobCount)
		{
			*pblobCount = m_blobCount;
		}

		if (pAvgWhite)
		{
			*pAvgWhite = m_whiteBalance;
		}

		if (pWhiteTarget)
		{
			*pWhiteTarget = m_whiteTarget;
		}

#ifndef INNOMAKER
		if (pContrast)
		{
            *pContrast = m_contrast;
        }

        if (pAutoExp)
        {
            *pAutoExp = m_autoExposure;
        }
#endif // !INNOMAKER

		if (m_pRegions)
		{
			std::list<ImageRegion*> * pRegions = new std::list<ImageRegion*>();

			for (ImageRegion * pRegion : *m_pRegions)
			{
				pRegions->push_back(new ImageRegion(pRegion));
			}

			return(pRegions);
		}

		return(0);
	}

	void SetNewShutterSpeed(int speed)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_targetShutterSpeed =
		g_shutterSpeed = speed;
		setShutterSpeed(g_shutterSpeed);
	}

	void SetNewGain(int gain)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_gain = gain;
		setGain(g_gain);
	}

	void setNewCaptureRate(int rate)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		FrameRate = rate;
	}

	void setNewWhiteDrop(int drop)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.whiteDrop = drop;
	}

	void setNewMinSize(int size)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.minSize = size;
	}

	void setNewWhiteCnt(int cnt)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.whiteCnt = cnt;
	}

	void setNewMinBlack(int min)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.minBlack = min;
	}

	void setNewMaxSlope(double max)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.maxSlope = max;
	}

	void setNewMaxParallel(double max)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.maxParallel = max;
	}

	void setNewMaxAspect(double max)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.maxAspect = 1 + max;
		g_params.minAspect = 1 - max;
	}

	void setNewSampleRegion(int region)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		g_params.sampleRegion = region;
	}

	void setNewWhiteTarget(int target)
	{
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_whiteTarget = target;
        }

#ifndef INNOMAKER
        setBrightness(target);
#endif // !INNOMAKER
	}

#ifndef INNOMAKER
	void setNewAutoExposure(bool autoExp)
	{
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_autoExposure = autoExp;
        }

        setAutoExposure(autoExp);
	}

	void setNewContrast(int contrast)
	{
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_contrast = contrast;
        }

        setContrast(contrast);
	}
#endif // !INNOMAKER


    // Save the specified image to a file in PPM format
	void SaveImageToFile (char * filepath,unsigned char *data)
	{
	    std::ofstream outFile ( filepath, std::ios::binary );

	    outFile<<"P5\n";
	    outFile << CaptureWidth << " " << CaptureHeight << " 255\n";
	    outFile.write ( ( char* ) data, CaptureSize );
	}

    // Saves the most recent image to file
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

void SaveImage(const char * prefix, unsigned char * pData)
{
	Camera.SaveImage(prefix, pData);
}

#define ERROR -1
#define PortNo	5800

// Generic Socket Server class
class SocketServer
{
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

int ImageLinePos	= 100;      // TODO: need to re-implement this
int ImageCenterPos	= 320;      // TODO: need to re-implement this

class Commands
{
	const char * cameraParamFile = "cameraparam.txt";

public:
	void ReadParams()
	{
		FILE	*	fp = fopen(cameraParamFile, "r");

		if (fp)
		{
			char line[512];
			int	 value;
			double fValue;

			while (fgets(line, sizeof(line), fp))
			{
				switch (line[0])
				{
					case 's':	// Shutter speed
						if (sscanf(line + 1, "%d\n", &value) == 1)
						{
							g_targetShutterSpeed = value;
							g_shutterSpeed = value;
							printf("Shutterspeed = %d\n", value);
						}
						break;

					case 'g':	// Gain
						if (sscanf(line + 1, "%d", &value) == 1)
						{
							g_gain = value;
							printf("Gain = %d\n", value);
						}
						break;

					case 'c':	// Capture Rate
						if (sscanf(line + 1, "%d", &value) == 1)
						{
							FrameRate = value;
							printf("Capture rate = %d\n", value);
						}
						break;

					case 'D':	// whiteDrop
						if (sscanf(line + 1, "%d", &value) == 1)
						{
							g_params.whiteDrop = value;
							printf("WhiteDrop = %d\n", value);
						}
						break;

					case 'm':	// MinSize
						if (sscanf(line + 1, "%d", &value) == 1)
						{
							g_params.minSize = value;
							printf("MinSize = %d\n", value);
						}
						break;

                    case 'W':   //White Count
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            g_params.whiteCnt = value;
                            printf("whiteCnt = %d\n", value);
                        }
                        break;

					case 'B':	// MinBlack
						if (sscanf(line + 1, "%d", &value) == 1)
						{
							g_params.minBlack = value;
							printf("MinBlack = %d\n", value);
						}
						break;

					case 'S':	// MaxSlope
						if (sscanf(line + 1, "%lf", &fValue) == 1)
						{
							g_params.maxSlope = fValue;
							printf("MaxSlope = %f\n", fValue);
						}
						break;

					case 'P':	// MaxParallel
						if (sscanf(line + 1, "%lf", &fValue) == 1)
						{
							g_params.maxParallel = fValue;
							printf("MaxParallel = %f\n", fValue);
						}
						break;

					case 'A':	// MaxAspect
						if (sscanf(line  + 1, "%lf", &fValue) == 1)
						{
							g_params.maxAspect = fValue;
							printf("MaxAspect = %f\n", fValue);
						}
						break;

					case 'a':	// MaxAspect
						if (sscanf(line  + 1, "%lf", &fValue) == 1)
						{
							g_params.minAspect = fValue;
							printf("MaxAspect = %f\n", fValue);
						}
						break;

					case 'T':	// WhiteTarget
						if (sscanf(line + 1, "%lf", &fValue) == 1)
						{
							Camera.m_whiteTarget = fValue;
							printf("WhiteTarget = %f\n", fValue);
						}
						break;

					case 'd':	// Decimate
						if (sscanf(line + 1, "%lf", &fValue) == 1)
						{
							setDecimate(fValue);
							printf("Decimate = %f\n", fValue);
						}
						break;

                    case 'R':   // sample region
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            g_params.sampleRegion = value;
                        }
                        break;

                    case 'F':   // fast tags
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            Camera.m_fast = value;
                        }
                        break;

                    case 'n':   // # threads
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            g_nThreads = value;
                        }
                        break;

                    case 'r':   // refine edges
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            g_refineEdges = value;
                        }
                        break;

                    case 'x':   // sigma
                        if (sscanf(line + 1, "%lf", &fValue) == 1)
                        {
                            g_sigma = fValue;
                        }
                        break;

#ifndef INNOMAKER
                    case 'e':   // auto  Exposure
                        if (sscanf(line + 1, "%d", &value) == 1)
                        {
                            Camera.m_autoExposure = value;
                        }
                        break;

                    case 'C':   // contrast
                        if (sscanf(line + 1, "%lf", &fValue) == 1)
                        {
                            Camera.m_contrast = value;
                        }
                        break;
#endif // !INNOMAKER
				}
			}
		}
	}

	void WriteParams()
	{
		FILE	*	fp = fopen(cameraParamFile, "w");

		if (fp)
		{
			fprintf(fp, "s %d\n", g_shutterSpeed);
			fprintf(fp, "g %d\n", g_gain);
			fprintf(fp, "c %d\n", FrameRate);
			fprintf(fp, "W %d\n", g_params.whiteCnt);
			fprintf(fp, "m %d\n", g_params.minSize);
			fprintf(fp, "D %d\n", g_params.whiteDrop);
			fprintf(fp, "B %d\n", g_params.minBlack);
			fprintf(fp, "S %f\n", g_params.maxSlope);
			fprintf(fp, "P %f\n", g_params.maxParallel);
			fprintf(fp, "A %f\n", g_params.maxAspect);
			fprintf(fp, "a %f\n", g_params.minAspect);
			fprintf(fp, "T %f\n", Camera.m_whiteTarget);
			fprintf(fp, "d %f\n", g_decimate);

            fprintf(fp, "R %d\n", g_params.sampleRegion);
            fprintf(fp, "F %d\n", Camera.m_fast);
            fprintf(fp, "n %d\n", g_nThreads);
            fprintf(fp, "r %d\n", g_refineEdges);
            fprintf(fp, "x %f\n", g_sigma);

#ifndef INNOMAKER
            fprintf(fp, "e %d\n", Camera.m_autoExposure);
            fprintf(fp, "C %f\n", Camera.m_contrast);
#endif // !INNOMAKER

			fclose(fp);

			printf("Camera parameters written\n");
		}
		else
		{
			printf("Cannot write camera parameters\n");
		}
	}

public:
	bool ProcessCommand()
	{
		int ch;

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

		case 0:
			break;

		default:
			printf("ch = %d\n", ch);
		}

		return(true);
	}
} Commands;

/*
 * This thread will listen on port 5800 and supply the information about the
 *	Apriltags to the RoboRio.
 */
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

				if ((command[0] != 'T') && (command[0] != '\n') && (command[0] != '\r') && (command[0] != 'k'))
				{
					printf("command: %s\n", command);
				}

				p	= command;

				while (*p)
				{
					switch (p[0])
					{
					case 'r':
						Camera.ResetFrameCount();
						printf("Reset frame count\n");
						break;

					case 'T':
						SyncCommand(command + 1);
						break;

					case 'p':	// ping
						PingResponse();
						break;

					case 'k':	// keep alive
						m_keepAliveTime = GetTimeMs();
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

							pRegions = Camera.GetFrameRegions(&nFrame, &captureTime);

							if (nFrame != frame)
							{
								char msg[256];

								snprintf(msg, sizeof(msg), "F %d %d %d %d %d %d %Ld %d %d\n",
																	nFrame, ImageLinePos, ImageCenterPos, CaptureWidth, CaptureHeight,
																	Camera.m_framesLost, captureTime - syncTimeOffset, (int) (GetTimeMs() - captureTime), (int) Camera.m_FPS);
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
										snprintf(msg, sizeof(msg), "R %d %0.3f %0.3f %0.3f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f\n",
												pRegion->m_tag,
												pRegion->m_rvec[0],
												pRegion->m_rvec[1],
												pRegion->m_rvec[2],
												pRegion->m_tvec[0],
												pRegion->m_tvec[1],
												pRegion->m_tvec[2],
												pRegion->m_corners[0][0],	// x1
												pRegion->m_corners[0][1], 	// y1
												pRegion->m_corners[1][0],	// x2
												pRegion->m_corners[1][1],	// y2
												pRegion->m_corners[2][0],	// x3
												pRegion->m_corners[2][1],	// y3
												pRegion->m_corners[3][0],	// x4
												pRegion->m_corners[3][1]);	// y4

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
							else
							{
                                sleepMs(3);
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

	short			whiteDrop;				//  8
	short			minSize;				// 10
	short			whiteCnt;				// 12
	short			minBlack;				// 14
	short			maxSlope;				// 16
	short			maxParallel;			// 18
	short			maxAspect;				// 20
	short			shutterSpeed;			// 22
	short			gain;					// 24
	short			captureRate;			// 26
	short			fps;					// 28
	short			blobCount;				// 30
	short			avgWhite;				// 32
	short			whiteTarget;			// 34   Brightness
	short			decimate;				// 36
	short			nThreads;				// 38
	short			fast;					// 40
	short			blur;					// 42
	short			refine;					// 44
	short           contrast;               // 46
	char            autoExposure ;          // 48
	char            piCamera;               // 49
	short           sampleRegion;           // 50
	short           pad;                    // 52

	short			nRegions;				// 54
	// size 56
};

/*
 * This thread will listen on port 5801 and provide an image stream to the ImageVieewer
 *	It can also receive commands from the viewer to control the camera and capture process
 */
class ImageServer : public SocketServer
{
	const int m_portNo = 5801;
	int 			m_frame = -1;
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

	// Process commands from the ImageViewer
	void RunRecv()
	{
		printf("ImageServer receiver started\n");

		while (m_connected)
		{
			int  len;
			int  amount;
			int  level;
			double dbl;
			char command[512];

			if ((len = read(command, sizeof(command) - 1)) == ERROR)
			{
				break;
			}

			command[len]	= 0;

			if (len && command[0] != '\r' && command[0] != '\n' && command[0] != 'k')
			{
				printf("%d: '%s'\n", len, command);
			}

			amount	= 0;
			sscanf(command + 1, "%d", &amount);

			switch (command[0])
			{
			case 'w':
				Commands.WriteParams();
				break;

			case 'I':	// Save next image
                g_saveData = true;
                break;

			case 's':	// Shutter speed
				sscanf(command + 1, "%d", &level);
				Camera.SetNewShutterSpeed(level);
				printf("Shutterspeed = %d\n\n", level);
				break;

			case 'g':	// Gain
				sscanf(command + 1, "%d", &level);
				Camera.SetNewGain(level);
				printf("Gain = %d\n", level);
				break;

			case 'c':	// Capture Rate
				sscanf(command + 1, "%d", &level);
				Camera.setNewCaptureRate(level);
				printf("Capture rate = %d\n", level);
				break;

			case 'D':	// WhiteDrop
				sscanf(command + 1, "%d", &level);
				Camera.setNewWhiteDrop(level);
				printf("witheDrop = %d\n", level);
				break;

			case 'm':	// MinSize
				sscanf(command + 1, "%d", &level);
				Camera.setNewMinSize(level);
				printf("MinSize = %d\n", level);
				break;

			case 'W':	// whiteCnt
				sscanf(command + 1, "%d", &level);
				Camera.setNewWhiteCnt(level);
				printf("whiteCnt = %d\n", level);
				break;

			case 'B':	// MinBlack
				sscanf(command + 1, "%d", &level);
				Camera.setNewMinBlack(level);
				printf("MinBlack = %d\n", level);
				break;

			case 'S':	// MaxSlope
				sscanf(command + 1, "%lf", &dbl);
				Camera.setNewMaxSlope(dbl);
				printf("MaxSlope = %f\n", dbl);
				break;

			case 'P':	// MaxParallel
				sscanf(command + 1, "%lf", &dbl);
				Camera.setNewMaxParallel(dbl);
				printf("MaxParallel = %f\n", dbl);
				break;

			case 'A':	// MaxAspect
				sscanf(command + 1, "%lf", &dbl);
				Camera.setNewMaxAspect(dbl);
				printf("MaxAspect = %f\n", dbl);
				break;

			case 'T':	// WhiteTarget
				sscanf(command + 1, "%d", &level);
				Camera.setNewWhiteTarget(level);
				printf("MaxAspect = %d\n", level);
				break;

            case 'R':   // Sample Range
                sscanf(command + 1, "%d", &level);
                Camera.setNewSampleRegion(level);
                printf("SampleRange = %d\n", level);
                break;

			case 'F':	// Fast Apriltags
				sscanf(command + 1, "%d", &level);
				Camera.SetFastMode(level);
				printf("Fast = %d\n", level);
				break;

			case 'd':	// Decimate
				sscanf(command + 1, "%lf", &dbl);
				setDecimate(dbl);
				printf("Decimate = %f\n", dbl);
				break;

			case 'n':	// # threads
				sscanf(command + 1, "%d", &level);
				setNThreads(level);
				printf("nThreads = %d\n", level);
				break;

			case 'r':	// Refine
				sscanf(command + 1, "%d", &level);
				setRefine(level);
				printf("Refine = %d\n", level);
				break;

			case 'x':	// Sigma/Blur
				sscanf(command + 1, "%lf", &dbl);
				setSigma(dbl);
				printf("Blur = %f\n", dbl);
				break;

#ifndef INNOMAKER
			case 'a':	// AutoExposure
				sscanf(command + 1, "%d", &level);
				Camera.setNewAutoExposure(level != 0);
				printf("AutoExposure = %d\n", level);
				break;

			case 'C':	// Contrast
				sscanf(command + 1, "%d", &level);
                Camera.setNewContrast(level);
				printf("Contrast = %d\n", level);
				break;
#endif // !INNOMAKER

			case 'k':		// Keep alive
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

	struct RegionPoint
	{
		short	x;
		short	y;
	};

	struct RegionData
	{
		short		tagId;
		short		distance;
		RegionPoint	corners[4];
	};

	int64_t m_keepAliveTime = 0;

	// Sends an image stream and image data to the ImageViewer
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


					while (true)
					{
						int	nFrame;
						int64_t time;
						int blobCount;
						int avgWhite;
						int whiteTarget;
						int contrast;
						bool autoExp;

						sleepMs(100);

						pRegions = Camera.GetFrameRegions(&nFrame, &time, &blobCount, &avgWhite, &whiteTarget, &contrast, &autoExp);

						if (nFrame != m_frame)
						{
							static unsigned char frameData[CaptureSize];

							{
								unsigned char * pJpeg;
								long			size;
								unsigned long	key	= 0xaa55aa55;

#ifdef R1080
#define jpegScale 4
#else   // !R1080
#define jpegScale 2
#endif // !R1080
								Camera.GetScaledFrame(frameData, jpegScale);

								pJpeg = CreateJpeg(frameData, CaptureWidth/jpegScale, CaptureHeight/jpegScale, 1, size);

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

								hdr.whiteDrop	= g_params.whiteDrop;
								hdr.minSize		= g_params.minSize;
								hdr.whiteCnt	= g_params.whiteCnt;
								hdr.minBlack	= g_params.minBlack;
								hdr.maxSlope	= (short) (g_params.maxSlope * 100 + 0.5);
								hdr.maxParallel	= (short) (g_params.maxParallel * 100 + 0.5);
								hdr.maxAspect	= (short) ((g_params.maxAspect - 1) * 100 + 0.5);
								hdr.shutterSpeed = g_shutterSpeed;
								hdr.gain		= g_gain;
								hdr.captureRate = FrameRate;
								hdr.fps			= (short) (Camera.m_FPS * 10);
								hdr.blobCount	= blobCount;
								hdr.avgWhite	= avgWhite;
								hdr.whiteTarget	= whiteTarget;
								hdr.decimate	= (short) (g_decimate * 10);
								hdr.nThreads	= g_nThreads;
								hdr.fast		= Camera.m_fast;
								hdr.blur		= (short) (g_sigma * 10);
								hdr.refine		= g_refineEdges;
#ifdef INNOMAKER
                                hdr.piCamera      = false;
#else   // !INNOMAKER
                                hdr.piCamera      = true;
								hdr.autoExposure  = autoExp;
								hdr.contrast      = contrast;
#endif // !INNOMAKER
								hdr.nRegions 	= 0;

								int regionSize = 0;
								RegionData * pRegionData = 0;

								if (pRegions)
								{
									regionSize	= pRegions->size() * sizeof(RegionData);

									hdrSize	+= regionSize;

									pRegionData = new RegionData[pRegions->size()];

									memset(pRegionData, 0, pRegions->size());

									int i = 0;
									for (ImageRegion * pRegion : *pRegions)
									{
										pRegionData[i].tagId	= pRegion->m_tag;
										pRegionData[i].distance = (short) (sqrt(pRegion->m_tvec[0] * pRegion->m_tvec[0] +
                                                                                    pRegion->m_tvec[1] * pRegion->m_tvec[1] +
                                                                                    pRegion->m_tvec[2] * pRegion->m_tvec[2]) * 10);

										for (int j = 0 ; j < 4 ; j++)
										{
											pRegionData[i].corners[j].x = pRegion->m_corners[j][0] * 10;
											pRegionData[i].corners[j].y = pRegion->m_corners[j][1] * 10;
										}

										i++;
									}
									hdr.nRegions	= pRegions->size();
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
							}
						}
						deleteImageRegions(pRegions);
					}

					Disconnect();

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

	printf("Shutter Speed = %d, Gain = %d, Frame Rate = %d\n", g_shutterSpeed, g_gain, FrameRate);

	Commands.ReadParams();

	Camera.StartCapture();

	printf("Normal Settings\n");

	sleep(5);

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


#include <mutex>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sys/mman.h>
#include <stdlib.h>

#include <libcamera/libcamera.h>
#include <drm_fourcc.h>
#include <cstring>
#include <fstream>
#include <mutex>
#include <atomic>

#include "cameraCapture.h"
#include "dma_heaps.cpp"
//#include "event_loop.h"

using namespace libcamera;



class FramePointer
{
public:
    int m_cameraNo;
    unsigned char * m_pData;
    int m_size;

    FramePointer(int cameraNo, unsigned char * pData, int size)
    {
        m_cameraNo = cameraNo;
        m_pData = pData;
        m_size = size;
    }
};

void SaveMonoImageToFile (const char * filepath, unsigned char *data, int width, int height)
{
    std::ofstream outFile ( filepath, std::ios::binary );

    outFile<<"P5\n";

    printf("SaveImageToFile: width = %d, height = %d\n", width, height);
    outFile << width << " " << height << " 255\n";
    outFile.write((char *) data, width * height);
    outFile.close();
}

//#define MaxCameras  2

class MyCamera
{
public:
    std::shared_ptr<Camera> camera;
    Stream *stream = 0;
    std::vector<std::unique_ptr<Request>> requests;
    unsigned char * m_pImage = 0;
    int m_frameNo;
    int m_requestWidth = 0;
    int m_requestHeight = 0;
    int m_width = 0;
    int m_height = 0;
    int m_stride = 0;
    int m_size = 0;
    uint64_t m_time;
    std::mutex m_mutex;
    double m_exposure = 0;
    int m_cameraNo;

    MyCamera(int cameraNo)
    {
        m_cameraNo = cameraNo;
    }

    void requestSize(int width, int height)
    {
        m_requestWidth = width;
        m_requestHeight = height;
    }

    bool setSize(int width, int height, int stride)
    {
        if (m_pImage == 0)
        {
            m_width = width;
            m_height = height;
            m_stride = stride;
            m_pImage = (unsigned char *) malloc(width * height);
        }
        else if ((width != m_width) || (height != m_height) || (stride != m_stride))
        {
            printf("ERROR: inconsistant size: %d!=%d or %d!=%d\n", m_width, width, m_height, height);
            return false;
        }

        return true;
    }

    void setImage(unsigned char * pImage, int width, int height, int stride, uint64_t time)
    {
        m_time = time;

        if ((width == m_width) && (height == m_height))
        {
            unsigned char * pSrc = pImage;
            unsigned char * pDst = m_pImage;
            std::lock_guard<std::mutex> lock(m_mutex);

            for (int y = 0 ; y < height ; y++)
            {
                for (int x = 0 ; x < width ; x++, pSrc += 3, pDst++)
                {
                    *pDst = (pSrc[0] + pSrc[1] + pSrc[2]) / 3;
                }

                pSrc += stride - (width * 3);
            }

            m_frameNo++;

//            if (frameNo <= 5)
//            {
//                char file[128];
//                snprintf(file, 127, "image%d.ppm", frameNo);
//                SaveMonoImageToFile(file, m_pImage, m_width, m_height);
//            }
        }
        else
        {
            printf("setImage invalid size: %d!=%d or %d!=%d", m_width, width, m_height, height);
        }
    }

    void getImageSize(int& width, int& height)
    {
        width = m_width;
        height = m_height;
    }

    int getImage(unsigned char * pImage, int size, uint64_t &time)
    {
        if (size == m_width * m_height)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            time = m_time;
            memmove(pImage, m_pImage, size);

            return(m_frameNo);
        }

        printf("getImage invalid size: %d!=%d\n", size, m_width * m_height);

        return(-1);
    }

    /*
     * Returns the image scaled down by a factor of scale
     */
    bool getScaledImage(unsigned char * pDst, int size, int scale)
    {
        int sizeNeeded = (m_width / scale) * (m_height / scale);

        if (size >= sizeNeeded)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            unsigned char * s;

            for (int y = 0 ; y < m_height ; y += scale)
            {
                s = m_pImage + (y * m_width);
                for (int x = 0 ; x < m_width ; x += scale, s += scale)
                {
                    *pDst++ = *s;
                }
            }

            return true;
        }

        printf("scaled buffer too small\n");

        return false;
    }
};

std::vector<MyCamera *> cameras;

//static std::shared_ptr<Camera> camera;
DmaHeap dma_heap_;
std::map<Stream *, std::vector<std::unique_ptr<FrameBuffer>>> frame_buffers_;
std::map<FrameBuffer *, FramePointer *> mapped_buffers_;
//static EventLoop loop;

/*
 * ----------------------------------------------------------------------------
 * Camera Naming.
 *
 * Applications are responsible for deciding how to name cameras, and present
 * that information to the users. Every camera has a unique identifier, though
 * this string is not designed to be friendly for a human reader.
 *
 * To support human consumable names, libcamera provides camera properties
 * that allow an application to determine a naming scheme based on its needs.
 *
 * In this example, we focus on the location property, but also detail the
 * model string for external cameras, as this is more likely to be visible
 * information to the user of an externally connected device.
 *
 * The unique camera ID is appended for informative purposes.
 */
std::string cameraName(Camera *camera)
{
	const ControlList &props = camera->properties();
	std::string name;

#ifdef notdef
	const auto &location = props.get(properties::Location);
	if (location) {
		switch (*location) {
		case properties::CameraLocationFront:
			name = "Internal front camera";
			break;
		case properties::CameraLocationBack:
			name = "Internal back camera";
			break;
		case properties::CameraLocationExternal:
			name = "External camera";
			const auto &model = props.get(properties::Model);
			if (model)
				name = " '" + *model + "'";
			break;
		}
	}

	name += " (" + camera->id() + ")";
#endif // notdef
    const auto &model = props.get(properties::Model);
    name = *model;

	return name;
}

static void processRequest(Request *request);

static void requestComplete(Request *request)
{
//    printf("requestComplete\n");
	if (request->status() == Request::RequestCancelled)
		return;

//    printf("processRequest\n");
    processRequest(request);

//	loop.callLater(std::bind(&processRequest, request));
}

//static int nFrames = 0;
//static int imgWidth;
//static int imgHeight;
//static int imgStride;

//static void convertToMono(unsigned char * dst, unsigned char * src, int width, int height, int stride)
//{
//    for (int y = 0 ; y < height ; y++)
//    {
//        for (int x = 0 ; x < width ; x++, src += 3, dst++)
//        {
//            *dst = (src[0] + src[1] + src[2]) / 3;
//        }
//
//        src += stride - (width * 3);
//    }
//}

uint64_t lastCapture = 0;

std::atomic<float> g_newExposure = 0;
std::atomic<int> g_newShutterSpeed = 20;
std::atomic<int> g_newFrameRate = 50;
std::atomic<int> g_newBrightness = 0;
std::atomic<int> g_newContrast = 100;

static void processRequest(Request *request)
{
    uint64_t time = GetTimeMs();

//	std::cout << std::endl
//		  << "Request completed: " << request->toString() << std::endl;

	/*
	 * When a request has completed, it is populated with a metadata control
	 * list that allows an application to determine various properties of
	 * the completed request. This can include the timestamp of the Sensor
	 * capture, or its gain and exposure values, or properties from the IPA
	 * such as the state of the 3A algorithms.
	 *
	 * ControlValue types have a toString, so to examine each request, print
	 * all the metadata for inspection. A custom application can parse each
	 * of these items and process them according to its needs.
	 */
#ifdef dump
	const ControlList &requestMetadata = request->metadata();
	for (const auto &ctrl : requestMetadata) {
		const ControlId *id = controls::controls.at(ctrl.first);
		const ControlValue &value = ctrl.second;

		std::cout << "\t" << id->name() << " = " << value.toString()
			  << std::endl;
	}
#endif // dump

	/*
	 * Each buffer has its own FrameMetadata to describe its state, or the
	 * usage of each buffer. While in our simple capture we only provide one
	 * buffer per request, a request can have a buffer for each stream that
	 * is established when configuring the camera.
	 *
	 * This allows a viewfinder and a still image to be processed at the
	 * same time, or to allow obtaining the RAW capture buffer from the
	 * sensor along with the image as processed by the ISP.
	 */
	const Request::BufferMap &buffers = request->buffers();
	for (auto bufferPair : buffers) {
		FrameBuffer *buffer = bufferPair.second;

//		nFrames++;

        FramePointer * pFramePointer = mapped_buffers_[buffer];

        if (pFramePointer != 0)
        {
            MyCamera * pCamera = cameras[pFramePointer->m_cameraNo];

            pCamera->setImage(pFramePointer->m_pData, pCamera->m_width, pCamera->m_height, pCamera->m_stride, time);

//            if (nFrames <= 5)
//            {
////                convertToMono(buf, buf, imgWidth, imgHeight, imgStride);
//
//                char file[128];
//                snprintf(file, 127, "image%d-%d.ppm", cameraNo, nFrames);
//                SaveMonoImageToFile(file, buf, imgWidth, imgHeight);
//            }

            /*
             * Image data can be accessed here, but the FrameBuffer
             * must be mapped by the application
             */
            /* Re-queue the Request to the camera. */
            request->reuse(Request::ReuseBuffers);

            ControlList &controls = request->controls();
            controls.set(controls::ExposureValue, g_newExposure);
            controls.set(controls::ExposureTime, g_newShutterSpeed * 1000);
            controls.set(controls::FrameDurationLimits, libcamera::Span<const std::int64_t, 2>({1000000/g_newFrameRate, 1000000/g_newFrameRate}));
            controls.set(controls::Brightness, g_newBrightness / 100.0);
            controls.set(controls::Contrast, g_newContrast / 100.0);

            pCamera->camera->queueRequest(request);
        }
        else
        {
            printf("ERROR: Cannot find framePointer\n");
        }
    }
}

//#define MaxCameras 2
//static int nCameras = 0;
static std::vector<std::string> cameraList;

std::vector<std::string> getCameraList()
{
	std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
	cm->start();

	/*
	 * Just as a test, generate names of the Cameras registered in the
	 * system, and list them.
	 */
    int cameraNo = 0;
	for (auto const &camera : cm->cameras())
	{
        std::string name = cameraName(camera.get());
		std::cout << " - " << name << std::endl;
		cameraList.push_back(name);

		MyCamera * pCamera = new MyCamera(cameraNo);
		cameras.push_back(pCamera);

		cameraNo++;
    }

    cm->stop();

    return(cameraList);
}

int initializeCamera(int shutterSpeed, int iso)
{
	/*
	 * --------------------------------------------------------------------
	 * Create a Camera Manager.
	 *
	 * The Camera Manager is responsible for enumerating all the Camera
	 * in the system, by associating Pipeline Handlers with media entities
	 * registered in the system.
	 *
	 * The CameraManager provides a list of available Cameras that
	 * applications can operate on.
	 *
	 * When the CameraManager is no longer to be used, it should be deleted.
	 * We use a unique_ptr here to manage the lifetime automatically during
	 * the scope of this function.
	 *
	 * There can only be a single CameraManager constructed within any
	 * process space.
	 */
	static std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
	cm->start();

	/*
	 * Just as a test, generate names of the Cameras registered in the
	 * system, and list them.
	 */

    int nCameras = 0;
	for (auto const &camera : cm->cameras())
	{
        std::string name = cameraName(camera.get());
		std::cout << " - " << name << std::endl;
		cameraList.push_back(name);
		nCameras++;
    }

	/*
	 * --------------------------------------------------------------------
	 * Camera
	 *
	 * Camera are entities created by pipeline handlers, inspecting the
	 * entities registered in the system and reported to applications
	 * by the CameraManager.
	 *
	 * In general terms, a Camera corresponds to a single image source
	 * available in the system, such as an image sensor.
	 *
	 * Application lock usage of Camera by 'acquiring' them.
	 * Once done with it, application shall similarly 'release' the Camera.
	 *
	 * As an example, use the first available camera in the system after
	 * making sure that at least one camera is available.
	 *
	 * Cameras can be obtained by their ID or their index, to demonstrate
	 * this, the following code gets the ID of the first camera; then gets
	 * the camera associated with that ID (which is of course the same as
	 * cm->cameras()[0]).
	 */
    int cameraNo = 0;
	if (cm->cameras().empty()) {
		std::cout << "No cameras were identified on the system."
			  << std::endl;
		cm->stop();
		return EXIT_FAILURE;
	}

  for (cameraNo = 0 ; cameraNo < nCameras ; cameraNo++)
  {
    MyCamera * pCamera;

	std::string cameraId = cm->cameras()[cameraNo]->id();

	if (cameraNo >= (int) cameras.size())
	{
        pCamera = new MyCamera(cameraNo);
        cameras.push_back(pCamera);
	}
	else
	{
        pCamera = cameras[cameraNo];
    }

	pCamera->camera = cm->cameras()[cameraNo];  //cm->get(cameraId);
	pCamera->camera->acquire();

	/*
	 * Stream
	 *
	 * Each Camera supports a variable number of Stream. A Stream is
	 * produced by processing data produced by an image source, usually
	 * by an ISP.
	 *
	 *   +-------------------------------------------------------+
	 *   | Camera                                                |
	 *   |                +-----------+                          |
	 *   | +--------+     |           |------> [  Main output  ] |
	 *   | | Image  |     |           |                          |
	 *   | |        |---->|    ISP    |------> [   Viewfinder  ] |
	 *   | | Source |     |           |                          |
	 *   | +--------+     |           |------> [ Still Capture ] |
	 *   |                +-----------+                          |
	 *   +-------------------------------------------------------+
	 *
	 * The number and capabilities of the Stream in a Camera are
	 * a platform dependent property, and it's the pipeline handler
	 * implementation that has the responsibility of correctly
	 * report them.
	 */

	/*
	 * --------------------------------------------------------------------
	 * Camera Configuration.
	 *
	 * Camera configuration is tricky! It boils down to assign resources
	 * of the system (such as DMA engines, scalers, format converters) to
	 * the different image streams an application has requested.
	 *
	 * Depending on the system characteristics, some combinations of
	 * sizes, formats and stream usages might or might not be possible.
	 *
	 * A Camera produces a CameraConfigration based on a set of intended
	 * roles for each Stream the application requires.
	 */
	std::unique_ptr<CameraConfiguration> config =
		pCamera->camera->generateConfiguration( { StreamRole::Viewfinder } );

    int size = config->size();
    printf("%d Streams:\n", size);
    for (int i = 0 ; i < size ; i++)
    {
        StreamConfiguration &sConfig = config->at(i);
        printf("%d: width=%d, height=%d\n", i, sConfig.size.width, sConfig.size.height);
    }

	/*
	 * The CameraConfiguration contains a StreamConfiguration instance
	 * for each StreamRole requested by the application, provided
	 * the Camera can support all of them.
	 *
	 * Each StreamConfiguration has default size and format, assigned
	 * by the Camera depending on the Role the application has requested.
	 */
	StreamConfiguration &streamConfig = config->at(0);
    streamConfig.pixelFormat = PixelFormat(DRM_FORMAT_RGB888);

    if (pCamera->m_requestWidth != 0 && pCamera->m_requestHeight != 0)
    {
        streamConfig.size.width = pCamera->m_requestWidth; //1456;
        streamConfig.size.height = pCamera->m_requestHeight; //1088;
    }

    int r = pCamera->camera->configure(config.get());
    if (r)
    {
        printf("config failed\n");
        return EXIT_FAILURE;
    }
    printf("Width=%d,Height=%d,Stride=%d\n", streamConfig.size.width, streamConfig.size.height, streamConfig.stride);

//    imgWidth = streamConfig.size.width;
//    imgHeight = streamConfig.size.height;
//    imgStride = streamConfig.stride;

	std::cout << "Default viewfinder configuration is: "
		  << streamConfig.toString() << std::endl;

	/*
	 * Each StreamConfiguration parameter which is part of a
	 * CameraConfiguration can be independently modified by the
	 * application.
	 *
	 * In order to validate the modified parameter, the CameraConfiguration
	 * should be validated -before- the CameraConfiguration gets applied
	 * to the Camera.
	 *
	 * The CameraConfiguration validation process adjusts each
	 * StreamConfiguration to a valid value.
	 */


	/*
	 * Validating a CameraConfiguration -before- applying it will adjust it
	 * to a valid configuration which is as close as possible to the one
	 * requested.
	 */
	config->validate();
	std::cout << "Validated viewfinder configuration is: "
		  << streamConfig.toString() << std::endl;

	/*
	 * Once we have a validated configuration, we can apply it to the
	 * Camera.
	 */
	pCamera->camera->configure(config.get());

	/*
	 * --------------------------------------------------------------------
	 * Buffer Allocation
	 *
	 * Now that a camera has been configured, it knows all about its
	 * Streams sizes and formats. The captured images need to be stored in
	 * framebuffers which can either be provided by the application to the
	 * library, or allocated in the Camera and exposed to the application
	 * by libcamera.
	 *
	 * An application may decide to allocate framebuffers from elsewhere,
	 * for example in memory allocated by the display driver that will
	 * render the captured frames. The application will provide them to
	 * libcamera by constructing FrameBuffer instances to capture images
	 * directly into.
	 *
	 * Alternatively libcamera can help the application by exporting
	 * buffers allocated in the Camera using a FrameBufferAllocator
	 * instance and referencing a configured Camera to determine the
	 * appropriate buffer size and types to create.
	 */
    StreamConfiguration cfg = streamConfig;
	{
		Stream *stream = cfg.stream();
		std::vector<std::unique_ptr<FrameBuffer>> fb;

		for (unsigned int i = 0; i < cfg.bufferCount; i++)
		{
			std::string name("rpicam-apps" + std::to_string(i));
			libcamera::UniqueFD fd = dma_heap_.alloc(name.c_str(), cfg.frameSize);

			if (!fd.isValid())
				throw std::runtime_error("failed to allocate capture buffers for stream");

			std::vector<FrameBuffer::Plane> plane(1);
			plane[0].fd = libcamera::SharedFD(std::move(fd));
			plane[0].offset = 0;
			plane[0].length = cfg.frameSize;

			fb.push_back(std::make_unique<FrameBuffer>(plane));
			unsigned char *memory = (unsigned char *) mmap(NULL, cfg.frameSize, PROT_READ | PROT_WRITE, MAP_SHARED, plane[0].fd.get(), 0);
            mapped_buffers_[fb.back().get()] = new FramePointer(cameraNo, memory, cfg.frameSize);

            if (!pCamera->setSize(cfg.size.width, cfg.size.height, cfg.stride))
            {
                return EXIT_FAILURE;
            }
        }

		frame_buffers_[stream] = std::move(fb);
	}

	/*
	 * --------------------------------------------------------------------
	 * Frame Capture
	 *
	 * libcamera frames capture model is based on the 'Request' concept.
	 * For each frame a Request has to be queued to the Camera.
	 *
	 * A Request refers to (at least one) Stream for which a Buffer that
	 * will be filled with image data shall be added to the Request.
	 *
	 * A Request is associated with a list of Controls, which are tunable
	 * parameters (similar to v4l2_controls) that have to be applied to
	 * the image.
	 *
	 * Once a request completes, all its buffers will contain image data
	 * that applications can access and for each of them a list of metadata
	 * properties that reports the capture parameters applied to the image.
	 */
   pCamera->stream = streamConfig.stream();
	const std::vector<std::unique_ptr<FrameBuffer>> &buffers = frame_buffers_[pCamera->stream];
//	std::vector<std::unique_ptr<Request>> requests;
	for (unsigned int i = 0; i < buffers.size(); ++i) {
		std::unique_ptr<Request> request = pCamera->camera->createRequest();
		if (!request)
		{
			std::cerr << "Can't create request" << std::endl;
			return EXIT_FAILURE;
		}

		const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
		int ret = request->addBuffer(pCamera->stream, buffer.get());
		if (ret < 0)
		{
			std::cerr << "Can't set buffer for request"
				  << std::endl;
			return EXIT_FAILURE;
		}

//		cameras[cameraNo].m_exposure = g_newExposure;

		/*
		 * Controls can be added to a request on a per frame basis.
		 */
		ControlList &controls = request->controls();
//		controls.set(controls::Brightness, 0.5);
//        controls.set(controls::ExposureTime, 5000);
        controls.set(controls::FrameDurationLimits, libcamera::Span<const std::int64_t, 2>({1000000/g_newFrameRate, 1000000/g_newFrameRate}));
        controls.set(controls::ExposureTime, g_newShutterSpeed * 1000);
        controls.set(controls::ExposureValue, g_newExposure);
        controls.set(controls::Brightness, g_newBrightness / 100.0);
        controls.set(controls::Contrast, g_newContrast / 100.0);
//        controls.set(controls::AnalogueGain, 10.0);

		pCamera->requests.push_back(std::move(request));
	}

	/*
	 * --------------------------------------------------------------------
	 * Signal&Slots
	 *
	 * libcamera uses a Signal&Slot based system to connect events to
	 * callback operations meant to handle them, inspired by the QT graphic
	 * toolkit.
	 *
	 * Signals are events 'emitted' by a class instance.
	 * Slots are callbacks that can be 'connected' to a Signal.
	 *
	 * A Camera exposes Signals, to report the completion of a Request and
	 * the completion of a Buffer part of a Request to support partial
	 * Request completions.
	 *
	 * In order to receive the notification for request completions,
	 * applications shall connecte a Slot to the Camera 'requestCompleted'
	 * Signal before the camera is started.
	 */
	pCamera->camera->requestCompleted.connect(requestComplete);

	/*
	 * --------------------------------------------------------------------
	 * Start Capture
	 *
	 * In order to capture frames the Camera has to be started and
	 * Request queued to it. Enough Request to fill the Camera pipeline
	 * depth have to be queued before the Camera start delivering frames.
	 *
	 * For each delivered frame, the Slot connected to the
	 * Camera::requestCompleted Signal is called.
	 */
	pCamera->camera->start();
	for (std::unique_ptr<Request> &request :pCamera->requests)
		pCamera->camera->queueRequest(request.get());
  }

//  while (1) { sleep(1); }

    return EXIT_SUCCESS;
}

bool getImageSize(int cameraNo, int& width, int& height)
{
    if (cameraNo < (int) cameras.size())
    {
        cameras[cameraNo]->getImageSize(width, height);

        return true;
    }

    return false;
}

int captureImage(int cameraNo, unsigned char * out_buffer, int out_buffer_size, uint64_t &time)
{
    if (cameraNo < (int) cameras.size())
    {
        return cameras[cameraNo]->getImage(out_buffer, out_buffer_size, time);
    }

    return(-1);
}

bool getScaledImage(int cameraNo, unsigned char * pDst, int size, int scale)
{
    if (cameraNo < (int) cameras.size())
    {
        return cameras[cameraNo]->getScaledImage(pDst, size, scale);
    }

    return(false);

}

int getCurrentFrameNo(int cameraNo)
{
    if (cameraNo < (int) cameras.size())
    {
        return cameras[cameraNo]->m_frameNo;
    }

    printf("getCurrentFrameNo: invalid camera = %d\n", cameraNo);

    return(-1);
}

bool requestSize(int cameraNo, int width, int height)
{
    if (cameraNo < (int) cameras.size())
    {
        cameras[cameraNo]->requestSize(width, height);

        return true;
    }

    return false;
}

std::mutex mutex;

void setShutterSpeed(int speed)
{
    g_newShutterSpeed = speed;
    printf("newShutterSpeed=%d\n", (int) g_newShutterSpeed);
}

void setExposure(int exposure)
{
    g_newExposure = exposure / 100.0;
    printf("newExposure=%f\n", (float) g_newExposure);
}

void setFrameRate(int rate)
{
    g_newFrameRate = rate;
    printf("newFrameRate=%d\n", (int) g_newFrameRate);
}

void grabImage()
{
}

void setContrast(int contrast)
{
    g_newContrast = contrast;
    printf("newContrast=%d\n", (int) g_newContrast);
}

void setAutoExposure(bool autoExp)
{
}

void setBrightness(int brightness)
{
    g_newBrightness = brightness;
    printf("newBrightness=%d\n", (int) g_newBrightness);
}


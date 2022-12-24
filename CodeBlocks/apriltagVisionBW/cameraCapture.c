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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <getopt.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>

#include "cameraCapture.h"

#define u8 unsigned char
#define  LOGD(...)  do {printf(__VA_ARGS__);printf("\n");} while (0)
#define DBG(fmt, args...) LOGD("%s:%d, " fmt, __FUNCTION__, __LINE__, ##args);
#define ASSERT(b) \
do \
{ \
    if (!(b)) \
    { \
        LOGD("error on %s:%d", __FUNCTION__, __LINE__); \
        return 0; \
    } \
} while (0)

#define VIDEO_DEVICE "/dev/video0"
#define IMAGE_WIDTH 1280
#define IMAGE_HEIGHT 800
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)

#define BUFFER_COUNT 3


#define  DEMO_NAME          "mipi raw capture demo"
#define  DEMO_MAINVERSION    (  0)  /**<  Main Version: X.-.-   */
#define  DEMO_VERSION        (  0)  /**<       Version: -.X.-   */
#define  DEMO_SUBVERSION     (  3)  /**<    Subversion: -.-.X   */

int cam_fd = -1;
struct v4l2_buffer video_buffer[BUFFER_COUNT];
u8* video_buffer_ptr[BUFFER_COUNT];
u8 buf[IMAGE_SIZE];

int cam_open()
{
    cam_fd = open(VIDEO_DEVICE, O_RDWR);

    if (cam_fd >= 0) return 0;
    else return -1;
}

int cam_close()
{
    close(cam_fd);

    return 0;
}

int cam_select(int index)
{
    int ret;

    int input = index;
    ret = ioctl(cam_fd, VIDIOC_S_INPUT, &input);
    return ret;
}

int cam_init()
{
    int i;
    int ret;
    struct v4l2_format format;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_SGRBG10;//10bit raw
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10P;//10bit raw
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_Y10;//10bit raw
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;//8 bit gray
    format.fmt.pix.width = IMAGE_WIDTH;
    format.fmt.pix.height = IMAGE_HEIGHT;
    ret = ioctl(cam_fd, VIDIOC_TRY_FMT, &format);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_TRY_FMT) failed %d(%s)", errno, strerror(errno));
        return ret;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(cam_fd, VIDIOC_S_FMT, &format);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_S_FMT) failed %d(%s)", errno, strerror(errno));
        return ret;
    }

    struct v4l2_requestbuffers req;
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(cam_fd, VIDIOC_REQBUFS, &req);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_REQBUFS) failed %d(%s)", errno, strerror(errno));
        return ret;
    }
    DBG("req.count: %d", req.count);
    if (req.count < BUFFER_COUNT)
    {
        DBG("request buffer failed");
        return ret;
    }

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = req.type;
    buffer.memory = V4L2_MEMORY_MMAP;
    for (i=0; i<req.count; i++)
    {
        buffer.index = i;
        ret = ioctl (cam_fd, VIDIOC_QUERYBUF, &buffer);
        if (ret != 0)
        {
            DBG("ioctl(VIDIOC_QUERYBUF) failed %d(%s)", errno, strerror(errno));
            return ret;
        }
        DBG("buffer.length: %d", buffer.length);
        DBG("buffer.m.offset: %d", buffer.m.offset);
        video_buffer_ptr[i] = (u8*) mmap(NULL, buffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, cam_fd, buffer.m.offset);//�ڴ�ӳ��
        if (video_buffer_ptr[i] == MAP_FAILED)
        {
            DBG("mmap() failed %d(%s)", errno, strerror(errno));
            return -1;
        }

        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        ret = ioctl(cam_fd, VIDIOC_QBUF, &buffer);
        if (ret != 0)
        {
            DBG("ioctl(VIDIOC_QBUF) failed %d(%s)", errno, strerror(errno));
            return ret;
        }
    }

    int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(cam_fd, VIDIOC_STREAMON, &buffer_type);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_STREAMON) failed %d(%s)", errno, strerror(errno));
        return ret;
    }

    DBG("cam init done.");

    return 0;
}

int cam_get_image(u8* out_buffer, int out_buffer_size)
{
    int ret;
    struct v4l2_buffer buffer;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = BUFFER_COUNT;
    ret = ioctl(cam_fd, VIDIOC_DQBUF, &buffer);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_DQBUF) failed %d(%s)", errno, strerror(errno));
        return ret;
    }

    if (buffer.index < 0 || buffer.index >= BUFFER_COUNT)
    {
        DBG("invalid buffer index: %d", buffer.index);
        return ret;
    }

    // DBG("dequeue done, index: %d", buffer.index);
    memcpy(out_buffer, video_buffer_ptr[buffer.index], IMAGE_SIZE);
    // DBG("copy done.");

    ret = ioctl(cam_fd, VIDIOC_QBUF, &buffer);
    if (ret != 0)
    {
        DBG("ioctl(VIDIOC_QBUF) failed %d(%s)", errno, strerror(errno));
        return ret;
    }
    // DBG("enqueue done.");

    return 0;
}

//ouyang add for free memory
void memory_free(void)
{
    int i,ret;
    for (i=0; i<BUFFER_COUNT; i++)
    {
    ret=munmap(video_buffer_ptr[i],IMAGE_SIZE);
    if (ret != 0)
    { DBG("Munmap failed!!."); }
    else
    {  DBG("Munmap Success!!.");}
    //free();
    }
}

int sensor_set_option(const char * a10cTarget, unsigned int ctlID, unsigned int val)
{
    int    ee, rc;
	struct v4l2_control  ctl;

		// Set new value.
    memset(&ctl, 0, sizeof(ctl));
    ctl.id    = ctlID;

    ctl.value = val;

    rc = ioctl(cam_fd , VIDIOC_S_CTRL, &ctl);
    if(rc<0)
    {
        if((EINVAL!=errno)&&(ERANGE!=errno)){ee=-3; goto fail;} //general error.
        else                                {ee=-4; goto fail;} //Value out of Range Error.
    }

	ee = 0;
fail:
	switch(ee)
	{
		case 0:
			break;
		case -1:
		case -3:
		case -5:
			//syslog(LOG_ERR, "%s():  ioctl(%s) throws Error (%d(%s))!\n", __FUNCTION__, (-3==ee)?("VIDIOC_S_CTRL"):("VIDIOC_G_CTRL"), errno, strerror(errno));
			printf( "%s():  ioctl(%s) throws Error (%d(%s))!\n", __FUNCTION__, (-3==ee)?("VIDIOC_S_CTRL"):("VIDIOC_G_CTRL"), errno, strerror(errno));
			break;
		case -2:
		case -6:
			//syslog(LOG_ERR, "%s():  V4L2_CID_.. is unsupported!\n", __FUNCTION__);
			printf( "%s():  V4L2_CID_.. is unsupported!\n", __FUNCTION__);
			break;
		case -4:
			//syslog(LOG_ERR, "%s():  %s Value is out of range (or V4L2_CID_.. is invalid)!\n", __FUNCTION__, a10cTarget);
			printf( "%s():  %s Value is out of range (or V4L2_CID_.. is invalid)!\n", __FUNCTION__, a10cTarget);
			break;
	}

	return(ee);
}

int setShutterSpeed(int speed)
{
    return(sensor_set_option("Speed", V4L2_CID_EXPOSURE, 8721 * speed));
}

int setGain(int gain)
{
    return(sensor_set_option("Speed", V4L2_CID_GAIN, gain));
}

int  sensor_set_parameters(int optGain, int optShutter,int opthflip,int optvflip)
{
	int    ee, target;
	unsigned int    ctlID, val;
	char   a10cTarget[11];

	for(target= 0; target< 4; target++)
	{
		switch(target)
		{
			case 0:  sprintf(a10cTarget,"Gain"    );  ctlID = V4L2_CID_GAIN;      val = optGain;
                break;
			case 1:  sprintf(a10cTarget,"Exposure");  ctlID = V4L2_CID_EXPOSURE;  val = optShutter;
                break;
            case 2:  sprintf(a10cTarget,"Hflip");     ctlID = V4L2_CID_HFLIP;     val = opthflip;
                break;
            case 3:  sprintf(a10cTarget,"Vflip");     ctlID = V4L2_CID_VFLIP;     val = optvflip;
  break;
		}

        if ((ee = sensor_set_option(a10cTarget, ctlID, val)) != 0)
        {
            return(ee);
        }
    }

    return(0);
}

int initializeCamera(int shutterSpeed, int gain)
{
    int    ret;
    int    optShutter=8721 * shutterSpeed;
	int    opthflip=0;
	int    optvflip=0;
    float  optGain=gain; //128;

    ret = cam_open();
    ASSERT(ret==0);

    ret =  sensor_set_parameters( optGain, optShutter,opthflip, optvflip);
    ASSERT(ret==0);

    ret = cam_select(0);
    ASSERT(ret==0);

    ret = cam_init();
    ASSERT(ret==0);

    return(0);
}

int captureImage(unsigned char * out_buffer, int out_buffer_size)
{
    ASSERT(out_buffer_size >= IMAGE_SIZE);
    int ret = cam_get_image(out_buffer, IMAGE_SIZE);
    ASSERT(ret==0);

    return(0);
}




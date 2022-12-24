#pragma once

//#include "resource.h"

#ifdef WIN32
#include <windows.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <gdiplus.h>
#include <stdio.h>

#pragma warning (disable: 4305 4091 4018 4101 4341 4102 4135 4746 4761 4091 4018 4341 4244 4245 4759 4309 4146 4996 4800 4250)

#define CaptureWidth	640
#define CaptureHeight	480

#define snprintf	_snprintf

#else	// !WIN32
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused-variable"

#define FALSE	0
#define TRUE	1

#define CaptureWidth	640
#define CaptureHeight	480

typedef int		BOOL;

struct RECT
{
	int	left;
	int	top;
	int right;
	int bottom;
};

struct POINT
{
	int	x;
	int	y;
};
#endif	// !WIN32

#define FIND_CORNERSx

extern int MinH;
extern int MaxH;
extern int MinS;
extern int MaxS;
extern int MinV;
extern int MaxV;
extern int MinArea;
extern int nBlobs;


#ifdef BGR
#define RED(p)		((p)[2])
#define GREEN(p)	((p)[1])
#define BLUE(p)		((p)[0])
#else
#define RED(p)		((p)[0])
#define GREEN(p)	((p)[1])
#define BLUE(p)		((p)[2])
#endif

#define NAN	1000

typedef struct {
 double r;       // percent
 double g;       // percent
 double b;       // percent
} rgb;

 typedef struct fhsv {
 double h;       // angle in degrees
 double s;       // percent
 double v;       // percent
} hsv;


hsv      rgb2hsv(rgb in);
rgb      hsv2rgb(hsv in);

enum BlobPosition
{
	BlobPosition_First,
	BlobPosition_Left,
	BlobPosition_Right,
};

int ProcessImage(
unsigned char	*	pBits,			// Pointer to the frame buffer data.  For this particular frame
									// buffer, the data is organized using 3 bytes per pixel arranged as BGR
int					width,			// Width of the image in pixels
int					height,			// Height of the image in pixels
int					stride,			// The number of bytes you must advance to get from one row to the next 
RECT			*	pBounds,		// Returns the bounds of the rectangle
#ifdef FIND_CORNERS
POINT			*	pCorners,		// Array of 2 (upper left, upper right)
#endif	// FIND_CORNERS
bool				modify,			// If true, black out matching pixels and draw bounding box
BlobPosition		pos = BlobPosition_First);

bool CreateFilterTable(bool force = false);
void SaveFilterTable(int frame);
void CheckFilterTable(const char * pTag);


extern int ImageLinePos;
extern int ImageCenterPos;


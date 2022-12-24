// ImageProcessing.cpp : Defines the entry point for the application.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ProcessImage.h"

#define IdentifyChevronsx
#define FindTopLeftRight

#define TIMERx
#ifdef TIMER
extern double GetTimer();
#endif	// TIMER

/*
 * This routine converts RGB to HSV.  Note, however, that it makes
 *	heavy use of floating point which is probaby something you
 *	are probabably going to want to avoid.
 */
hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if( max > 0.0 ) {
        out.s = (delta / max);                  // s
    } else {
        // r = g = b = 0                        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}

/*
 * These macros extract the component colors from the pixel data.
 *	Note that by using macros like this, you can easily adjust
 *	for any change in the pixel order of your frame buffer
 */


#define EdgeThreshold	100

struct RGB
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct HSV
{
	unsigned char h;
	unsigned char s;
	unsigned char v;
};


unsigned char * pFilterTable = 0;

bool CreateFilterTable(HSVFilters * pFilters)
{
	int	r;
	int	g;
	int	b;

	printf("Create Filter Table...");

	if (!pFilterTable)
	{
		if (!(pFilterTable = new unsigned char[32 * 32 * 32]))
		{
			return(false);
		}
	}

	memset(pFilterTable, 0, 32 * 32 * 32);

	for (r = 0 ; r < 32 ; r++)
	{
		for (g = 0 ; g < 32 ; g++)
		{
			for (b = 0 ; b < 32 ; b++)
			{
				rgb	rgb;
				hsv	hsv;

				/*
				 * Retrieve the pixel and convert to a floating point value from 0.0 to 1.0
				 */
				rgb.r	= ((double) r) / 31;
				rgb.g	= ((double) g) / 31;
				rgb.b	= ((double) b) / 31;

				/*
				 * Convert to HSV
				 */
				hsv		= rgb2hsv(rgb);

				if (hsv.h == NAN)
				{
					hsv.h	= 0;
				}

				int hue	= (hsv.h * 255) / 360;
				int sat	= (hsv.s * 255);
				int val	= (hsv.v * 255);

				int	bit	= 1;

				for (int f = 0 ; f < DIM(pFilters->m_filters) ; f++)
				{
					HSVFilter	*	pFilter	= pFilters->m_filters + f;

					if (pFilter->m_maxV)
					{
						if ( ((pFilter->m_minH > pFilter->m_maxH) ? ((hue >= pFilter->m_minH) || (hue <= pFilter->m_maxH)) : ((hue >= pFilter->m_minH) && (hue <= pFilter->m_maxH))) &&
							(sat >= pFilter->m_minS) && (sat <= pFilter->m_maxS)	&&
							(val >= pFilter->m_minV) && (val <= pFilter->m_maxV))
						{
							unsigned long index	= r + (g << 5) + (b << 10);

							pFilterTable[index]	|= bit;
						}
					}

					bit <<= 1;
				}
			}
		}
	}

	printf("Done\n");

	return(true);
}


unsigned char  * pFilterBuffer = 0;			// Pointer to the current filtered image
int				 filterBufferSize = 0;		// Size of the current filtered image

/*
 * Converts an RGB image to a byte image where the lower 4 bits
 *	of each byte specify the presence of one of the 4 matched colors
 *
 */
unsigned char * FilterImage(
unsigned char	*	pBits,			// Pointer to the frame buffer data.  For this particular frame
									// buffer, the data is organized using 3 bytes per pixel arranged as BGR
int					width,			// Width of the image in pixels
int					height,			// Height of the image in pixels
int					stride,			// The number of bytes you must advance to get from one row to the next 
bool				modify)			// If true, modify matching pixels
{
	int					x;
	int					y;
	int					size	= width * height;

	if (size > filterBufferSize)
	{
		delete pFilterBuffer;

		pFilterBuffer = new unsigned char[size];
	}

	unsigned char	*	pDst	= pFilterBuffer;

	for (y = 0 ; y < height ; y++, pBits += stride)
	{
		unsigned char * p	= pBits;

		for (x = 0 ; x < width ; x++, p += 3, pDst++)
		{
			unsigned long index	= (RED(p) >> 3) | ((GREEN(p) & 0xf8) << 2) | ((BLUE(p) & 0xf8) << 7);

			/*
			 * Use table lookup for color match
			 */
			if (pFilterTable[index])
			{
				/*
				 * Color Matches
				 */
				*pDst	= pFilterTable[index];

				if (modify)
				{
					/*
					 * For testing purposes, set the pixel to black
					 */
					RED(p)		= 128;
					GREEN(p)	= 128;
					BLUE(p)		= 255;
				}
			}
			else
			{
				*pDst	= 0;
			}
		}
	}

	return(pFilterBuffer);
}

/*
 * Returns the pixel that is located at the position (x, y)
 */
int GetPixel(int x, int y, int width, int height, unsigned char * pImage)
{
	if ((x < 0) || (x >= width) || (y < 0) || (y >= height))
	{
		return(0);
	}

	return(pImage[x + (y * width)]);
}

#define PixelMask	0x37
#define PixelLeft	0x70
#define PixelRight	0x80

/*
 * Marks the specified pixel as the left edge of a traced blob
 */
void MarkLeft(int x, int y, int width, int height, unsigned char * pImage)
{
	pImage[x + (y * width)]	|= PixelLeft;
}

/*
 * Marks the specified pixel as the right edge of a traced blob
 */
void MarkRight(int x, int y, int width, int height, unsigned char * pImage)
{
	pImage[x + (y * width)]	|= PixelRight;
}

/*
 * Specifies the current direction of the edge trace
 */
enum Direction
{
	Right,
	Down,
	Left,
	Up,
	DirectionCount,
};

/*
 * Returns the position of the pixel that you would get if you moved in the specified direction
 */
void Move(Direction dir, int x, int y, int * px, int * py)
{
	switch (dir)
	{
	case Right:
		*px	= x + 1;
		*py = y;
		break;

	case Down:
		*px = x;
		*py = y + 1;
		break;

	case Left:
		*px = x - 1;
		*py	= y;
		break;

	case Up:
		*px	= x;
		*py	= y - 1;
		break;

	default:
		break;
	}
}

/****
 * 
 * Traces the outline of a blob using a maze solving algorithim called the 'left hand rule'.
 *	Basically you can find your way out of any simply-connected maze by placing your left
 *	hand on the wall and continue walking without letting go of the wall.
 *
 *	From a program point of view we define this rule as:
 *	'Turn left if you can, right if you must'
 *
 */
void TraceBlob(	unsigned char * pImage,		// Pointer to the image buffer which consists of an array of pixels whose bits represent the 4 matched colors
			    int				width,		// Width of the image in pixels
				int				height,		// Height of the image in pixels
				int				xStart,		// Horizontal coordinate of the starting pixel
				int				yStart,		// Vertical coordiante of the starting pixel
				ImageRect	 *	pBounds,	// Pointer to a rectangle which returns the bounding rectangle for the blob
				unsigned char	pixel)		// One of the lower 4 bits specifies which color is to be matched
{
	int	x				= xStart;
	int	y				= yStart;
	Direction dir		= Right;

	pBounds->left	=
	pBounds->right  = x;
	pBounds->top	=
	pBounds->bottom = y;

	MarkLeft(x, y, width, height, pImage);

	do
	{
		int nx = 0;
		int	ny = 0;
		Direction nDir;

		/*
		 * Try and turn left (left if you can)
		 */
		Move(nDir = (Direction) ((dir + DirectionCount - 1) % DirectionCount), x, y, &nx, &ny);
		if (!(GetPixel(nx, ny, width, height, pImage) & pixel))
		{
			/*
			 * Try and move straight
			 */
			Move(nDir = dir, x, y, &nx, &ny);
			if (!(GetPixel(nx, ny, width, height, pImage) & pixel))
			{
				/*
				 * Turn Right (right if you must)
				 */
				nDir	= dir;
				
				do
				{
					Move(nDir = (Direction) ((nDir + 1) % DirectionCount), x, y, &nx, &ny);
					if (GetPixel(nx, ny, width, height, pImage) & pixel)
					{
						break;
					}
					nx	= x;
					ny	= y;
				} while (nDir != dir);
			}
		}

		/*
		 * Since we will be attempting to find multiple blobs, we need
		 *	to mark the edges of this blob so that we only trace it once
		 */
		if ((nDir == Down) || (nDir == Left))
		{
			if (!(GetPixel(x + 1, y, width, height, pImage) & pixel))
			{
				MarkRight(x, y, width, height, pImage);	// Mark pixel as the right edge of the blob
			}
		}
		else
		{
			if (!(GetPixel(x - 1, y, width, height, pImage) & pixel))
			{
				MarkLeft(x, y, width, height, pImage);	// Mark pixel as the left edge of the blob
			}

			if (nDir == Up)
			{
				if (!(GetPixel(nx - 1, ny, width, height, pImage) & pixel))
				{
					MarkLeft(nx, ny, width, height, pImage);	// Mark pixel as the left edge of the blob
				}
			}
		}

		x	= nx;
		y	= ny;
		dir = nDir;

		/*
		 * Update the bounds, if necessary
		 */
		if (x < pBounds->left)
		{
			pBounds->left	= x;
		}
		else if (x > pBounds->right)
		{
			pBounds->right	= x;
		}

		if (y < pBounds->top)
		{
			pBounds->top	= y;
		}
		else if (y > pBounds->bottom)
		{
			pBounds->bottom	= y;
		}
	}
	while ((x != xStart) || (y != yStart));  //|| (dir != startDir));

	/*
	 * We want the right and bottom edges of the bounding rectangle to
	 *	NOT include the pixels of the blob
	 */
	pBounds->right++;
	pBounds->bottom++;
}

/*
 * Find the top left edge of the blob
 */
int FindTopLeft(unsigned char * pFilter, int width, int height, ImageRect& bounds)
{
	int	y;
	int	x;
	int	xMax	= bounds.left + (bounds.right - bounds.left) / 4;

	for (y = bounds.top ; y < bounds.bottom ; y++)
	{
		for (x = bounds.left ; x < xMax ; x++)
		{
			if (GetPixel(x, y, width, height, pFilter))
			{
				goto found;
			}
		}
	}
found:

	return(y);
}

/*
 * Find the top right edge of the blob
 */
int FindTopRight(unsigned char * pFilter, int width, int height, ImageRect& bounds)
{
	int	y;
	int	x;
	int	xMin	= bounds.right - (bounds.right - bounds.left) / 4;

	for (y = bounds.top ; y < bounds.bottom ; y++)
	{
		for (x = xMin ; x < bounds.right ; x++)
		{
			if (GetPixel(x, y, width, height, pFilter))
			{
				goto found;
			}
		}
	}
found:

	return(y);
}

#ifdef IdentifyChevrons
int FindTopLeftPixelCount(unsigned char * pFilter, int width, int height, ImageRect& bounds, int mask)
{
	int xMax = (bounds.right - bounds.left) / 3;
	int yMax = (bounds.bottom - bounds.top) / 4;

	int on = 0;
	int off	= 0;

	if (xMax < 2)
	{
		xMax = 2;
	}
	xMax += bounds.left;

	if (yMax < 4)
	{
		yMax = 4;
	}
	yMax += bounds.top;

	for (int y = bounds.top ; y < yMax ; y++)
	{
		for (int x = bounds.left ; x < xMax ; x++)
		{
			if (GetPixel(x, y, width, height, pFilter) & mask)
			{
				on++;
			}
			else
			{
				off++;
			}
		}
	}

	if (off == 0)
	{
		off = 1;
	}

//	return (1000*on) / off;
	return(on);
}

int FindPixelCount(unsigned char * pFilter, int width, int height, ImageRect& bounds, int mask)
{
	int on = 0;

	for (int y = bounds.top ; y < bounds.bottom ; y++)
	{
		for (int x = bounds.left ; x < bounds.right ; x++)
		{
			if (GetPixel(x, y, width, height, pFilter) & mask)
			{
				on++;
			}
		}
	}

	return(on);
}

int FindTopRightPixelCount(unsigned char * pFilter, int width, int height, ImageRect& bounds, int mask)
{
	int xMin = (bounds.right - bounds.left) / 3;
	int yMax = (bounds.bottom - bounds.top) / 4;

	int on = 0;
	int off	= 0;

	if (xMin < 2)
	{
		xMin = 2;
	}
	xMin = bounds.left - xMin;

	if (yMax < 4)
	{
		yMax = 4;
	}
	yMax += bounds.top;

	for (int y = bounds.top ; y < yMax ; y++)
	{
		for (int x = xMin ; x < bounds.right ; x++)
		{
			if (GetPixel(x, y, width, height, pFilter) & mask)
			{
				on++;
			}
			else
			{
				off++;
			}
		}
	}

	if (off == 0)
	{
		off = 1;
	}

	return(1000*on)/off;
}
#endif		// IdentifyChevrons

int MinArea = 200;

/*
 * Finds all of the regions for a filtered image
 */
void FindBlobRegions(unsigned char * pFilter, int width, int height, ImageRegions * pRegions)
{
	int	y;
	unsigned char * p;
	bool inBlob;
	ImageRect	imageBounds;

	imageBounds.left	= 0;
	imageBounds.top	= 0;
	imageBounds.right	= 0;
	imageBounds.bottom	= 0;

	/*
	 * Find first pixel
	 */
	for (y = 0, p = pFilter ; y < height ; y++, p += width)
	{
		int x;
		unsigned char * pRow = p;

		inBlob	= false;

		for (x = 0 ; x < width ; x++, pRow++)
		{
			int	pixel = *pRow;

			ImageRect bounds; 

			if (pixel & PixelLeft)
			{
				inBlob	= true;
			}

			/*
			 * Only process pixels that are NOT inside a previously traced blob
			 */
			if (!inBlob)
			{
				int	pixelBit	= pixel & PixelMask;

				if (pixelBit)
				{
					int area;

					inBlob	= true;

					TraceBlob(pFilter, width, height, x, y, &bounds, pixelBit);

					area	= (bounds.right - bounds.left) * (bounds.bottom - bounds.top);

					if (area >= MinArea)
					{
						/*
						 * Blob area is sufficient, so add it to the list
						 */
						int color;
						int topLeft = 0;
						int topRight = 0;

#ifdef IdentifyChevrons
						topLeft = FindTopLeftPixelCount(pFilter, width, height, bounds, pixelBit);
						topRight = FindPixelCount(pFilter, width, height, bounds, pixelBit);
#endif	// IdentifyCheverons

#ifdef FindTopLeftRight
						if (x > ((bounds.left + bounds.right) / 2))
						{
							topRight = bounds.top;
							topLeft = FindTopLeft(pFilter, width, height, bounds);
						}
						else
						{
							topLeft = bounds.top;
							topRight = FindTopRight(pFilter, width, height, bounds);
						}
#endif	// FindTopLeftRight

						for (color = 0 ; !(pixelBit & 1) ; color++, pixelBit >>= 1);

						pRegions->AddRegion(&bounds, topLeft, topRight, area, color);
					}
				}
			}

			if (pixel & PixelRight)
			{
				inBlob	= false;
			}
		}
	}
}

/*
 * Process the specified image and return the regions found
 */
int ProcessImage(
unsigned char	*	pBits,			// Pointer to the frame buffer data.  For this particular frame
									// buffer, the data is organized using 3 bytes per pixel arranged as BGR
int					width,			// Width of the image in pixels
int					height,			// Height of the image in pixels
int					stride,			// The number of bytes you must advance to get from one row to the next 
bool				modify,			// If true, black out matching pixels and draw bounding box
ImageRegions	*	pRegions)
{
	int	area = 0;

	if (pFilterTable)
	{
#ifdef TIMER
	  double	time = GetTimer();
	  double	filterTime = 0;
	  for (int i = 0 ; i < 500 ; i++)
#endif	// TIMER
	  {
#ifdef TIMER
		double	fTime = GetTimer();
#endif	// TIMER

		unsigned char * pFilter = FilterImage(pBits, width, height, stride, modify);

#ifdef TIMER
		filterTime	+= GetTimer() - fTime;
#endif	// TIMER

		FindBlobRegions(pFilter, width, height, pRegions);
	  }

#ifdef TIMER
	  time	= GetTimer() - time;

	  char msg[512];

	  _snprintf(msg, sizeof(msg), "Time = %0.3f, filter = %0.3f\n", time, filterTime);
	  OutputDebugString(msg);
#endif	// TIMER
	}

	return(area);
}

// ImageProcessing.cpp : Defines the entry point for the application.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <windows.h>
#include <debugapi.h>
#include "ProcessImage.h"

/*
 * Returns the position of the pixel that you would get if you moved in the specified direction
 */
void Move(Direction dir, int x, int y, int* px, int* py)
{
	switch (dir)
	{
	case Right:
		*px = x + 1;
		*py = y;
		break;

	case Down:
		*px = x;
		*py = y + 1;
		break;

	case Left:
		*px = x - 1;
		*py = y;
		break;

	case Up:
		*px = x;
		*py = y - 1;
		break;

	default:
		break;
	}
}

/*
* returns true if pixel is black
*/
bool TestPixel(unsigned char* pImage, int x, int y, int width, int BlackColor)
{
	return(*(pImage + (y * width) + x) <= BlackColor);
}

unsigned short leftEdge[MaxHeight];
unsigned short rightEdge[MaxHeight];

AprilTag * checkBlob(unsigned char* pImage,		// Pointer to the image buffer which consists of an array of pixels whose bits represent the 4 matched colors
	int	width,		// Width of the image in pixels
	int	height,		// Height of the image in pixels
	int	xStart,		// Horizontal coordinate of the starting pixel
	int	yStart,		// Vertical coordiante of the starting pixel
	int	BlackColor,
	int minSize)
{
	int	x = xStart;
	int	y = yStart;
	Direction dir = Right;
	int minX = xStart;
	int maxX = xStart;
	int minY = yStart;
	int maxY = yStart;
	int leftMin = yStart;
	int leftMax = yStart;
	int rightMin = yStart;
	int rightMax = yStart;

	for (int i = 0; i < height; i++)
	{
		leftEdge[i] = width;
		rightEdge[i] = 0;
	}

	do
	{
		int nx = 0;
		int	ny = 0;
		Direction nDir;

		/*
		 * Try and turn left (left if you can)
		 */
		Move(nDir = (Direction)((dir + DirectionCount - 1) % DirectionCount), x, y, &nx, &ny);
		if (!TestPixel(pImage, nx, ny, width, BlackColor))
		{
			/*
			 * Try and move straight
			 */
			Move(nDir = dir, x, y, &nx, &ny);
			if (!TestPixel(pImage, nx, ny, width, BlackColor))
			{
				/*
				 * Turn Right (right if you must)
				 */
				nDir = dir;

				do
				{
					Move(nDir = (Direction)((nDir + 1) % DirectionCount), x, y, &nx, &ny);
					if (TestPixel(pImage, nx, ny, width, BlackColor))
					{
						break;
					}
					nx = x;
					ny = y;
				} while (nDir != dir);
			}
		}

		if (ny < (yStart - 2))
		{
			return NULL;		// Not a tag
		}

		x = nx;
		y = ny;
		dir = nDir;

		if (y == 553)
		{
			int xx = 0;
		}

		if (x < leftEdge[y])
		{
			leftEdge[y] = x;

			if (x == 791)
			{
				int xx = 0;
			}

			if (y < leftMin)
			{
				leftMin = y;
			}
			else if (y > leftMax)
			{
				leftMax = y;
			}
		}
		
		if (x > rightEdge[y])
		{
			rightEdge[y] = x;

			if (y < rightMin)
			{
				rightMin = y;
			}
			else if (y > rightMax)
			{
				rightMax = y;
			}
		}

		if (x < minX)
		{
			minX = x;
		}
		else if (x > maxX)
		{
			maxX = x;
		}

		if (y < minY)
		{
			minY = y;
		}
		else if (y > maxY)
		{
			maxY = y;
		}
	} while ((x != xStart) || (y != yStart));  //|| (dir != startDir));

	if (((maxX - minX + 1) < minSize) || ((maxY - minY + 1) < minSize))
	{
		return NULL;		// Tag too small
	}

	char msgbuf[128];
	sprintf(msgbuf, "(%d,%d),(%d,%d)\n", minX + 200, minY + 43, maxX + 200, maxY + 43);
	OutputDebugString(msgbuf);

	return new AprilTag(minX, minY, maxX, maxY, leftEdge, leftMin, leftMax, rightEdge, rightMin, rightMax, width, height);
}

void deleteAprilTags(std::list<AprilTag*>* pList)
{
	for (AprilTag* pTest : *pList)
	{
		delete pTest;
	}

	delete pList;
}

std::list<AprilTag *> * findAprilTags(unsigned char * pImage, int width, int height, int BlackColor, int MinWhite, int MinBlack, int minSize)
{
	int			x;
	int			y;
	int			white = 0;
	std::list<AprilTag *>* pList = new std::list<AprilTag *>();

	for (y = 1; y < height; y++)
	{
		unsigned char* s = pImage + (y * width);

		for (x = 0; x < width; x++, s++)
		{
			if (*s <= BlackColor)
			{
				if (s[-width] > BlackColor)
				{
					if (white >= MinWhite)
					{
						int i;

						for (i = 1; i <= MinBlack; i++)
						{
							if (s[i] > BlackColor)
							{
								break;
							}
						}

						if (i > MinBlack)
						{
							bool duplicate = false;

							for (AprilTag * pTest : *pList) {
								if ((x >= pTest->m_minX) && (x <= pTest->m_maxX) &&
									(y >= pTest->m_minY) && (y <= pTest->m_maxY))
								{
									duplicate = true;
									break;
								}
							}

							if (!duplicate)
							{
								AprilTag* pTag = checkBlob(pImage, width, height, x, y, BlackColor, minSize);

								if (pTag != NULL)
								{
									pList->push_back(pTag);
								}
							}
						}
					}
				}
				white = 0;
			}
			else
			{
				white++;
			}
		}
	}

	return(pList);
}

void deleteImageRegions(std::list<ImageRegion*>* pRegions)
{
	for (ImageRegion* pRegion : *pRegions)
	{
		delete pRegion;
	}

	delete pRegions;
}

std::list<ImageRegion*>* processImageFast(unsigned char* pImage, int width, int height, 
													int BlackColor, int MinWhite, int MinBlack, int minSize)
{
	std::list<AprilTag*> * pList = findAprilTags(pImage, width, height, BlackColor, MinWhite, MinBlack, minSize);
	std::list<ImageRegion*>* pRet = new std::list<ImageRegion*>();

	for (AprilTag* pTest : *pList)
	{
		ImageRegion* pRegion = new ImageRegion(0, pTest->m_corners);

		pRet->push_back(pRegion);
	}

	deleteAprilTags(pList);

	return(pRet);
}


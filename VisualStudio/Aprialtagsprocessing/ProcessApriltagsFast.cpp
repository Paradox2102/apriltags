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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <cmath>
#include "ProcessApriltagsFast.h"

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
bool TestPixel(unsigned char* pImage, int x, int y, int width, int height, int BlackColor)
{
	if ((x < 0) || (y < 0) || (x >= width) || (y >= height))
	{
		return false;
	}
	return(*(pImage + (y * width) + x) <= BlackColor);
}

unsigned char* marked = 0;
int markedWidth = 0;
int markedHeight = 0;

/*
 * Traces the outline of the blob using a maze solving technique know as the left hand rule
 *  The basic idea is that you can escape from any simply connected maze by placing your left
 *  hand on the left wall and start walking without letting your hand leave the wall.
 *  This will eventually lead you the the exit.
 * 
 *  The programing equivalent of this is 'turn left if you can, right if you must'.
 */
int traceBlob(unsigned char* pImage,	// Pointer to the image buffer, one byte per pixel, grey scale
                    int	width,		    // Width of the image in pixels
                    int	height,		    // Height of the image in pixels
                    int	xStart,		    // Horizontal coordinate of the starting pixel
                    int	yStart,		    // Vertical coordiante of the starting pixel
                    int blackColor,     // All values less than or equal to this value are considered black
                    int& minX,          // Return of left edge of the blob
                    int& maxX,          // Return of right edge of the blob
                    int& minY,          // Return of top edge of the blob
                    int& maxY,          // Return of the bottom edge of the blob
                    short* pPoints,     // Pointer to buffer into which the border points are returned
                    int maxPoints)      // Max number of points that can be returned
{
    int	x = xStart;
    int	y = yStart;
    Direction dir = Right;
    int count = 0;

    minX = xStart;
    maxX = xStart;
    minY = yStart;
    maxY = yStart;

    int nPoints = 0;

    do
    {
        int nx = 0;
        int	ny = 0;
        Direction nDir;

        marked[(y * width) + x] = 1;

        if (nPoints < maxPoints)
        {
            *pPoints++ = x;
            *pPoints++ = y;

            nPoints += 2;
        }

        /*
         * Try and turn left (left if you can)
         */
        Move(nDir = (Direction)((dir + DirectionCount - 1) % DirectionCount), x, y, &nx, &ny);
        if (!TestPixel(pImage, nx, ny, width, height, blackColor))
        {
            /*
             * Try and move straight
             */
            Move(nDir = dir, x, y, &nx, &ny);
            if (!TestPixel(pImage, nx, ny, width, height, blackColor))
            {
                /*
                 * Turn Right (right if you must)
                 */
                nDir = dir;

                do
                {
                    Move(nDir = (Direction)((nDir + 1) % DirectionCount), x, y, &nx, &ny);
                    if (TestPixel(pImage, nx, ny, width, height, blackColor))
                    {
                        break;
                    }
                    nx = x;
                    ny = y;
                } while (nDir != dir);
            }
        }

        x = nx;
        y = ny;
        dir = nDir;

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

        count++;
    } while ((x != xStart) || (y != yStart) || (count < 10));

    return(nPoints);
}

short tracePoints[2 * (1280 + 800)];
unsigned char * g_pImage;

/*
 * Checks the blob that is at the specified starting position to
 * see if it is valid tag.
 */
AprilTag* checkBlob(
    unsigned char* pImage,	// Pointer to the image buffer 
    int	width,		        // Width of the image in pixels
    int	height,		        // Height of the image in pixels
    int	xStart,		        // Horizontal coordinate of the starting pixel
    int	yStart,		        // Vertical coordiante of the starting pixel
    int blackColor,         // All values less than or equal to this value are considered black
    ApriltagParams* params)
{
    int minX;       // Left edge of the possible tag
    int maxX;       // Right edge of the possible tag
    int minY;       // Top edge of the possible tag
    int maxY;       // Bottom edge of the possible tag;
    int nPoints;    // # of points which specify the tag's bounds

    g_pImage = pImage;

    nPoints = traceBlob(pImage, width, height, xStart, yStart, blackColor, minX, maxX, minY, maxY,
                                            tracePoints, sizeof(tracePoints) / sizeof(*tracePoints));

    if (nPoints >= (int) (sizeof(tracePoints) / sizeof(*tracePoints)))
    {
        return NULL;        // Perimeter is too large
    }

	if (((maxX - minX + 1) < params->minSize) || ((maxY - minY + 1) < params->minSize))
	{
		return NULL;		// Tag too small
	}

	AprilTag * pTag = new AprilTag(minX, minY, maxX, maxY, blackColor, pImage, 
                                                    width, height, params, tracePoints, nPoints);

	if (!pTag->m_isValid)
	{
        // Tag was determined to not be valid

		delete pTag;

		return(NULL);
	}

	return(pTag);
}

void deleteAprilTags(std::list<AprilTag*>* pList)
{
	for (AprilTag* pTest : *pList)
	{
		delete pTest;
	}

	delete pList;
}

/*
 * Adds a tag to the list if it is not a duplicate
 */
 bool addTag(std::list<AprilTag*>* pList, AprilTag* pTag)
 {
    for (AprilTag * pNextTag : *pList)
    {
        double dx = pTag->m_corners[0][0] - pNextTag->m_corners[0][0];
        double dy = pTag->m_corners[0][1] - pNextTag->m_corners[0][1];

        if ((abs(dx) < 10) && (abs(dy) < 10))
        {
//            printf("duplicate: tag=%d, x=%0.1f/%0.1f, y=%0.1f/%0.1f\n", pTag->m_tag, pTag->m_corners[0][0], pNextTag->m_corners[0][0], pTag->m_corners[0][1], pNextTag->m_corners[0][1]);
            return(false);
        }
    }

    pList->push_back(pTag);
    return(true);
}

/*
 * Creates a list of all of the valid tags in an image
 */
std::list<AprilTag*>* findAprilTags(
        unsigned char* pImage,      // Pointer to image as a on byte per pixel grey scale
        int width,                  // Width of the image in pixels
        int height,                 // Height of the image in pixels
        ApriltagParams* params,     // Parameters which control the detection
        int * pBlobCount,           // Returns the total number of blobs that had to be traced
        int * pAvgWhite)            // Returns the average white level for the image (used for auto exposure correction)
{
	int			x;
	int			y;
    uint64_t    whiteSum = 0;       // sum of all the pixels within the exposure correction region
    int         whiteCount = 0;     // count of number of pixels within the exposure correction region
	int			blobCount = 0;      // count of total number of blobs traced (not just valid ones)
	int         whiteLeft = width/2 - (width*params->sampleRegion)/200;     // specifies the left edge of the region used for exposure correction
	int         whiteRight = width/2 + (width*params->sampleRegion)/200;    // specifies the right edge of the region used for exposure correction
	int         whiteTop = height/2 - (height*params->sampleRegion)/200;    // specifies the top edge of the region used for exposure correction
	int         whiteBottom = height/2 + (height*params->sampleRegion)/200; // specifies the bottom edge of the region used for expsure correction
	std::list<AprilTag*>* pList = new std::list<AprilTag*>();

    /*
     * Allocate space the the buffer that is used to mark which blobs have been traced
     *  This is used to prevent the re-tracing of the same blob multiple times.
     */
	if (!marked || (width != markedWidth) || (height != markedHeight))
	{
		if (marked)
		{
			free(marked);
		}

		marked = (unsigned char*)malloc(width * height);
		markedWidth = width;
		markedHeight = height;
	}

	memset(marked, 0, width * height);

#define avgCnt  3   // When searching for a black pixel, specifies the # of pixels to average for a white value
                    //   It would be nice to set this value from the params, however using anything but a constant
                    //   here significantly reduces performance

    int	avgWhite;                       // Specifies the average value for the last 'avgCnt' pixels.
    int whiteDrop = params->whiteDrop;  // Specifies the percent drop required to distinguish a black pixel from white.

	for (y = 1; y < height - 1; y += 3)
	{
		unsigned char* s = pImage + (y * width);
        unsigned char* m = marked + (y * width);

        int markCount = width;  // Counts how many unmarked pixels we have seen since the last marked pixel.

        // Compute the starting average white value
        avgWhite = 0;
        for (x = 0; x < avgCnt; x++, s++)
        {
            avgWhite += *s;
        }

        for (; x < width; x++, s++)
		{
            if (x >= whiteLeft && x <= whiteRight && y >= whiteTop && y<= whiteBottom)
            {
                // Pixel is withing the specified exposure correction region
                whiteCount++;
                whiteSum += *s;
            }

            if (m[x])
            {
                // This pixel has been previously marked as part of a blob so clear markCount
                markCount = 0;
            }

            int black = ((avgWhite * whiteDrop) / (100 * avgCnt));  // Compute possible black color
            if (*s < black)
            {
                // pixel is black

                int x0 = x;
                bool duplicate = markCount <= 12;       // If any of the last 12 pixels have been previously marked then this blob is a duplicate

                //black = (s[-avgCnt] * whiteDrop) / 100; // Recompute the black color based on the pixel that is 'avcCnt' back
#define         avgCount 6
                int b = 0;
                if (x > avgCount + 2)
                {
                    for (int i = 0; i < avgCount; i++)
                    {
                        b += s[-i - 2];
                    }
                    black = (b * whiteDrop) / (100 * avgCount);
                }

                // recheck to see if we are at the left most edge of the blob given the new black value
                while ((x > 1) && (s[-1] < black))
                {
                    s--;
                    x--;
                }
                x0 = x;     // marks the start of the black run

                // Check for min black run
                int cnt = 0;
                for (; (x < width) && (cnt < params->minBlack); x++, s++)
                {
                    if (m[x])
                    {
                        duplicate = true;   // If we pass over a previously marked pixel then mark this blob as a duplicate
                    }

                    if (*s >= black)
                    {
                        break;  // Pixel is not black
                    }
                    cnt++;
                }

                // Only mark if black run is long enough and it is not a duplicate
                if (!duplicate && (cnt >= params->minBlack))
                {
                    AprilTag* pTag = checkBlob(pImage, width, height, x0, y, black, params);

                    if (pTag != NULL)
                    {
                        if (!addTag(pList, pTag))
                        {
//                            printf("Duplicate tag: %d\n", pTag->m_tag);
                            //SaveImage("dup", pImage);
                        }
                    }
                }
                    
                blobCount++;

                // Compute the new starting white average for the next run
                avgWhite = 0;
                for (int i = 0; (i < avgCnt) && (x < width); i++, x++, s++)
                {
                    avgWhite += *s;
                }
                x--;
                s--;
                //markCount = width;
                markCount = 0;
            }
            else
            {
                avgWhite += *s - s[-avgCnt];    // Add the current pixel and subtract the 'avgCnt' previous pixel to keep a running average
                markCount++;
            }
		}
	}

    *pBlobCount = blobCount;
    *pAvgWhite = (int) ((double) whiteSum / (double) whiteCount);

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

/*
 * Find all of the valid Apriltags for the specified image
 */
std::list<ImageRegion*>* processApriltagsFast(
        unsigned char* pImage,  // Pointer to the image as a one byte per pixel gery scale
        int width,              // Width of the image in pixels
        int height,             // Height of the image in pixels
        ApriltagParams* params, // Parameters which control the detection
        int * pBlobCount,       // Return of number of blobs traced
        int * pAvgWhite)        // Retrun of the average white within the exposure correction region.
{
	std::list<AprilTag*> * pList = findAprilTags(pImage, width, height, params, pBlobCount, pAvgWhite);
	std::list<ImageRegion*>* pRet = new std::list<ImageRegion*>();

	for (AprilTag* pTest : *pList)
	{
		ImageRegion* pRegion = new ImageRegion(pTest->m_tag, pTest->m_corners);

		pRet->push_back(pRegion);
	}

	deleteAprilTags(pList);

	return(pRet);
}

#ifdef SmallTag
// List of valid tags along with their 16 bit values
AprilTagData tagData[] =
{
        {   8, 0x01a6 },
        {  11, 0x0f54 },
        {  24, 0x1623 },
        {   7, 0x18ba },
        {  20, 0x1ec8 },
        {  22, 0x20bd },
        {  27, 0x2acf },
        {   6, 0x4ca7 },
        {  29, 0x50d1 },
        {  15, 0x50ef },
        {  26, 0x52a0 },
        {  18, 0x5afc },
        {  17, 0x6c4b },
        {  14, 0x738d },
        {   5, 0x8094 },
        {   4, 0x8659 },
        {  25, 0x8c52 },
        {  21, 0xa86a },
        {  13, 0xb8c9 },
        {  19, 0xb970 },
        {   3, 0xba46 },
        {  10, 0xc7f4 },
        {   2, 0xcb95 },
        {   1, 0xd15a },
        {   0, 0xdce4 },
        {  23, 0xe3e2 },
        {   9, 0xea92 },
        {  12, 0xf27b },
        {  16, 0xf6c3 },
        {  28, 0xf835 },
};
#else   // !SmallTag
AprilTagData tagData[] =
{
        { 479, 0x131f1a66LLU },
        { 112, 0x274e02e9LLU },
        { 385, 0x2b6d1f8eLLU },
        { 269, 0x2c94b153LLU },
        { 328, 0x47a8d8d9LLU },
        { 301, 0x4a8232eeLLU },
        {  45, 0x52d0190dLLU },
        { 183, 0x57be434aLLU },
        { 225, 0x62456919LLU },
        { 337, 0x6bb437daLLU },
        { 235, 0x7258e54fLLU },
        { 445, 0x78676730LLU },
        { 340, 0x8625b34bLLU },
        { 424, 0x9a384f07LLU },
        { 323, 0x9c4c1ed6LLU },
        { 449, 0xa426c326LLU },
        { 428, 0xb7fd84a4LLU },
        {   5, 0xce254c53LLU },
        { 256, 0xd0093ee9LLU },
        { 247, 0xd6de92cfLLU },
        { 270, 0xe2089bd4LLU },
        { 209, 0xf59e8c5aLLU },
        { 519, 0xfa528b21LLU },
        { 455, 0x1172612beLLU },
        { 327, 0x11aa06491LLU },
        { 198, 0x11abb9d22LLU },
        { 355, 0x125c87f5bLLU },
        {  82, 0x12a66b8edLLU },
        {  73, 0x1309de0a9LLU },
        { 495, 0x13211bd77LLU },
        { 436, 0x13764fa31LLU },
        { 409, 0x13d9aad88LLU },
        {   4, 0x1434357ddLLU },
        { 348, 0x1487e72d3LLU },
        { 370, 0x1504b51a6LLU },
        { 576, 0x153087458LLU },
        { 243, 0x15b79dc84LLU },
        {  93, 0x15ba30318LLU },
        {  86, 0x161da2ad4LLU },
        {  79, 0x168115290LLU },
        { 383, 0x168658a17LLU },
        { 554, 0x174ebe7c8LLU },
        { 586, 0x17c41b48cLLU },
        { 338, 0x17cd807f6LLU },
        { 292, 0x185b47522LLU },
        { 177, 0x18974a350LLU },
        { 453, 0x18dfccab8LLU },
        { 204, 0x1917e616bLLU },
        { 529, 0x19e56b8a2LLU },
        { 499, 0x1a3bdbd3cLLU },
        { 438, 0x1a5792d00LLU },
        { 251, 0x1ab0a88bcLLU },
        {  44, 0x1b22a3babLLU },
        {  30, 0x1b545cf89LLU },
        {   3, 0x1b8616367LLU },
        { 238, 0x1bd19c1e5LLU },
        { 539, 0x1c1393357LLU },
        { 201, 0x1c96ec707LLU },
        { 380, 0x1e3ff53a3LLU },
        { 111, 0x1fbc63111LLU },
        { 546, 0x20c7835e4LLU },
        { 382, 0x2199b09ddLLU },
        {   2, 0x22d7f6ef1LLU },
        { 134, 0x232bcaf43LLU },
        { 107, 0x23970cab4LLU },
        { 315, 0x24014d2c6LLU },
        { 414, 0x24e0d735cLLU },
        { 578, 0x25722065dLLU },
        { 230, 0x25cb356feLLU },
        { 325, 0x25d31938bLLU },
        {  43, 0x261d74cfaLLU },
        {   1, 0x2680e74b6LLU },
        { 114, 0x26dc8a8bdLLU },
        { 133, 0x27cd5985eLLU },
        {  67, 0x293139725LLU },
        { 516, 0x2935771f0LLU },
        { 335, 0x298d093fdLLU },
        { 486, 0x299050abfLLU },
        {  29, 0x29f81e69dLLU },
        { 207, 0x29f8e8bc8LLU },
        { 217, 0x2a1de15dcLLU },
        {   0, 0x2a29d7a7bLLU },
        { 500, 0x2a4d3ce51LLU },
        { 110, 0x2ab734260LLU },
        { 363, 0x2afd5401fLLU },
        { 170, 0x2bba7f70bLLU },
        { 265, 0x2c2b496a9LLU },
        { 232, 0x2c60de465LLU },
        { 456, 0x2d06e728dLLU },
        {  42, 0x2d6f55884LLU },
        {  28, 0x2da10ec62LLU },
        { 404, 0x2eae6200eLLU },
        { 343, 0x2f94eeb59LLU },
        { 344, 0x2f96835afLLU },
        { 488, 0x302b657b7LLU },
        {  55, 0x30e68ca6bLLU },
        { 450, 0x315b3d409LLU },
        { 213, 0x31c8877cdLLU },
        { 312, 0x31db7f3d5LLU },
        { 306, 0x32639ee99LLU },
        { 406, 0x32c35af4eLLU },
        { 378, 0x32c7c3999LLU },
        { 357, 0x32da8123aLLU },
        { 347, 0x33ed22e64LLU },
        { 561, 0x34e048743LLU },
        { 115, 0x354e92bf3LLU },
        { 330, 0x35564316dLLU },
        { 310, 0x37e514ca1LLU },
        { 433, 0x388368da5LLU },
        { 221, 0x39fe7692eLLU },
        { 163, 0x3a97c95b2LLU },
        { 460, 0x3b0f30b39LLU },
        { 497, 0x3bd097893LLU },
        {  54, 0x3be15dbbaLLU },
        { 105, 0x3d67586f5LLU },
        { 236, 0x3da062069LLU },
        { 484, 0x3e2de4628LLU },
        { 462, 0x3e3507339LLU },
        {  85, 0x3e5ff6a4bLLU },
        { 509, 0x41591c1fbLLU },
        { 261, 0x4222ff1bcLLU },
        { 155, 0x4223328cfLLU },
        {  41, 0x4364f7b22LLU },
        { 346, 0x439e1264dLLU },
        { 399, 0x4488f1dd1LLU },
        { 437, 0x4497d2b36LLU },
        { 143, 0x453ec66afLLU },
        { 252, 0x4630ab287LLU },
        { 139, 0x463764a05LLU },
        {  66, 0x4678bc54dLLU },
        {  65, 0x46aa7592bLLU },
        { 320, 0x46db790b6LLU },
        {  27, 0x473fa14c5LLU },
        { 255, 0x479768eeeLLU },
        { 239, 0x487bf62e4LLU },
        { 407, 0x48c47486cLLU },
        { 162, 0x493b8acc6LLU },
        { 352, 0x496b719d8LLU },
        { 533, 0x49bf08c3dLLU },
        { 194, 0x4abc941ecLLU },
        { 548, 0x4b4096988LLU },
        { 374, 0x4c438c777LLU },
        { 329, 0x4d3d21ab7LLU },
        { 160, 0x4dab60203LLU },
        { 205, 0x4e0cbf14aLLU },
        {  26, 0x4e918204fLLU },
        { 118, 0x4ebb6c078LLU },
        { 431, 0x4ed383f3bLLU },
        { 244, 0x4f3697a5fLLU },
        { 193, 0x4f5e22b07LLU },
        { 423, 0x4fb233671LLU },
        { 388, 0x507f42925LLU },
        { 228, 0x5093f18bbLLU },
        { 168, 0x511ac8035LLU },
        { 331, 0x516e7d24cLLU },
        { 149, 0x518dd8f5bLLU },
        {  64, 0x51a546a7aLLU },
        { 504, 0x51c2bb4d5LLU },
        { 549, 0x51de79712LLU },
        {  40, 0x5208b9236LLU },
        {  25, 0x523a72614LLU },
        { 319, 0x52533dd55LLU },
        { 117, 0x52645c63dLLU },
        { 212, 0x52eab3f98LLU },
        { 104, 0x535cfa993LLU },
        { 555, 0x53961f5c2LLU },
        { 496, 0x5431a0e2fLLU },
        { 569, 0x54c24bbc5LLU },
        { 444, 0x550e5d3b9LLU },
        { 545, 0x557e43dcaLLU },
        { 294, 0x55e6a407eLLU },
        { 492, 0x5661d6177LLU },
        { 470, 0x56768c6c3LLU },
        {  92, 0x579b16af2LLU },
        { 420, 0x57c5de9aeLLU },
        {  88, 0x57cccfed0LLU },
        { 463, 0x580a47ac2LLU },
        { 466, 0x581478d61LLU },
        { 277, 0x588b33561LLU },
        {  72, 0x5893b4e48LLU },
        {  63, 0x58f727604LLU },
        { 518, 0x59662a3e4LLU },
        { 402, 0x5a02d84c4LLU },
        { 144, 0x5a6d839d5LLU },
        { 527, 0x5b28cc758LLU },
        { 412, 0x5b4fe1496LLU },
        { 154, 0x5bf37e510LLU },
        {  24, 0x5d3543763LLU },
        { 413, 0x5e75eecb8LLU },
        { 551, 0x5f5615eb3LLU },
        { 291, 0x5f9929f0fLLU },
        { 125, 0x5fddc661dLLU },
        { 377, 0x5fe775946LLU },
        { 218, 0x600b58911LLU },
        {  53, 0x607ac156cLLU },
        { 505, 0x60a3a2ef6LLU },
        {  39, 0x60ac7a94aLLU },
        {  23, 0x60de33d28LLU },
        { 287, 0x6126298d4LLU },
        { 556, 0x6127c5e1eLLU },
        { 219, 0x62267efe6LLU },
        { 132, 0x625c5f4aeLLU },
        { 464, 0x62ac97e74LLU },
        { 200, 0x6384a336dLLU },
        { 579, 0x6556df2d4LLU },
        { 400, 0x657dd4293LLU },
        { 103, 0x65a9ac66cLLU },
        { 295, 0x65c247dbbLLU },
        {  97, 0x660d1ee28LLU },
        { 273, 0x66d073aceLLU },
        { 284, 0x67329cf40LLU },
        { 299, 0x68188b679LLU },
        { 531, 0x6851d9f44LLU },
        { 368, 0x68631dc2eLLU },
        { 222, 0x68cbc64b7LLU },
        { 258, 0x68fdcafa1LLU },
        { 174, 0x6923c15f3LLU },
        { 288, 0x69322ea0cLLU },
        { 102, 0x69529cc31LLU },
        { 157, 0x696ce84f0LLU },
        { 128, 0x6a436bbd2LLU },
        { 432, 0x6a602627dLLU },
        { 341, 0x6a907f664LLU },
        { 451, 0x6aaf418f9LLU },
        { 122, 0x6b0a50b4aLLU },
        { 566, 0x6c566a041LLU },
        { 489, 0x6ced2beccLLU },
        { 480, 0x6cf60c8fcLLU },
        { 468, 0x6d616d759LLU },
        { 275, 0x6d7d8a354LLU },
        { 373, 0x6edcc7845LLU },
        {  62, 0x6eecc98a2LLU },
        { 366, 0x6f3c19d59LLU },
        { 148, 0x6fcdfa0d9LLU },
        { 386, 0x7044caee7LLU },
        { 272, 0x70ea143e6LLU },
        { 552, 0x7111d6856LLU },
        { 227, 0x715a7ac7fLLU },
        { 130, 0x71639337eLLU },
        { 405, 0x718111ca6LLU },
        { 248, 0x72529a1b6LLU },
        {  61, 0x7295b9e67LLU },
        {  52, 0x72c773245LLU },
        {  38, 0x72f92c623LLU },
        {  22, 0x732ae5a01LLU },
        { 491, 0x736141085LLU },
        { 106, 0x741bb49a2LLU },
        { 446, 0x7441a1d08LLU },
        { 285, 0x7462e0023LLU },
        { 309, 0x749c01990LLU },
        { 506, 0x7526ce928LLU },
        { 567, 0x75be8d721LLU },
        { 233, 0x75ef48bd0LLU },
        { 350, 0x76212b15eLLU },
        {  60, 0x763eaa42cLLU },
        { 415, 0x78813c1daLLU },
        {  91, 0x788b89edfLLU },
        { 229, 0x78a48c454LLU },
        { 311, 0x7901f42a3LLU },
        { 354, 0x79c6d6b5dLLU },
        {  51, 0x7a1953dcfLLU },
        {  21, 0x7a7cc658bLLU },
        { 440, 0x7aa922309LLU },
        { 164, 0x7bb1cae14LLU },
        { 226, 0x7c5d1b183LLU },
        { 435, 0x7c6717f8dLLU },
        { 190, 0x7d10b96c6LLU },
        { 151, 0x7d15aacdbLLU },
        { 293, 0x7d9b9b810LLU },
        { 257, 0x7dd8b4275LLU },
        { 458, 0x7ded9c909LLU },
        { 550, 0x7e1e2d262LLU },
        {  20, 0x7e25b6b50LLU },
        { 191, 0x7f2bdfd9bLLU },
        { 481, 0x7f89b2e22LLU },
        { 314, 0x7fb7255eaLLU },
        { 167, 0x80aefcb36LLU },
        { 584, 0x80dc1fdd1LLU },
        { 421, 0x80f70604aLLU },
        { 498, 0x812bfd15fLLU },
        { 367, 0x81614098aLLU },
        { 184, 0x81ba17396LLU },
        {  19, 0x81cea7115LLU },
        { 565, 0x8293f42dcLLU },
        { 371, 0x835801d42LLU },
        { 384, 0x8394b3793LLU },
        {  84, 0x83e9cd7eaLLU },
        { 127, 0x844570bf1LLU },
        { 195, 0x8484b4ec4LLU },
        { 342, 0x84a0d9aa3LLU },
        { 266, 0x851c8b0ebLLU },
        { 419, 0x851f86d56LLU },
        { 398, 0x85c86e587LLU },
        { 240, 0x86ddd6522LLU },
        { 457, 0x8723c602cLLU },
        { 410, 0x87a5d3e4cLLU },
        {  78, 0x87f63056bLLU },
        { 121, 0x8851d3972LLU },
        {  50, 0x88bd154e3LLU },
        { 339, 0x8958c02a6LLU },
        { 113, 0x89ade4484LLU },
        { 411, 0x8a5f867a5LLU },
        { 542, 0x8ae98cd41LLU },
        { 332, 0x8b491a42eLLU },
        {  81, 0x8b6d67752LLU },
        { 188, 0x8b8a90db1LLU },
        { 459, 0x8baf58d58LLU },
        { 186, 0x8bbc4a18fLLU },
        {  59, 0x8c344c6caLLU },
        { 116, 0x8cf36228dLLU },
        { 522, 0x8d0e75d21LLU },
        { 181, 0x8d4a1407fLLU },
        { 286, 0x8d4d09e18LLU },
        { 263, 0x8d6bfb5e0LLU },
        { 389, 0x8f1f4dadeLLU },
        {  37, 0x9040af44bLLU },
        {  96, 0x91f863364LLU },
        { 511, 0x921128bcaLLU },
        { 290, 0x9356208c5LLU },
        {  58, 0x93862d254LLU },
        {  36, 0x93e99fa10LLU },
        {  18, 0x941b58deeLLU },
        { 214, 0x94f58479bLLU },
        { 535, 0x95ddfd21eLLU },
        { 276, 0x963d9c346LLU },
        { 140, 0x968a53515LLU },
        { 322, 0x96e119bdfLLU },
        { 448, 0x96f096c88LLU },
        { 401, 0x975b4e012LLU },
        {  35, 0x97928ffd5LLU },
        {  17, 0x97c4493b3LLU },
        { 158, 0x98087ec9bLLU },
        { 441, 0x98a7b491dLLU },
        { 101, 0x99188ab10LLU },
        { 425, 0x9a2250965LLU },
        {  71, 0x9a749b622LLU },
        { 336, 0x9abeb931bLLU },
        { 185, 0x9ac37e05fLLU },
        { 264, 0x9acdf7aa1LLU },
        {  34, 0x9b3b8059aLLU },
        { 289, 0x9c993dafbLLU },
        { 274, 0x9d215d5bfLLU },
        { 141, 0x9d4708505LLU },
        { 211, 0x9d47d2a30LLU },
        { 180, 0x9d7b9f683LLU },
        { 280, 0x9db4758e4LLU },
        { 124, 0x9e15bc832LLU },
        { 159, 0x9ef6ed069LLU },
        { 136, 0x9f3844bb1LLU },
        { 234, 0xa076ad245LLU },
        { 503, 0xa0f0a7f71LLU },
        { 557, 0xa124aa134LLU },
        {  83, 0xa13150612LLU },
        { 471, 0xa147e2c81LLU },
        { 169, 0xa16db6b45LLU },
        {  77, 0xa194c2dceLLU },
        { 202, 0xa2a59916eLLU },
        { 145, 0xa36e9195bLLU },
        {  80, 0xa50bf9fb5LLU },
        { 241, 0xa53073baaLLU },
        { 537, 0xa55606791LLU },
        {  16, 0xa6680aac7LLU },
        { 475, 0xa6d993349LLU },
        { 260, 0xa7a3e0ac7LLU },
        { 536, 0xa82c1da0dLLU },
        { 408, 0xa8535e3bcLLU },
        { 224, 0xa857fde29LLU },
        { 521, 0xa8da1f81eLLU },
        { 316, 0xa98367a73LLU },
        {  33, 0xa9df41caeLLU },
        { 271, 0xaa24d88d9LLU },
        { 262, 0xaa6f48b20LLU },
        { 513, 0xaa7a80a1dLLU },
        { 540, 0xab16a1e51LLU },
        { 304, 0xab1a32069LLU },
        { 308, 0xabfcf72f6LLU },
        { 564, 0xac11c770eLLU },
        { 365, 0xac28e6f10LLU },
        {  76, 0xac8f93f1dLLU },
        { 465, 0xaca1fde4fLLU },
        { 573, 0xad007ca78LLU },
        { 259, 0xad2b9122eLLU },
        { 119, 0xad4ea9ae0LLU },
        {  15, 0xadb9eb651LLU },
        { 321, 0xae053a69bLLU },
        { 334, 0xae097977eLLU },
        { 100, 0xaf0e2cdaeLLU },
        { 559, 0xaf2043940LLU },
        {  90, 0xaf719f56aLLU },
        { 216, 0xafa700c13LLU },
        {  70, 0xb06a3d8c0LLU },
        { 560, 0xb0a7caf75LLU },
        {  14, 0xb162dbc16LLU },
        { 494, 0xb18b70b04LLU },
        { 109, 0xb221f17d9LLU },
        { 520, 0xb30a04a92LLU },
        {  89, 0xb31a8fb2fLLU },
        { 215, 0xb3897996bLLU },
        { 305, 0xb3ea3e379LLU },
        { 303, 0xb405d2f82LLU },
        {  69, 0xb4132de85LLU },
        { 137, 0xb4ca74693LLU },
        { 175, 0xb4d5217e8LLU },
        { 135, 0xb55fa022dLLU },
        { 253, 0xb5ced5775LLU },
        {  99, 0xb6600d938LLU },
        { 307, 0xb66c7f4d5LLU },
        { 469, 0xb71751033LLU },
        { 434, 0xb75dd2b9dLLU },
        { 376, 0xb77769b55LLU },
        { 203, 0xb7a29d0b6LLU },
        { 524, 0xb7acbdbc3LLU },
        { 571, 0xb84911129LLU },
        {  13, 0xb8b4bc7a0LLU },
        { 553, 0xb8bf3faa6LLU },
        { 165, 0xb9b807c4bLLU },
        { 534, 0xba5e53639LLU },
        { 485, 0xba811bac1LLU },
        { 430, 0xbb4502ba3LLU },
        { 487, 0xbb7ce19adLLU },
        { 478, 0xbb8bc62e4LLU },
        { 541, 0xbb97784f9LLU },
        { 454, 0xbba74f9d6LLU },
        {  32, 0xbc2bf3987LLU },
        {  12, 0xbc5dacd65LLU },
        { 467, 0xbdd5984b6LLU },
        { 129, 0xbe3f4aca7LLU },
        { 387, 0xbf4b5f648LLU },
        {  49, 0xbfa32ab6eLLU },
        { 562, 0xbfda312d0LLU },
        { 379, 0xbffd65c02LLU },
        {  11, 0xc0069d32aLLU },
        { 575, 0xc0155ca9bLLU },
        { 391, 0xc0b5ee72aLLU },
        { 490, 0xc0f682c11LLU },
        { 375, 0xc15781cdaLLU },
        { 544, 0xc1d926c73LLU },
        { 150, 0xc2d13ae58LLU },
        { 395, 0xc56a526f1LLU },
        {  87, 0xc598fabe6LLU },
        { 574, 0xc5ea8a227LLU },
        { 356, 0xc611d50e2LLU },
        { 381, 0xc6190d491LLU },
        { 397, 0xc62ba83faLLU },
        { 324, 0xc64181a8dLLU },
        { 563, 0xc686e1413LLU },
        { 543, 0xc7e85370aLLU },
        { 360, 0xc81dc0752LLU },
        { 512, 0xc8e731e83LLU },
        { 461, 0xc98432232LLU },
        {  75, 0xc9d716d45LLU },
        { 526, 0xca3e47f2eLLU },
        { 439, 0xca70feecbLLU },
        { 482, 0xcac0e195fLLU },
        {  10, 0xcb016e479LLU },
        { 473, 0xcb88f26fcLLU },
        { 507, 0xcbdd45f9bLLU },
        {  98, 0xcc55afbd6LLU },
        { 142, 0xcca983c28LLU },
        { 583, 0xccb853b75LLU },
        { 532, 0xcd29d8010LLU },
        { 416, 0xce09cbe47LLU },
        {  57, 0xce1532ea4LLU },
        { 268, 0xcea3264a1LLU },
        { 282, 0xcf792ad64LLU },
        { 508, 0xd012ee2a9LLU },
        { 568, 0xd018f1facLLU },
        { 242, 0xd03d6564fLLU },
        { 390, 0xd0a3ab26cLLU },
        { 298, 0xd0fef218cLLU },
        { 474, 0xd110798d7LLU },
        { 514, 0xd1253849dLLU },
        {  48, 0xd1efdc847LLU },
        {  31, 0xd22195c25LLU },
        { 358, 0xd262943ddLLU },
        { 267, 0xd2859f1f9LLU },
        { 426, 0xd3064794cLLU },
        { 362, 0xd3680b0f6LLU },
        { 452, 0xd3a4c759fLLU },
        { 582, 0xd3ca4763cLLU },
        { 483, 0xd3e3ed233LLU },
        { 173, 0xd43f9a09aLLU },
        { 517, 0xd481e0396LLU },
        { 317, 0xd49123a43LLU },
        { 192, 0xd5112c167LLU },
        { 231, 0xd5468d810LLU },
        { 147, 0xd64844265LLU },
        { 171, 0xd72974a9cLLU },
        { 396, 0xd73abbf6aLLU },
        { 501, 0xd7979c419LLU },
        { 369, 0xd80f36716LLU },
        { 345, 0xd83c6f868LLU },
        { 254, 0xd8454369dLLU },
        {  47, 0xd941bd3d1LLU },
        { 278, 0xd994deef1LLU },
        {   9, 0xd9a52fb8dLLU },
        { 182, 0xd9c2591ecLLU },
        { 146, 0xd9f13482aLLU },
        { 351, 0xda95f45cbLLU },
        { 525, 0xdadc5b296LLU },
        {  95, 0xdb2b2a6c8LLU },
        { 300, 0xdb75003d6LLU },
        { 585, 0xdba6f9704LLU },
        {   8, 0xdd4e20152LLU },
        { 326, 0xdf30e85a3LLU },
        { 318, 0xdf870357dLLU },
        { 126, 0xdfc4e9c2eLLU },
        { 199, 0xe089bb331LLU },
        { 580, 0xe0caf12c9LLU },
        { 353, 0xe1094efabLLU },
        { 472, 0xe17fd7848LLU },
        { 477, 0xe202d7b10LLU },
        { 523, 0xe313ad9f6LLU },
        { 196, 0xe317f292cLLU },
        { 223, 0xe318bce57LLU },
        { 153, 0xe35e3ba89LLU },
        {  74, 0xe375a95a8LLU },
        { 281, 0xe3ac6bf94LLU },
        { 220, 0xe452b2cf5LLU },
        { 279, 0xe4eb53447LLU },
        { 245, 0xe528b75b8LLU },
        { 313, 0xe54d6119fLLU },
        {  94, 0xe625fb817LLU },
        { 581, 0xe75a179e0LLU },
        { 120, 0xe7abf6352LLU },
        { 179, 0xe7d8be11bLLU },
        {  46, 0xe7e57eae5LLU },
        { 178, 0xe89fa3093LLU },
        { 108, 0xe90806e64LLU },
        { 197, 0xe93f7bd82LLU },
        { 502, 0xe9fbbb26bLLU },
        { 189, 0xea8140fd5LLU },
        {  68, 0xeaf943510LLU },
        { 427, 0xeb34ae1daLLU },
        {  56, 0xeb5cb5cccLLU },
        { 372, 0xeb6f35709LLU },
        { 333, 0xec84dad50LLU },
        { 570, 0xec9b124faLLU },
        { 417, 0xec9c5c5b3LLU },
        { 572, 0xed18e50feLLU },
        { 210, 0xed3f1e139LLU },
        { 359, 0xede2c4b82LLU },
        { 152, 0xee590cbd8LLU },
        { 361, 0xee60ce309LLU },
        { 442, 0xee9f6bc60LLU },
        { 364, 0xef43c3588LLU },
        {   7, 0xef9ad1e2bLLU },
        { 447, 0xefe111c9cLLU },
        { 547, 0xf0c5c4c0dLLU },
        { 429, 0xf13d91574LLU },
        { 131, 0xf14ab698fLLU },
        { 493, 0xf16054dd3LLU },
        { 156, 0xf19e8a9e1LLU },
        { 528, 0xf24e50fedLLU },
        { 422, 0xf2f308c56LLU },
        { 538, 0xf337a6275LLU },
        { 403, 0xf3c66a486LLU },
        { 297, 0xf49cd9a48LLU },
        { 250, 0xf4a0e8b39LLU },
        { 302, 0xf4e581b26LLU },
        { 187, 0xf5adcb502LLU },
        { 249, 0xf5d30f622LLU },
        { 123, 0xf5ec452aaLLU },
        { 206, 0xf62e6731dLLU },
        { 392, 0xf63e76846LLU },
        { 349, 0xf6d031325LLU },
        { 166, 0xf6f75fb0aLLU },
        { 577, 0xf78d46b07LLU },
        { 515, 0xf7a293aedLLU },
        { 394, 0xf7b4ff4e8LLU },
        { 443, 0xf7b777e91LLU },
        { 176, 0xf83c02afdLLU },
        { 393, 0xf86889153LLU },
        { 418, 0xf8a274645LLU },
        { 558, 0xf8cbf017eLLU },
        { 237, 0xf9075a26bLLU },
        { 161, 0xf91a55594LLU },
        { 138, 0xf98d664baLLU },
        { 530, 0xfa20fc06eLLU },
        {   6, 0xfa95a2f7aLLU },
        { 172, 0xfb2dac8b4LLU },
        { 510, 0xfb4fd6113LLU },
        { 246, 0xfc4b8ed2aLLU },
        { 208, 0xfe6947a93LLU },
        { 296, 0xfeb2d2095LLU },
        { 283, 0xfeee22991LLU },
        { 476, 0xfeef4459eLLU },
};
#endif  // SmallTag

int nTagData = sizeof(tagData) / sizeof(*tagData);

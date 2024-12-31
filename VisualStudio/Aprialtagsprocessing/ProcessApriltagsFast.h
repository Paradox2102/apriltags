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

#ifndef _ProcessAprilTagsFast_h
#define _ProcessAprilTagsFast_h

//#define SmallTag

#include <list>
#include "ProcessApriltags.h"

#define max(a, b)	(((a) > (b)) ? (a) : (b))
#define DIM(a)		((int) (sizeof(a) / sizeof(*(a))))

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
 * Specifies the parameters to be used for detecting the tags
 */
class ApriltagParams
{
public:
	int whiteDrop = 80;			// Percent drop in white to recognize black
    int whiteCnt = 3;           // Number of pixels to avarage to get current 'white' value
	int minBlack = 16;			// Min number of consecutive black pixels that are required
	int minSize = 20;			// Min width/height for blob
	double maxSlope = 0.3;		// Max allowable slope from horz/vert allowd
	double maxParallel = 0.15;	// Max allowable deviation for parallel sides
	double minAspect = 0.7;		// Min aspect ratio allowdd
	double maxAspect = 1.3;		// Max aspect ratio allowed
	int sampleRegion = 50;		// % of total region used for auto exposure calculation
};

struct AprilTagData
{
	int tag;
#ifdef SmallTag
	unsigned short value;
#else	// !SmallTag
	uint64_t value;
#endif	// !SmallTag
};

extern AprilTagData tagData[];
extern int nTagData;
extern unsigned char * g_pImage;

static unsigned char bitCount[256] =
{
	0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
	1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
	1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
	1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
	2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
	3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
	3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
	4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
};


class AprilTag
{
public:
	bool m_isValid;					// TRUE if tag is valid
	int m_tag;						// Tag ID
	int m_width;					// Width of image
	int m_height;					// Height of image
	unsigned char* m_pImage;		// Pointer to image data
	int m_minX;						// Left edge of blob
	int m_maxX;						// Right edge of blob
	int m_minY;						// Top edge of blob
	int m_maxY;						// Bottom edge of blob
	int m_nTracePoints = 0;			// Number of point which bound the blob
	short * m_pTracePoints = NULL;	// List of points which bound the blob
	ApriltagParams m_params;		// Process parameters
	double m_corners[4][2];			// Four corners of the blob

	AprilTag(int minX, int minY, int maxX, int maxY, int blackColor,
		unsigned char * pImage, int width, int height, ApriltagParams* params,
		short * pTracePoints = 0, int nTracePoints = 0)
	{
		m_minX = minX;
		m_maxX = maxX;
		m_minY = minY;
		m_maxY = maxY;
		m_width = width;
		m_height = height;
		m_pImage = pImage;
		m_params = *params;

		m_pTracePoints = new short[nTracePoints];
		memmove(m_pTracePoints, pTracePoints, nTracePoints * sizeof(short));
		m_nTracePoints = nTracePoints;
		findCornersFromTracepoints();

		if (m_isValid)
		{
			computeTag(blackColor);
		}
	}

	~AprilTag()
	{
		if (m_pTracePoints)
		{
			delete[] m_pTracePoints;
		}
	}

#ifdef SmallTag
	double m_leftEdgePoints[4][2];
	double m_rightEdgePoints[4][2];
	double m_points[4][4][2];

	/*
	 * This function extracts the bit data from the 6x6 set of white or black
	 * 'pixels' which represents the 16 bits of binary data which identifies the tag
	 *
	 */

	uint64_t markEdgePoints(int x1, int y1, int x2, int y2, double edgePoints[4][2], int blackColor, uint64_t tag)
	{
		for (int i = 3; i <= 9; i += 2)
		{
			double dy = (y2 - y1) / 12.0;
			double dx = (x2 - x1);
			double y = y1 + (i * dy);
			double x = x1 + (i / 12.0) * dx;

			int value = m_pImage[(((int)(y + 1.0)) * m_width) + ((int)(x + 0.5))];

			tag <<= 1;
			if (value < blackColor)
			{
				tag |= 1;
			}

			edgePoints[(i / 2) - 1][0] = x;
			edgePoints[(i / 2) - 1][1] = y;
		}

		return(tag);
	}
#else	// SmallTag
	double m_leftEdgePoints[6][2];
	double m_rightEdgePoints[6][2];
	double m_points[6][6][2];

	uint64_t markEdgePoints(int x1, int y1, int x2, int y2, double edgePoints[6][2], int blackColor, uint64_t tag)
	{
		for (int i = 3; i <= 13; i += 2)
		{
			double dy = (y2 - y1) / 16.0;
			double dx = (x2 - x1);
			double y = y1 + (i * dy);
			double x = x1 + (i / 16.0) * dx;

			int value = m_pImage[(((int)(y + 0.5)) * m_width) + ((int)(x + 0.5))];

			tag <<= 1;
			if (value < blackColor)
			{
				tag |= 1;
			}

			edgePoints[(i / 2) - 1][0] = x;
			edgePoints[(i / 2) - 1][1] = y;
		}

		return(tag);
	}
#endif	// SmallTag

	// Need my own version of this since _Popcount does not seem to be avaiable on the PI
	int popcount(unsigned short value)
	{
		unsigned char* p = (unsigned char*)&value;
		return(bitCount[p[0]] + bitCount[p[1]]);
	}

	int findBestTag(unsigned short value)
	{
		int bestErrors = 16;
		int bestIdx = 0;

		for (int i = 0; i < nTagData; i++)
		{
			int nErrors = popcount((unsigned short) (value ^ tagData[i].value));

			if (nErrors < bestErrors)
			{
				bestErrors = nErrors;
				bestIdx = i;
			}

		}

		if (bestErrors < 3)
		{
			return(tagData[bestIdx].tag);
		}

		return(-1);
	}

	/*
	 * Finds the tag # by searching the valid tag numbers for a match
	 */
#ifdef SmallTag
	int findTag(unsigned int value)
#else	// !SmallTag
	int findTag(uint64_t value)
#endif	// !SmallTag
	{
		int	low = 0;
		int high = nTagData;
		int count = 0;

		// Binary search
		while (high > low)
		{
			int mid = (low + high) / 2;
			uint64_t test = tagData[mid].value;

			if (value == test)
			{
				return(tagData[mid].tag);
			}
			else if (value > test)
			{
				low = mid + 1;
			}
			else
			{
				high = mid - 1;
			}

			if (++count > 1000)
			{
				printf("count=%d\n", count);

				return(-1);
			}
		}

		if (value != tagData[low].value)
		{
#ifdef SmallTag
			return findBestTag(value);
#else	// !SmallTag
			return(-1);
#endif	// !SmallTag
		}

		return (tagData[low].tag);
	}

	int findTagLinear(uint64_t value)
	{
		for (int i = 0; i < nTagData; i++)
		{
			if (value == tagData[i].value)
			{
				return(tagData[i].tag);
			}
		}

		return(-1);
	}

#ifdef SmallTag
	/*
	 * Computes the tag id by finding the 16 bits represented by the black and white
	 * squares of the tag and then searching the valid tag for a match.
	 */
	void computeTag(int blackColor)
	{
		//blackColor -= 10;

		uint64_t value = 0;
		double xul = m_corners[0][0];
		double yul = m_corners[0][1];
		double xur = m_corners[1][0];
		double yur = m_corners[1][1];
		double xlr = m_corners[2][0];
		double ylr = m_corners[2][1];
		double xll = m_corners[3][0];
		double yll = m_corners[3][1];

		markEdgePoints(xul, yul, xll, yll, m_leftEdgePoints, blackColor, 0);
		markEdgePoints(xur, yur, xlr, ylr, m_rightEdgePoints, blackColor, 0);

		for (int i = 0; i < 4; i++)
		{
			value = markEdgePoints(m_leftEdgePoints[i][0], m_leftEdgePoints[i][1],
				m_rightEdgePoints[i][0], m_rightEdgePoints[i][1],
				m_points[i], blackColor, value);
		}

		m_tag = findTag(value);

		m_isValid = m_tag >= 0;
	}
#else	// !SmallTag
	void computeTag(int blackColor)
	{
		uint64_t value = 0;
		double xul = m_corners[0][0];
		double yul = m_corners[0][1];
		double xur = m_corners[1][0];
		double yur = m_corners[1][1];
		double xlr = m_corners[2][0];
		double ylr = m_corners[2][1];
		double xll = m_corners[3][0];
		double yll = m_corners[3][1];

//		double ldx = xll - xul;
//		double rdx = xlr - xur;

		markEdgePoints(xul, yul, xll, yll, m_leftEdgePoints, blackColor, 0);
		markEdgePoints(xur, yur, xlr, ylr, m_rightEdgePoints, blackColor, 0);

		for (int i = 0; i < 6; i++)
		{
			value = markEdgePoints(m_leftEdgePoints[i][0], m_leftEdgePoints[i][1],
				m_rightEdgePoints[i][0], m_rightEdgePoints[i][1],
				m_points[i], blackColor, value);
		}

		m_tag = findTag(value);
	}
#endif	// SmallTag

	/*
	 * Applies a number of test to quickly determine which tags may be invalid.
	 * Returns true if tag is valid.
	 */
	bool checkSides()
	{
		double m1;
		double m2;
		double dm;

		/*
		 * Should have a roughly square aspect ratio.
		 */
		double aspect = (double) (m_maxX - m_minX) / (double) (m_maxY - m_minY);
		if ((aspect < m_params.minAspect) || (aspect > m_params.maxAspect))
		{
			return false;
		}

		/*
		 * Left and right sides should be roughly vertical and parallel
		 */
		m1 = (m_corners[0][0] - m_corners[3][0]) / (m_corners[0][1] - m_corners[3][1]);
		m2 = (m_corners[1][0] - m_corners[2][0]) / (m_corners[1][1] - m_corners[2][1]);
		dm = abs(m1 - m2);

		if ((abs(m1) > m_params.maxSlope) || (abs(m2) > m_params.maxSlope) || (dm > m_params.maxParallel))
		{
			return false;
		}

		/*
		 * Top and bottom sides should be roughly horizontal and parallel
		 */
		m1 = (m_corners[0][1] - m_corners[1][1]) / (m_corners[0][0] - m_corners[1][0]);
		m2 = (m_corners[3][1] - m_corners[2][1]) / (m_corners[3][0] - m_corners[2][0]);
		dm = abs(m1 - m2);

		if ((abs(m1) > m_params.maxSlope) || (abs(m2) > m_params.maxSlope) || (dm > m_params.maxParallel))
		{
			return false;
		}

		return true;
	}

	int m_ulCorner;
	int m_urCorner;
	int m_llCorner;
	int m_lrCorner;

	double m_upperM;
	double m_upperB;
	double m_lowerM;
	double m_lowerB;
	double m_leftM;
	double m_leftB;
	double m_rightM;
	double m_rightB;

	/*
	 * Computes the slope and intersect (m & b) of a line which bounds the left or right side of the tag
	 * by performing a least squares fit to the points along that edge.
	 */
	void computeHorzLine(int startIdx, int endIdx, double& m, double& b)
	{
		int n = 0;
		double xsum = 0, x2sum = 0, ysum = 0, xysum = 0;                //variables for sums/sigma of xi,yi,xi^2,xiyi etc

		while (startIdx != endIdx)
		{
			int x = m_pTracePoints[startIdx];
			int y = m_pTracePoints[startIdx + 1];
			xsum = xsum + x;                    //calculate sigma(xi)
			ysum = ysum + y;                    //calculate sigma(yi)
			x2sum = x2sum + x*x;                //calculate sigma(x^2i)
			xysum = xysum + x * y;              //calculate sigma(xi*yi)
			n++;

			startIdx += 2;
			if (startIdx >= m_nTracePoints)
			{
				// Wrap around the buffer
				startIdx = 0;
			}
		}
		m = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum);          // calculate slope
		b = (x2sum * ysum - xsum * xysum) / (x2sum * n - xsum * xsum);		// calculate intercept
	}

	/*
	 * Computes the slope and intersect (m & b) of a line which bounds the top or bottom of the tag
	 * by performing a least squares fit to the points along that edge.
	 */
	void computeVertLine(int startIdx, int endIdx, double& m, double& b)
	{
		int n = 0;
		double xsum = 0, x2sum = 0, ysum = 0, xysum = 0;                //variables for sums/sigma of xi,yi,xi^2,xiyi etc

		while (startIdx != endIdx)
		{
			int y = m_pTracePoints[startIdx];
			int x = m_pTracePoints[startIdx + 1];
			xsum = xsum + x;                      //calculate sigma(xi)
			ysum = ysum + y;                      //calculate sigma(yi)
			x2sum = x2sum + x * x;                //calculate sigma(x^2i)
			xysum = xysum + x * y;                //calculate sigma(xi*yi)
			n++;

			startIdx += 2;
			if (startIdx >= m_nTracePoints)
			{
				startIdx = 0;
			}
		}
		m = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum);          //calculate slope
		b = (x2sum * ysum - xsum * xysum) / (x2sum * n - xsum * xsum);		// calculate intercept
	}

	void computeCorner(double horzM, double horzB, double vertM, double vertB, double corner[2])
	{
		double x = (horzB * vertM + vertB) / (1 - horzM * vertM);
		double y = horzM * x + horzB;

		if ((corner[0] < 0 )|| (corner[0] >= m_width) || (corner[1] < 0) || (corner[1] >= m_height))
		{
            printf("bad corner\n");
            return;
		}

		corner[0] = x;
		corner[1] = y;
	}

	/*
	 * Finds the four corners of the polygon which bounds the tag
	 * from the array of points generated by the traceBlob function
	 */
	void findCornersFromTracepoints()
	{
		int cx = (m_minX + m_maxX) / 2;
		int cy = (m_minY + m_maxY) / 2;
		int dTopLeft = 0;
		int dBotLeft = 0;
		int dTopRight = 0;
		int dBotRight = 0;

        dTopLeft = 0;
        dBotLeft = 0;
        dTopRight = 0;
        dBotRight = 0;

		m_ulCorner = -1;
		m_urCorner = -1;
		m_llCorner = -1;
		m_lrCorner = -1;

		for (int i = 0 ; i < 4 ; i++)
		{
            m_corners[i][0] = -1;
            m_corners[i][1] = -1;
		}

		for (int i = 0; i < m_nTracePoints ; i += 2)
		{
			int x = m_pTracePoints[i];
			int y = m_pTracePoints[i + 1];

			int dx = (cx - x);
			int dy = (cy - y);
			int d = (dx * dx) + (dy * dy);

			if (y < cy)
			{
				// top
				if (x < cx)
				{
					// top left
					if (d > dTopLeft)
					{
						m_ulCorner = i;
						dTopLeft = d;
						m_corners[0][0] = x;
						m_corners[0][1] = y;
					}
				}
				else
				{
					// top right
					if (d > dTopRight)
					{
						m_urCorner = i;
						dTopRight = d;
						m_corners[1][0] = x;
						m_corners[1][1] = y;
					}
				}
			}
			else
			{
				// bottom
				if (x < cx)
				{
					// bottom left
					if (d > dBotLeft)
					{
						m_llCorner = i;
						dBotLeft = d;
						m_corners[3][0] = x;
						m_corners[3][1] = y;
					}
				}
				else
				{
					// bottom right
					if (d > dBotRight)
					{
						m_lrCorner = i;
						dBotRight = d;
						m_corners[2][0] = x;
						m_corners[2][1] = y;
					}
				}
			}
		}

		// Make sure there are 4 corners

		if ((m_ulCorner < 0) || (m_urCorner < 0) || (m_llCorner < 0) || (m_lrCorner < 0))
		{
			m_isValid = false;
			return;
		}

        if (!(m_isValid = checkSides()))
        {
            return;
        }

		computeHorzLine(m_ulCorner, m_urCorner, m_upperM, m_upperB);
		computeHorzLine(m_lrCorner, m_llCorner, m_lowerM, m_lowerB);
		computeVertLine(m_llCorner, m_ulCorner, m_leftM, m_leftB);
		computeVertLine(m_urCorner, m_lrCorner, m_rightM, m_rightB);

		computeCorner(m_upperM, m_upperB, m_leftM, m_leftB, m_corners[0]);
		computeCorner(m_upperM, m_upperB, m_rightM, m_rightB, m_corners[1]);
		computeCorner(m_lowerM, m_lowerB, m_rightM, m_rightB, m_corners[2]);
		computeCorner(m_lowerM, m_lowerB, m_leftM, m_leftB, m_corners[3]);
	}
};


extern unsigned char* marked;
extern int markedWidth;
extern int markedHeight;
extern void Move(Direction dir, int x, int y, int* px, int* py);
extern std::list<ImageRegion *> * processApriltagsFast(unsigned char* pImage, int width, int height, ApriltagParams* params, int *pBlobCount, int* pAvgWhite);
extern std::list<AprilTag*>* findAprilTags(unsigned char* pImage, int width, int height, ApriltagParams* params, int * pBlobCount, int *pAvgWhit);
extern void deleteAprilTags(std::list<AprilTag*>* pList);
extern void deleteImageRegions(std::list<ImageRegion*>* pRegions);
extern bool TestPixel(unsigned char* pImage, int x, int y, int width, int height, int BlackColor);
extern int traceBlob(unsigned char* pImage,		// Pointer to the image buffer
	int	width,		// Width of the image in pixels
	int	height,		// Height of the image in pixels
	int	xStart,		// Horizontal coordinate of the starting pixel
	int	yStart,		// Vertical coordiante of the starting pixel
	int blackColor, // If blackColor < 0 pick a new black based on starting pixels
	int& minX,
	int& maxX,
	int& minY,
	int& maxY,
	short* pPoints = 0,
	int maxPoints = 0);

#endif	// ProcessApriltagsFast

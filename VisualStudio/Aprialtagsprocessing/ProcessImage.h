#ifndef _ProcessImage_h
#define _ProcessImage_h

#include <list>
#include "ProcessApriltags.h"

//#ifdef WIN32
//#pragma warning (disable: 4305 4091 4018 4101 4341 4102 4135 4746 4761 4091 4018 4341 4244 4245 4759 4309 4146 4996 4800 4250)
//
//#define snprintf	_snprintf
//#endif	// WIN32

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

#define MaxWidth	720
#define MaxHeight	1280

class AprilTag
{
public:
	int m_width;
	int m_height;
	int m_minX;
	int m_maxX;
	int m_minY;
	int m_maxY;
	int m_leftMin;
	int m_leftCount;
	int m_rightMin;
	int m_rightCount;
	double m_corners[4][2];

	unsigned short * m_leftEdge;
	unsigned short * m_rightEdge;

	AprilTag(int minX, int minY, int maxX, int maxY, unsigned short* leftEdge, int leftMin, int leftMax, 
					unsigned short* rightEdge, int rightMin, int rightMax, int width, int height)
	{
		m_minX = minX;
		m_maxX = maxX;
		m_minY = minY;
		m_maxY = maxY;
		m_width = width;
		m_height = height;
		m_leftMin = leftMin;
		m_leftCount = leftMax - leftMin + 1;
		m_rightMin = rightMin;
		m_rightCount = rightMax - rightMin + 1;

		m_leftEdge = new unsigned short[m_leftCount];
		m_rightEdge = new unsigned short[m_rightCount];

		memcpy(m_leftEdge, leftEdge + leftMin, m_leftCount * sizeof(unsigned short));
		memcpy(m_rightEdge, rightEdge + rightMin, m_rightCount * sizeof(unsigned short));

		findCorners();
	}

	~AprilTag()
	{
		delete[] m_leftEdge;
		delete[] m_rightEdge;
	}

	void findCorners()
	{
		int cx = (m_minX + m_maxX) / 2;
		int cy = (m_minY + m_maxY) / 2;
		int dTopLeft = 0;
		int dBotLeft = 0;
		int dTopRight = 0;
		int dBotRight = 0;

		for (int i = 0; i < m_leftCount + m_rightCount; i++)
		{
			int x;
			int y;

			if (i < m_leftCount)
			{
				x = m_leftEdge[i];
				y = m_leftMin + i;
			}
			else
			{
				x = m_rightEdge[i - m_leftCount];
				y = m_rightMin + i - m_leftCount;
			}

			int dx = (cx - x);
			int dy = (cy - y);
			int d = (dx * dx) + (dy * dy);

			if (y > cy)
			{
				// top
				if (x < cx)
				{
					// top left
					if (d > dTopLeft)
					{
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
						dBotRight = d;
						m_corners[2][0] = x;
						m_corners[2][1] = y;
					}
				}
			}
		}

		//dTop = 0;
		//dBot = 0;

		//for (int i = 0; i < m_rightCount; i++)
		//{
		//	int x = m_rightEdge[i];
		//	int y = m_rightMin + i;
		//	int dx = (cx - x);
		//	int dy = (cy - y);
		//	int d = (dx * dx) + (dy * dy);

		//	if (y > cy)
		//	{
		//		if (d > dTop)
		//		{
		//			dTop = d;
		//			m_corners[1][0] = x;
		//			m_corners[1][1] = y;
		//		}
		//	}
		//	else
		//	{
		//		if (d > dBot)
		//		{
		//			dBot = d;
		//			m_corners[2][0] = x;
		//			m_corners[2][1] = y;
		//		}
		//	}
		//}
	}
};

//class ImageRegion
//{
//public:
//	int m_tag;
//	int m_corners[4][2];
//
//	ImageRegion(int tag, int corners[4][2])
//	{
//		m_tag = tag;
//		memmove(m_corners, corners, sizeof(m_corners));
//	}
//};

extern void Move(Direction dir, int x, int y, int* px, int* py);
extern std::list<ImageRegion *> * processImageFast(unsigned char* pImage, int width, int height, int BlackColor, int MinWhite, int MinBlack, int minSize);
extern std::list<AprilTag*>* findAprilTags(unsigned char* pImage, int width, int height, int BlackColor, int MinWhite, int MinBlack, int minSize);
extern void deleteAprilTags(std::list<AprilTag*>* pList);
extern void deleteImageRegions(std::list<ImageRegion*>* pRegions);
#endif	// ProcessImage_h

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

#ifndef _ProcessApriltags_h
#define _ProcessApriltags_h

#include <stdio.h>
#include <list>
#include <cstring>
#include "stdint.h"

#ifdef WIN32
#pragma warning (disable: 4305 4091 4018 4101 4341 4102 4135 4746 4761 4091 4018 4341 4244 4245 4759 4309 4146 4996 4800 4250)

#define snprintf	_snprintf
#endif	// WIN32

#define max(a, b)	(((a) > (b)) ? (a) : (b))
#define DIM(a)		((int) (sizeof(a) / sizeof(*(a))))

class ImageRegion
{
public:
	int 	m_tag;
	double	m_corners[4][2];

	ImageRegion(int tag, double corners[4][2])
	{
		m_tag = tag;
		memmove(m_corners, corners, sizeof(m_corners));
	}

	ImageRegion(ImageRegion * pSrc)
	{
		m_tag = pSrc->m_tag;
		memmove(m_corners, pSrc->m_corners, sizeof(m_corners));
	}
};


extern std::list<ImageRegion*>* ProcessAprilTags(unsigned char * pData, int width, int height);
extern void deleteImageRegions(std::list<ImageRegion*>* pRegions);

#endif	// _ProcessApriltags_h

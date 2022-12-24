#ifndef _ProcessImage_h
#define _ProcessImage_h

#ifdef WIN32
#pragma warning (disable: 4305 4091 4018 4101 4341 4102 4135 4746 4761 4091 4018 4341 4244 4245 4759 4309 4146 4996 4800 4250)

#define snprintf	_snprintf
#endif	// WIN32

#define max(a, b)	(((a) > (b)) ? (a) : (b))
#define DIM(a)		((int) (sizeof(a) / sizeof(*(a))))
#define NAN	1000

#ifdef BGR
#define RED(p)		((p)[2])
#define GREEN(p)	((p)[1])
#define BLUE(p)		((p)[0])
#else
#define RED(p)		((p)[0])
#define GREEN(p)	((p)[1])
#define BLUE(p)		((p)[2])
#endif

enum BlobPosition
{
	BlobPosition_First,
	BlobPosition_Left,
	BlobPosition_Right,
};

class HSVFilter
{
public:
//	bool	m_set;
	int		m_minH;
	int		m_maxH;
	int		m_minS;
	int		m_maxS;
	int		m_minV;
	int		m_maxV;

	HSVFilter()
	{
//		m_set	= false;
		m_minH		= 0;
		m_maxH		= 0;
		m_minS		= 0;
		m_maxS		= 0;
		m_minV		= 0;
		m_maxV		= 0;
	}

	void SetHSVFilter(int minH, int maxH, int minS, int maxS, int minV, int maxV)
	{
//		m_set	= true;
		m_minH	= minH;
		m_maxH	= maxH;
		m_minS	= minS;
		m_maxS	= maxS;
		m_minV	= minV;
		m_maxV	= maxV;
	}
};

class HSVFilters
{
public:
	int			m_color;
	int			m_nFilters;
	HSVFilter	m_filters[4];

	HSVFilters()
	{
		m_color		= 0;
		m_nFilters	= 0;
	}

	void SetHSVFilter(int filter, int minH, int maxH, int minS, int maxS, int minV, int maxV)
	{
		if ((filter >= 0) && (filter < DIM(m_filters)))
		{
			m_filters[filter].SetHSVFilter(minH, maxH, minS, maxS, minV, maxV);

			if ((filter + 1) > m_nFilters)
			{
				m_nFilters	= filter + 1;
			}
		}
	}

	void SetColor(int color)
	{
		if ((color >= 0) && (color < DIM(m_filters)))
		{
			m_color	= color;
		}
	}
};

class ImageRect
{
public:
	int	left;
	int	top;
	int	right;
	int	bottom;

	ImageRect()
	{
		left	= 0;
		top		= 0;
		right	= 0;
		bottom	= 0;
	}
};

class ImagePoint
{
public:
	int	x;
	int	y;

	ImagePoint()
	{
		x	= 0;
		y	= 0;
	}

	ImagePoint(int xx, int yy)
	{
		x	= xx;
		y	= yy;
	}
};

class ImageRegion
{
public:
	int			m_color;
	int			m_area;
	int			m_topLeft;
	int			m_topRight;
	ImageRect	m_bounds;
//	ImagePoint	m_corners[4];

	void Reflect(int width)
	{
		int bWidth	= m_bounds.right - m_bounds.left;

		m_bounds.left	= width - m_bounds.right;
		m_bounds.right	= m_bounds.left + bWidth;

#ifdef notdef
		for (int i = 0 ; i < DIM(m_corners) ; i++)
		{
			m_corners[i].x	= width - m_corners[i].x;
		}
#endif	// notdef
	}

	ImagePoint GetCenterTop()
	{
#ifdef notdef
		ImagePoint	p1;
		ImagePoint	p2;

		if (m_corners[0].y < m_corners[1].y)
		{
			p1	= m_corners[0];
			p2	= m_corners[1];
		}
		else
		{
			p1	= m_corners[1];
			p2	= m_corners[0];
		}

		for (int i = 2 ; i < 4 ; i++)
		{
			if (m_corners[i].y < p1.y)
			{
				p1	= m_corners[i];
			}
			else if (m_corners[i].y < p2.y)
			{
				p2	= m_corners[i];
			}
		}

		ImagePoint p((m_corners[0].x + m_corners[2].x) / 2, (p1.y + p2.y) / 2);
#endif	// notdef

		ImagePoint	p((m_bounds.left + m_bounds.right) / 2, (m_topLeft + m_topRight) / 2);

		return(p);
	}
};

class ImageRegions
{
	int				m_maxRegions;

public:
	int				m_nRegions;
	ImageRegion	*	m_pRegions;

	ImageRegions(int max)
	{
		m_maxRegions	= max;
		m_nRegions		= 0;
		m_pRegions		= new ImageRegion[m_maxRegions];
	}

	~ImageRegions()
	{
		delete[] m_pRegions;
	}

	ImageRegions * Clone()
	{
		ImageRegions	*	pDstRegions = new ImageRegions(m_maxRegions);

		for (int i = 0 ; i < m_nRegions ; i++)
		{
			pDstRegions->m_pRegions[i]	= m_pRegions[i];
		}

		pDstRegions->m_nRegions	= m_nRegions;

		return(pDstRegions);
	}

	void Reflect(int width)
	{
		for (int i = 0 ; i < m_nRegions ; i++)
		{
			m_pRegions[i].Reflect(width);
		}
	}

	void AddRegion(ImageRect * pBounds, int topLeft, int topRight, int area, int color)
	{
		int	i;

		for (i = 0 ; i < m_nRegions ; i++)
		{
			if (area > m_pRegions[i].m_area)
			{
				break;
			}
		}

		//if (i < m_maxRegions)
		{
			if (m_nRegions < m_maxRegions)
			{
				m_nRegions++;
			}

			if (i >= m_nRegions)
			{
				return;
			}

			for (int j = m_nRegions - 1 ; j > i ; j--)
			{
				m_pRegions[j]	= m_pRegions[j - 1];
			}
		
			m_pRegions[i].m_area		= area;
			m_pRegions[i].m_color		= color;
			m_pRegions[i].m_topLeft		= topLeft;
			m_pRegions[i].m_topRight	= topRight;

			m_pRegions[i].m_bounds	= *pBounds;
			
#ifdef notdef
			for (int j = 0 ; j < DIM(m_pRegions->m_corners) ; j++)
			{
				m_pRegions[i].m_corners[j]	= pCorners[j];
			}
#endif	// notdef
		}
	}
};

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

extern int MinArea;
//extern int nBlobs;



extern bool CreateFilterTable(HSVFilters * pFilters);

extern int ProcessImage(
unsigned char	*	pBits,			// Pointer to the frame buffer data.  For this particular frame
									// buffer, the data is organized using 3 bytes per pixel arranged as BGR
int					width,			// Width of the image in pixels
int					height,			// Height of the image in pixels
int					stride,			// The number of bytes you must advance to get from one row to the next 
bool				modify,			// If true, black out matching pixels and draw bounding box
ImageRegions	*	pRegions);

extern hsv rgb2hsv(rgb in);


#endif	// ProcessImage_h

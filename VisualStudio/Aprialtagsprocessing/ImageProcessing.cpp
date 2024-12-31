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

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include "ProcessAprilTagsFast.h"
#include <windows.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <gdiplus.h>

/*
 * This program is a tool which can be used to debug the apriltag processing code.
 * The files 'ProcessApriltagsFast.cpp' and 'ProcessApriltagsFast.' are the same
 * file that are used to build the Raspberry Pi executable which performs the
 * tag detection.
 * 
 */

#pragma warning (disable: 4305 4091 4018 4101 4341 4102 4135 4746 4761 4091 4018 4341 4244 4245 4759 4309 4146 4996 4800 4250)

#include "resource.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

void Log(char* format, ...)
{

	char msgbuf[128];
	va_list argptr;
	va_start(argptr, format);
	vsnprintf(msgbuf, sizeof(msgbuf), format, argptr);
	va_end(argptr);

	OutputDebugString(msgbuf);
}

double GetTimer()
{
	int	time	= GetTickCount();

	return(((double) time) / 1000.0);
}

HWND g_hWnd;
//#define ID_MarkBlack		200
#define ID_MarkNormal		201
#define ID_MarkLeftEdge		202
#define ID_MarkBlobs		203
//#define ID_MarkTags			204
#define ID_MarkRegions		205
#define ID_NextImage		206
#define ID_PrevImage		207
#define ID_ImageCount		208
//#define ID_AdjustBlack		209

#define ButtonHeight	30
#define ButtonWidth		120

HWND g_hCountWnd;

HWND CreateButtonControl(int id, char* pText, int x, int y)
{
	return(CreateWindow("BUTTON", pText, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		x, y, ButtonWidth, ButtonHeight, g_hWnd, (HMENU)id,
		hInst, NULL));
}

HWND CreateStaticControl(char* pText, int x, int y, int width)
{
	return(CreateWindow("STATIC", pText, WS_VISIBLE | WS_CHILD, x, y, width, 20, g_hWnd, NULL, hInst, NULL));
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   g_hWnd = 
   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   CreateButtonControl(ID_MarkNormal, "Mark Normal", 0, 0);
//   CreateButtonControl(ID_MarkBlack, "Mark Black", 0, ButtonHeight);
//   CreateButtonControl(ID_MarkLeftEdge, "Mark Left", 0, 2 * ButtonHeight);
   CreateButtonControl(ID_MarkBlobs, "Mark Blobs", 0, 3 * ButtonHeight);
//   CreateButtonControl(ID_MarkTags, "Mark Tags", 0, 4 * ButtonHeight);
   CreateButtonControl(ID_MarkRegions, "Mark Regions", 0, 5 * ButtonHeight);
//   CreateButtonControl(ID_AdjustBlack, "Refine Edges", 0, 6 * ButtonHeight);

   CreateButtonControl(ID_NextImage, "Next Image", 0, 700 - ButtonHeight);
   CreateButtonControl(ID_PrevImage, "Prev Image", 0, 700);

   g_hCountWnd = CreateStaticControl("xxx", 0, 700 + ButtonHeight, 200);


   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

Bitmap * CreatePPmBitmap(unsigned char * pData, int width, int height)
{
	return(new Bitmap(width, height, width, PixelFormat16bppGrayScale, pData));
}

void MarkPixel(BitmapData& data, int x, int y)
{
	unsigned char* p = ((unsigned char*)data.Scan0) + (y * data.Stride) + (x * 3);

	p[1] = 0xff;
}

void MarkPixelRed(BitmapData& data, int x, int y)
{
	unsigned char* p = ((unsigned char*)data.Scan0) + (y * data.Stride) + (x * 3);

	p[0] = 0;
	p[1] = 0;
	p[2] = 0xff;
}

void Paint(HWND hWnd);

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
unsigned char* markedPixels = 0;

ApriltagParams params;
bool adjustBlack = false;

void FitLine(int* x, int* y, int n, double& m, double& b)
{
	double xsum = 0, x2sum = 0, ysum = 0, xysum = 0;                //variables for sums/sigma of xi,yi,xi^2,xiyi etc
	for (int i = 0; i < n; i++)
	{
		xsum = xsum + x[i];                        //calculate sigma(xi)
		ysum = ysum + y[i];                        //calculate sigma(yi)
		x2sum = x2sum + pow(x[i], 2);                //calculate sigma(x^2i)
		xysum = xysum + x[i] * y[i];                    //calculate sigma(xi*yi)
	}
	m = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum);            //calculate slope
	b = (x2sum * ysum - xsum * xysum) / (x2sum * n - xsum * xsum);
}

class PPmBitmap : public Bitmap
{
public:
	int				m_width;
	int				m_height;
	unsigned char *	m_pData;

	void SetPixels(BitmapData& data)
	{
		for (int y = 0; y < m_height; y++)
		{
			unsigned char* d = ((unsigned char*)data.Scan0) + (y * data.Stride);
			unsigned char* s = m_pData + (y * m_width);

			for (int x = 0; x < m_width; x++, s++, d += 3)
			{
				unsigned char v = *s;

				d[0] =
				d[1] =
				d[2] = v;
			}
		}

		UnlockBits(&data);
	}

	void SetPixels()
	{
		int			x;
		int			y;
		Rect		rect(0, 0, m_width, m_height);
		BitmapData	data;

		LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &data);

		SetPixels(data);

		UnlockBits(&data);
	}

	void MarkLeftEdge(bool markBlob = false)
	{
		int			x;
		int			y;
		Rect		rect(0, 0, m_width, m_height);
		BitmapData	data;
		int			white = 0;
		int			black = 0;
		int			firstBlack;
		int			blobCount = 0;

		memset(marked, 0, markedWidth * markedHeight);

		LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &data);

		SetPixels(data);

		int avgCnt = 3;
		int	avgWhite;
		int whiteDrop = 80;

		for (y = 1; y < m_height - 1; y += 1)
		{
			unsigned char* s = m_pData + (y * m_width);
			unsigned char* m = marked + (y * m_width);

			int black = 0;
			int firstBlack = 0;
			int white = 0;
			int x0 = 0;
			int markCount = m_width;

			avgWhite = 0;
			for (x = 0; x < avgCnt; x++, s++)
			{
				avgWhite += *s;
			}

			for (; x < m_width; x++, s++)
			{
				if (m[x])
				{
					markCount = 0;
				}

				int black = ((avgWhite * whiteDrop) / (100 * avgCnt));
				if (*s < black)
				{
					int x0 = x;
					bool duplicate = markCount <= 12;

					black = (s[-avgCnt] * whiteDrop) / 100;

					while ((x > 1) && (s[-1] < black))
					{
						s--;
						x--;
					}
					x0 = x;

					// Skip to end of black run
					for (; x < m_width; x++, s++)
					{
						if (m[x])
						{
							duplicate = true;
						}

						if (*s >= black)
						{
							break;
						}
					}

					// Only mark if black run is long enough
					if (!duplicate && ((x - x0) > 24))
					{
						MarkPixel(data, x0, y);

						if (markBlob)
						{
							int minX;
							int maxX;
							int minY;
							int maxY;
							int leftMin;
							int leftMax;
							int rightMin;
							int rightMax;

							traceBlob(m_pData,		// Pointer to the image buffer 
								m_width,		// Width of the image in pixels
								m_height,		// Height of the image in pixels
								x0,		// Horizontal coordinate of the starting pixel
								y,		// Vertical coordiante of the starting pixel
								black, // If blackColor < 0 pick a new black based on starting pixels
								minX,
								maxX,
								minY,
								maxY,
								0, 0);

							blobCount++;
						}
					}

					avgWhite = 0;
					for (int i = 0; (i < avgCnt) && (x < m_width); i++, x++, s++)
					{
						avgWhite += *s;
					}
					x--;
					s--;
					markCount = m_width;
				}
				else
				{
					avgWhite += *s - s[-avgCnt];
					markCount++;
				}
			}
		}

		if (markBlob)
		{
			unsigned char* s = marked;

			for (int y = 0; y < m_height; y++)
			{
				for (int x = 0; x < m_width; x++, s++)
				{
					if (*s)
					{
						MarkPixel(data, x, y);
					}
				}
			}
		}

		UnlockBits(&data);
	}

	void MarkBlobs()
	{
		int count;
		int white;
		BitmapData	data;

		LockBits(&Gdiplus::Rect(0, 0, m_width, m_height), ImageLockModeWrite, PixelFormat24bppRGB, &data);

		SetPixels(data);

		std::list<AprilTag*>* pList = findAprilTags(m_pData, m_width, m_height, &params, &count, &white);

		unsigned char* s = marked;

		for (int y = 0; y < m_height; y++)
		{
			for (int x = 0; x < m_width; x++, s++)
			{
				if (*s)
				{
					MarkPixel(data, x, y);
				}
			}
		}

		UnlockBits(&data);

		deleteAprilTags(pList);
	}

	void GetPixels()
	{
		int			x;
		int			y;
		Rect		rect(0, 0, m_width, m_height);
		BitmapData	data;

		m_pData	= (unsigned char *) malloc(m_width * m_height * 3);

		LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &data);

		for (y = 0 ; y < m_height ; y++)
		{
			unsigned char * s	= ((unsigned char *) data.Scan0) + (y * data.Stride);
			unsigned char * d	= m_pData + (y * m_width * 3);

			for (x = 0 ; x < m_width ; x++, s += 3, d += 3)
			{
				d[0]	= s[2];
				d[1]	= s[1];
				d[2]	= s[0];
			}
		}

		UnlockBits(&data);
	}

	PPmBitmap(unsigned char * pData, int width, int height) : Bitmap(width, height, PixelFormat24bppRGB)
	{
		m_width		= width;
		m_height	= height;

		m_pData	= pData;

		SetPixels();
	}

	PPmBitmap(WCHAR * pFile) : Bitmap(pFile)
	{
		m_width		= GetWidth();
		m_height	= GetHeight();

		GetPixels();
	}

	~PPmBitmap()
	{
		free(m_pData);
	}
};

PPmBitmap * LoadPPmBitmap(TCHAR * pFile)
{
	PPmBitmap	*	pBitmap	= 0;
	FILE		*	fp	= fopen(pFile, _T("rb"));
	char			line[512];

	if (fp == NULL)
	{
		printf("Cannot open %s\n", pFile);
		return(NULL);
	}

	fgets(line, DIM(line), fp);

	if (!strcmp(line, "P5\n"))
	{
		int	width = 0;
		int	height = 0;
		int value = 0;

		fgets(line, DIM(line), fp);

		if (sscanf(line, "%d%d%d", &width, &height, &value) != 3)
		{
			if (sscanf(line, "%d", &width) == 1)
			{
				fgets(line, DIM(line), fp);

				if (sscanf(line, "%d", &height) == 1)
				{
					fgets(line, DIM(line), fp);

					sscanf(line, "%d", &value);
				}
			}
		}

		if (width && height && value)
		{
			fpos_t			pos1;
			fpos_t			pos2;
			int				size;
			unsigned char * pData;

			fgetpos(fp, &pos1);
			fseek(fp, 0, SEEK_END);
			fgetpos(fp, &pos2);

			size	= pos2 - pos1;

			fseek(fp, pos1, SEEK_SET);

			if (size == (width * height))
			{
				unsigned char * pPixels	= (unsigned char *) malloc(size);

				if (fread(pPixels, 1, size, fp) == size)
				{
					pBitmap = new PPmBitmap(pPixels, width, height);
				}
				
				if (!pBitmap)
				{
					free(pPixels);
				}
			}
		}
	}

	return(pBitmap);
}

PPmBitmap* g_pBitmap;
#define Margin	200
bool g_markRegions = true;
int g_bitmapIdx = 0;
int g_bitmapCount;
std::list<char*> g_files;

void GetFiles(char* pFolder)
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	char path[512];

	// Search subfolders
	snprintf(path, sizeof(path), "%s\\*", pFolder);

	hFind = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE != hFind)
	{
		do
		{
			if ((ffd.cFileName[0] != '.') && (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				snprintf(path, sizeof(path), "%s\\%s", pFolder, ffd.cFileName);
				GetFiles(path);
			}
		} while (FindNextFile(hFind, &ffd) != 0);
	}

	// Enumerate '.ppm' files
	snprintf(path, sizeof(path), "%s\\*.ppm", pFolder);

	hFind = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE != hFind)
	{
		do
		{
			int len = strlen(pFolder) + strlen(ffd.cFileName) + 3;

			char* pPath = (char*)malloc(len);

			snprintf(pPath, len, "%s\\%s", pFolder, ffd.cFileName);

			g_files.push_back(pPath);

		} while (FindNextFile(hFind, &ffd) != 0);
	}
	g_bitmapCount = g_files.size();
}

char* GetBitmapPath(int idx)
{
	int i = 0;

	for (char* pFile : g_files)
	{
		if (i == idx)
		{
			return(pFile);
		}
		i++;
	}

	return(0);
}

void SetFileText(char * pFile)
{
	SendMessage(g_hCountWnd, WM_SETTEXT, 0, (LPARAM)pFile);
}

PPmBitmap* GetBitmap(int idx)
{
	char* pFile = GetBitmapPath(idx);

	if (pFile)
	{
		SetFileText(pFile);
		return LoadPPmBitmap(pFile);
	}

	return(0);
}

void LoadNextBitmap()
{
	if (g_bitmapIdx < (g_bitmapCount - 1))
	{
		if (g_pBitmap) delete g_pBitmap;
		g_bitmapIdx++;
		g_pBitmap = GetBitmap(g_bitmapIdx);
	}
	InvalidateRect(g_hWnd, NULL, false);
}

void LoadPrevBitmap()
{
	if (g_bitmapIdx > 0)
	{
		if (g_pBitmap) delete g_pBitmap;
		g_bitmapIdx--;
		g_pBitmap = GetBitmap(g_bitmapIdx);
	}
	InvalidateRect(g_hWnd, NULL, false);
}

void DrawHorzLine(Gdiplus::Graphics& graphics, int width, double m, double b)
{
	graphics.DrawLine(&Pen(Color(0, 255, 0), 1), (float) Margin, (float)b, (float)(Margin + width), (float)(m * width + b));
}

void DrawVertLine(Gdiplus::Graphics& graphics, int height, double m, double b)
{
	graphics.DrawLine(&Pen(Color(0, 255, 0), 1), (float)(Margin + b), 0.0, (float)(Margin + m * height + b), (float)height);
}

void Paint(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC	hdc = BeginPaint(hWnd, &ps);

	if (g_pBitmap)
	{
		Graphics		graphics(hdc);

		int		width = g_pBitmap->GetWidth();
		int		height = g_pBitmap->GetHeight();
		/*
		 * paint the image before we process it
		 */
		Status s = graphics.DrawImage(g_pBitmap, Margin, 0, width, height);

		if (g_markRegions)
		{
			int	blobCount;
			int avgWhite;

			std::list<ImageRegion*>* pRegions = processApriltagsFast(g_pBitmap->m_pData, width, height, &params, &blobCount, &avgWhite);
			for (ImageRegion* pRegion : *pRegions)
			{
				for (int i = 0; i < 4; i++)
				{
					int ii = (i + 1) % 4;

					graphics.DrawLine(&Pen(Color(255, 0, 0), 1), (int)pRegion->m_corners[i][0] + Margin, (int)pRegion->m_corners[i][1],
						(int)pRegion->m_corners[ii][0] + Margin, (int)pRegion->m_corners[ii][1]);
				}
			}

			deleteImageRegions(pRegions);

			std::list<AprilTag*>* pTags = findAprilTags(g_pBitmap->m_pData, width, height, &params, &blobCount, &avgWhite);

			for (AprilTag* pTag : *pTags)
			{
#ifdef SmallTag
				int max = 4;
#else	//	!SmallTag
				int max = 6;
#endif	//	!SmallTag

				for (int i = 0; i < max; i++)
				{
					graphics.DrawLine(&Pen(Color(0, 244, 0), 1),
						(int)(pTag->m_leftEdgePoints[i][0] + 0.5) + Margin,
						(int)(pTag->m_leftEdgePoints[i][1] + 0.5),
						(int)(pTag->m_rightEdgePoints[i][0] + 0.5) + Margin,
						(int)(pTag->m_rightEdgePoints[i][1] + 0.5));
				}

				for (int i = 0; i < max; i++)
				{
					for (int j = 0; j < max; j++)
					{
						int x = (int)(pTag->m_points[i][j][0] + 0.5) + Margin;
						int y = (int)(pTag->m_points[i][j][1] + 0.5);

						graphics.DrawLine(&Pen(Color(255, 255, 0), 1), x, y - 1, x, y + 1);
					}
				}

				DrawHorzLine(graphics, width, pTag->m_upperM, pTag->m_upperB);
				DrawHorzLine(graphics, width, pTag->m_lowerM, pTag->m_lowerB);
				DrawVertLine(graphics, height, pTag->m_leftM, pTag->m_leftB);
				DrawVertLine(graphics, height, pTag->m_rightM, pTag->m_rightB);
			}


			deleteAprilTags(pTags);
		}

	}

	EndPaint(hWnd, &ps);
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;

		case ID_MarkNormal:
			g_pBitmap->SetPixels();
			InvalidateRect(hWnd, NULL, FALSE);
			break;

		case ID_MarkLeftEdge:
			g_pBitmap->MarkLeftEdge();
			InvalidateRect(hWnd, NULL, FALSE);
			break;

		case ID_MarkBlobs:
			g_pBitmap->MarkBlobs();
			InvalidateRect(hWnd, NULL, FALSE);
			break;

		case ID_MarkRegions:
			g_markRegions = !g_markRegions;
			InvalidateRect(hWnd, NULL, FALSE);
			break;
		case ID_NextImage:
			LoadNextBitmap();
			break;
		case ID_PrevImage:
			LoadPrevBitmap();
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		Paint(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IMAGEPROCESSING));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_IMAGEPROCESSING);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

void computePopcount()
{
	for (int i = 0; i < 256; i++)
	{
		int count = 0;
		int byte = i;
		for (int b = 0; b < 8; b++)
		{
			if (byte & 1)
			{
				count++;
			}
			byte >>= 1;
		}

		char buf[128];
		snprintf(buf, sizeof(buf), "%d,", count);
		OutputDebugString(buf);
		if ((i % 16) == 15)
		{
			OutputDebugString("\n");
		}
	}
}

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//computePopcount();

	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	ULONG_PTR token;
	GdiplusStartupInput	input;

	GdiplusStartup(&token, &input, 0);

	//g_pBitmap = LoadPPmBitmap(_T("images\\image0038.ppm"));
	//g_pBitmap = LoadPPmBitmap(_T("images\\imageRot.ppm"));
	//g_pBitmap = LoadPPmBitmap(_T("images\\imageRotCCW.ppm"));
	//g_pBitmap = LoadPPmBitmap(_T("images\\none1\\none003.ppm"));

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_IMAGEPROCESSING, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	GetFiles("images");
	g_pBitmap = GetBitmap(0);
	InvalidateRect(g_hWnd, 0, false);

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_IMAGEPROCESSING));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	GdiplusShutdown(token);

	return (int)msg.wParam;
}
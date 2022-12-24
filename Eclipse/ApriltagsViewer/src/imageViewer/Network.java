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

package imageViewer;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Timer;
import java.util.TimerTask;

/**
 * 
 * The Network class handles the connection to the image server on port 5801 on the Raspberry Pi
 *
 */

public class Network 
{
	private String m_host;
	private int    m_port;
	Receiver	   m_receiver;
	static final long m_key = 0xaa55aa55;		// Marks the start of a frame
	ImageViewer m_imageViewer = null;
	private PrintStream m_printStream = null;
	
	public class Region
	{
		public	int 		m_tagId;
		public	double		m_distance;
		public  double[][]	m_corners = new double[4][2];
		
		public Region Clone()
		{
			Region ret = new Region();
			
			ret.m_tagId		= m_tagId;
			ret.m_distance	= m_distance;
			
			for (int i = 0 ; i < 4 ; i++)
			{
				for (int j = 0 ; j < 2 ; j++)
				{
					ret.m_corners[i][j] = m_corners[i][j]; 
				}
			}
			
			return(ret);
		}
	}
	
	public class Regions
	{
		public int	m_nRegions	= 0;
		public Region m_regions[] = null;
	}
	
	public class CameraData
	{
		public int m_width  = 640;
		public int m_height = 480;
		
		public int m_whiteDrop 	= 100;
		public int m_minSize		= 24;
		public int m_minWhite		= 4;
		public int m_minBlack		= 4;
		public double m_maxSlope	= 0.1;
		public double m_maxParallel	= 0.2;
		public double m_maxAspect 	= 0.1;
		public int m_shutterSpeed	= 400;
		public int m_gain			= 64;	// ISO for Pi Camera
		public int m_captureRate	= 60;
		public double m_fps			= 30.0;
		public int m_blobCount		= 0;
		public int m_avgWhite		= 0;
		public int m_whiteTarget	= 100;	// Brighness for Pi Camera
		public double m_decimate	= 1;
		public int m_nThreads		= 4;
		public double m_blur		= 0;
		public boolean m_refine	 	= true;
		public boolean m_fastTags	= true;
		public int m_contrast		= 0;
		public boolean m_autoExp	= true;
		public boolean m_piCamera	= false;
		public int m_sampleRegion	= 50;

		public Regions m_regions;
		
		public CameraData Clone()
		{
			CameraData	data = new CameraData();
			
			data.m_width			= m_width;
			data.m_height			= m_height;
			
			data.m_whiteDrop		= m_whiteDrop;
			data.m_minSize			= m_minSize;
			data.m_minWhite			= m_minWhite;
			data.m_minBlack			= m_minBlack;
			data.m_maxSlope			= m_maxSlope;
			data.m_maxParallel		= m_maxParallel;
			data.m_maxAspect		= m_maxAspect;
			data.m_shutterSpeed		= m_shutterSpeed;
			data.m_gain				= m_gain;
			data.m_captureRate		= m_captureRate;
			data.m_fps				= m_fps;
			data.m_blobCount		= m_blobCount;
			data.m_avgWhite			= m_avgWhite;
			data.m_whiteTarget		= m_whiteTarget;
			data.m_decimate			= m_decimate;
			data.m_nThreads			= m_nThreads;
			data.m_fastTags			= m_fastTags;
			data.m_blur				= m_blur;
			data.m_refine			= m_refine;
			data.m_contrast			= m_contrast;
			data.m_autoExp			= m_autoExp;
			data.m_piCamera			= m_piCamera;
			data.m_sampleRegion		= m_sampleRegion;
			
			data.m_regions			= m_regions;
			
			return(data);
		}
	}
	
	CameraData m_cameraData = new CameraData();
	Timer m_watchdogTimer = null;
	boolean m_connected = false;
	long m_lastMessage;
	final static int k_timeout = 5000;
    Socket m_clientSocket = null;
	InputStream m_inputStream = null;
	OutputStream m_outputStream = null;
	
	void CloseConnection()
	{
		System.out.println("CloseConnection");
		
		synchronized(this)
		{
			if (m_printStream != null)
			{
				m_printStream.close();
				m_printStream	= null;
			}
			
			if (m_outputStream != null)
			{
				try {
					m_outputStream.close();
				} catch (IOException e) {
				}
				m_outputStream = null;
			}
			
			if (m_inputStream != null)
			{
				try {
					m_inputStream.close();
				} catch (IOException e) {
				}
				m_inputStream = null;
			}
			
			if (m_clientSocket != null)
			{
				try {
					m_clientSocket.close();
				} catch (IOException e) {
				}
				m_clientSocket = null;
			}
		}
	}
		
	Network(ImageViewer imageViewer)
	{
		m_imageViewer	= imageViewer;
		
		m_watchdogTimer = new Timer();
		m_watchdogTimer.schedule(new TimerTask() {

            @Override
            public void run() {
            	if (m_connected)
            	{
	                SendMessage("k");
	            	System.out.println("WatchDog");
	            	
	            	if (m_lastMessage + k_timeout < System.currentTimeMillis())
	            	{
	            		System.out.println("Network timeout");
	            		CloseConnection();
	            	}
            	}
            }
        }, 1000, 1000);		
	}
	
	public void Connect(String host, int port)
	{
		m_host	= host;
		m_port	= port;
		
		m_receiver	= new Receiver();
		
		(new Thread(m_receiver)).start();
		
	}
	
	public void SendMessage(String message)
	{
		if (m_printStream != null)
		{
			m_printStream.println(message);
		}
	}

	private class Receiver implements Runnable
	{
		private void Sleep(int ms)
		{
			try 
			{
				Thread.sleep(ms);
			}
			catch (InterruptedException ex)
			{
				  
			}
		 }
		
		int Read(InputStream stream, byte[] buf) throws IOException
		{
			int	length	= buf.length;
			int pos		= 0;
			
			while (length > 0)
			{
				int count	= stream.read(buf,  pos,  length);
				
				if (count <= 0)
				{
					break;
				}
				
				pos	+= count;
				length -= count;
				
			}
			
			return(pos);
		}
		
//		struct ImageHeader		//  C++ structure of header for data received
//		{
//			unsigned long	imageSize;				//  0
//			short			width;					//  4
//			short			height;					//  6
//
//			short			whiteDrop;				//  8
//			short			minSize;				// 10
//			short			whiteCnt;				// 12
//			short			minBlack;				// 14
//			short			maxSlope;				// 16
//			short			maxParallel;			// 18
//			short			maxAspect;				// 20
//			short			shutterSpeed;			// 22
//			short			gain;					// 24
//			short			captureRate;			// 26
//			short			fps;					// 28
//			short			blobCount;				// 30
//			short			avgWhite;				// 32
//			short			whiteTarget;			// 34   Brightness
//			short			decimate;				// 36
//			short			nThreads;				// 38
//			short			fast;					// 40
//			short			blur;					// 42
//			short			refine;					// 44
//			short           contrast;               // 46
//			char            autoExposure ;          // 48
//			char            piCamera;               // 49
//			short           sampleRegion;           // 50
//			short           pad;                    // 52
//
//			short			nRegions;				// 54
//			// size 56
//		};	

		private int ProcessHeader(byte[] hdr)
		{
			ByteBuffer	buf 		= ByteBuffer.wrap(hdr).order(ByteOrder.LITTLE_ENDIAN);
			int 		imageSize 	= buf.getInt();
			
			
			m_cameraData.m_width = buf.getShort();
			m_cameraData.m_height = buf.getShort();
			m_cameraData.m_whiteDrop = buf.getShort();
			m_cameraData.m_minSize = buf.getShort();
			m_cameraData.m_minWhite = buf.getShort();
			m_cameraData.m_minBlack = buf.getShort();
			m_cameraData.m_maxSlope = ((double) buf.getShort()) / 100;
			m_cameraData.m_maxParallel = ((double) buf.getShort()) / 100;
			m_cameraData.m_maxAspect = ((double) buf.getShort()) / 100;
			m_cameraData.m_shutterSpeed = buf.getShort();
			m_cameraData.m_gain = buf.getShort();
			m_cameraData.m_captureRate = buf.getShort();
			m_cameraData.m_fps = buf.getShort() / 10.0;
			m_cameraData.m_blobCount = buf.getShort();
			m_cameraData.m_avgWhite = buf.getShort();
			m_cameraData.m_whiteTarget = buf.getShort();
			m_cameraData.m_decimate = buf.getShort() / 10.0;
			m_cameraData.m_nThreads = buf.getShort();
			m_cameraData.m_fastTags = buf.getShort() != 0;
			m_cameraData.m_blur = buf.getShort() / 10.0;
			m_cameraData.m_refine = buf.getShort() != 0;
			m_cameraData.m_contrast = buf.getShort();
			m_cameraData.m_autoExp = buf.get() != 0;
			m_cameraData.m_piCamera = buf.get() != 0;
			m_cameraData.m_sampleRegion = buf.getShort();
			buf.getShort();	// pad
			
			m_cameraData.m_regions = new Regions();
			m_cameraData.m_regions.m_nRegions = buf.getShort();	

//			c++ structures for region data received
//			struct RegionPoint
//			{
//				short	x;
//				short	y;
//			};
//
//			struct RegionData
//			{
//				short		tagId;
//				short		distance;
//				RegionPoint	corners[4];
//			};

			if (m_cameraData.m_regions.m_nRegions != 0)
			{
				m_cameraData.m_regions.m_regions = new Region[m_cameraData.m_regions.m_nRegions];
				
				for (int r = 0 ; r < m_cameraData.m_regions.m_nRegions ; r++)
				{
					Region region = new Region();
					m_cameraData.m_regions.m_regions[r]	= region;
					
					region.m_tagId				= buf.getShort();
					region.m_distance			= buf.getShort() / 10.0;

					for (int i = 0 ; i < 4 ; i++)
					{
						region.m_corners[i][0] = buf.getShort() / 10.0;
						region.m_corners[i][1] = buf.getShort() / 10.0;
					}
				}
			}
			
			return(imageSize);
		}
		
		@Override
		public void run() 
		{
			System.out.println("Receiver thread started");
			while (true)
			{
				System.out.println("Connecting to pi camera");
				
				try
				{
					synchronized(this)
					{
					    m_clientSocket = new Socket(m_host, m_port);
						m_inputStream = m_clientSocket.getInputStream();
						m_outputStream = m_clientSocket.getOutputStream();
						m_printStream = new PrintStream(m_outputStream);
					}
				
					System.out.println("Connected to pi camera");
					
					byte[] hdr = null;	// = new byte[m_hdrSize];
					byte[] key = new byte[4];
					byte[] hdrSize = new byte[4];
					int		headerSize;
					
					m_connected = true;
					m_lastMessage = System.currentTimeMillis();

					while (m_connected)
					{
						int nBytes;
						long keyValue;
						
						nBytes = m_inputStream.read(key);
						
						if (nBytes != 4)
						{
							System.out.println("Invalid key length: " + nBytes);
							break;
						}
						
						keyValue = ByteBuffer.wrap(key).order(ByteOrder.LITTLE_ENDIAN).getInt();
						
						if (keyValue != m_key)
						{
							System.out.println("Invalid key: " + keyValue);
							break;
						}
						
						nBytes = m_inputStream.read(hdrSize);
						
						if (nBytes != 4)
						{
							System.out.println("Invalid header size length: " + nBytes);
							break;
						}
						
						headerSize	= ByteBuffer.wrap(hdrSize).order(ByteOrder.LITTLE_ENDIAN).getInt();
						hdr = new byte[headerSize];
						
						nBytes = Read(m_inputStream, hdr);
						
						if (nBytes != headerSize)
						{
							System.out.println("Incorrect header size: " + nBytes + " != " + headerSize);
							break;
						}
						
						int imageSize = ProcessHeader(hdr);
						
						byte[] image = new byte[imageSize];
						
						nBytes = Read(m_inputStream, image);
						
						if (nBytes != imageSize)
						{
							System.out.println("Cannot read image data: " + nBytes + " != " + imageSize);
							break;
						}
						
						m_lastMessage = System.currentTimeMillis();
						
						m_imageViewer.DisplayImage(image, m_cameraData.Clone());
					}
				}
				catch (Exception ex)
				{
					System.out.println("Receiver: StartServer exception: " + ex);
				}
				
				CloseConnection();
								
				m_connected = false;
				
				Sleep(2000);
			}
		}
	}
}

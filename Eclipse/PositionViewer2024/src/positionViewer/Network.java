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

package positionViewer;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.Socket;
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
	PositionViewer m_positionViewer = null;
	private PrintStream m_printStream = null;
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
		
	Network(PositionViewer imageViewer)
	{
		m_positionViewer	= imageViewer;
		
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
		
		@Override
		public void run() 
		{
			boolean test = false;
			
			System.out.println("Receiver thread started");
			while (true)
			{
				if (test)
				{
					try {
						Thread.sleep(1000);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
					continue;
				}
				
				System.out.println("Connecting to RoboRio");
				
				try
				{
					synchronized(this)
					{
					    m_clientSocket = new Socket(m_host, m_port);
						m_inputStream = m_clientSocket.getInputStream();
						m_outputStream = m_clientSocket.getOutputStream();
						m_printStream = new PrintStream(m_outputStream);
					}
				
					System.out.println("Connected to RoboRio");
					
					m_connected = true;
					m_lastMessage = System.currentTimeMillis();
					
					m_positionViewer.connected();
					
					StringBuilder command = new StringBuilder();

					while (m_connected)
					{
						int ch = m_inputStream.read();
						
						if (ch < 0)
						{
							break;
						}
						
						if (ch == '\n')
						{
							if (command.length() >= 1)
							{
								String cmd = command.toString();
								m_positionViewer.commandReceived(cmd);
//								System.out.println(cmd);
							}
							
							m_lastMessage = System.currentTimeMillis();							
							
							command.setLength(0);
							
						}
						else if (ch >= 0)
						{
							command.append((char) ch);
						}
						else
						{
							Thread.sleep(5);
						}
					}
				}
				catch (Exception ex)
				{
					System.out.println("Receiver: StartServer exception: " + ex);
//					ex.printStackTrace();
				}
				
				CloseConnection();
								
				m_connected = false;
				
				m_positionViewer.disconnected();
				
				Sleep(2000);
			}
		}
	}
}

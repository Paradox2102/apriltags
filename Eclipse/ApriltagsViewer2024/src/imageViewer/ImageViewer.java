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

import java.awt.*;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.EventQueue;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JButton;
import javax.swing.JSpinner;
import javax.swing.SwingUtilities;
import javax.swing.border.AbstractBorder;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.event.ChangeListener;

//import ApriltagsCamera.PiCamera;
import imageViewer.Network.Regions;

import javax.swing.event.ChangeEvent;
import javax.swing.SpinnerNumberModel;
import javax.swing.JCheckBox;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import javax.swing.SwingConstants;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;

public class ImageViewer 
{
	static int ImageWidth	= 640;
	static int ImageHeight = 480;

	@SuppressWarnings("serial")
	public class RoundedLineBorder extends AbstractBorder {
	    int lineSize, cornerSize;
	    Paint fill;
	    Stroke stroke;
	    private Object aaHint;

	    public RoundedLineBorder(Paint fill, int lineSize, int cornerSize) {
	        this.fill = fill;
	        this.lineSize = lineSize;
	        this.cornerSize = cornerSize;
	        stroke = new BasicStroke(lineSize);
	    }
	    public RoundedLineBorder(Paint fill, int lineSize, int cornerSize, boolean antiAlias) {
	        this.fill = fill;
	        this.lineSize = lineSize;
	        this.cornerSize = cornerSize;
	        stroke = new BasicStroke(lineSize);
	        aaHint = antiAlias? RenderingHints.VALUE_ANTIALIAS_ON: RenderingHints.VALUE_ANTIALIAS_OFF;
	    }

	    @Override
	    public Insets getBorderInsets(Component c, Insets insets) {
	        int size = Math.max(lineSize, cornerSize);
	        if(insets == null) insets = new Insets(size, size, size, size);
	        else insets.left = insets.top = insets.right = insets.bottom = size;
	        return insets;
	    }

	    @Override
	    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
	        Graphics2D g2d = (Graphics2D)g;
	        Paint oldPaint = g2d.getPaint();
	        Stroke oldStroke = g2d.getStroke();
	        Object oldAA = g2d.getRenderingHint(RenderingHints.KEY_ANTIALIASING);
	        try {
	            g2d.setPaint(fill!=null? fill: c.getForeground());
	            g2d.setStroke(stroke);
	            if(aaHint != null) g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, aaHint);
	            int off = lineSize >> 1;
	            g2d.drawRoundRect(x+off, y+off, width-lineSize, height-lineSize, cornerSize, cornerSize);
	        }
	        finally {
	            g2d.setPaint(oldPaint);
	            g2d.setStroke(oldStroke);
	            if(aaHint != null) g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, oldAA);
	        }
	    }
	}
	
	private Network m_network = new Network(this);
	private CustomPanel m_camera1;
	private CustomPanel m_camera2;
	private BufferedImage m_image = null;
	private JSpinner	m_shutterSpeed 		= null;
	private JSpinner	m_captureRate		= null;
	private JSpinner	m_sampleRegion		= null;
	private JSpinner	m_contrast			= null;
	private JSpinner	m_brightness		= null;
	private JSpinner	m_exposure		 		= null;
	private JSpinner	m_whiteDrop		= null;
	private JSpinner	m_minBlack			= null;
	private JSpinner	m_minSize			= null;
	private JSpinner	m_maxSlope			= null;
	private JSpinner	m_maxParallel		= null;
	private JSpinner	m_maxAspect			= null;
	private JSpinner	m_decimate			= null;
	private JSpinner	m_blur				= null;
	private JSpinner	m_threads			= null;
	private JLabel 		m_fps1				= null;
	private JLabel		m_fps2				= null;
	private JCheckBox 	m_fast				= null;
//	private JCheckBox	m_autoExposure		= null;
	private JCheckBox	m_refine			= null;
	Network.CameraData 	m_data 				= null;
	int g_selectProfile = -1;
	boolean g_profileChanged = false;
	boolean m_light = false;	    
   	
	@SuppressWarnings("serial")
	private class CustomPanel extends JPanel
	{
		private Network.Regions m_regions = null;
		@SuppressWarnings("unused")
		private int m_horzTarget = 0;
		@SuppressWarnings("unused")
		private int m_vertTarget = 0;
		
	    public CustomPanel() {
	        setBorder(BorderFactory.createLineBorder(Color.black));
	    }

	    public Dimension getPreferredSize() {
	        return new Dimension(250,200);
	    }
	    
	    public void SetRegions(Network.Regions regions, int vertTarget, int horzTarget)
	    {
	    	m_regions	= regions;
	    	m_horzTarget	= horzTarget;
	    	m_vertTarget	= vertTarget;
	    }
	    
	    int viewWidth;
	    int viewHeight;

	    double m_scale = 1;
	    
	    int s(double v)
	    {
	    	return (int) (v * m_scale);
	    }

	    public void paintComponent(Graphics g) 
	    {
	        super.paintComponent(g);       
	        
	        if (m_image != null && m_data != null)
	        {
	        	viewWidth	= getWidth();
	        	viewHeight  = getHeight();
	        	ImageWidth = m_data.m_width;
	        	ImageHeight = m_data.m_height;
	        	
	        	double sx = (double) viewWidth / ImageWidth;
	        	double sy = (double) viewHeight / ImageHeight;
	        	
	        	m_scale = (sx < sy) ? sx : sy;   
	        	
//	        	int wid = m_image.getWidth();
//	        	int hgt = m_image.getHeight();

	        	g.drawImage(m_image, 0, 0, s(ImageWidth), s(ImageHeight), null);
	        	
	        	if (m_regions != null)
	        	{
					Graphics2D g2 = (Graphics2D)g;
					g2.setRenderingHint(
					     RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
						
					Font font = new Font("Sanserif", Font.BOLD, 16);
					g2.setFont(font);
	        		g2.setStroke(new BasicStroke(2));
	        		
	        		for (int i = 0 ; i < m_regions.m_nRegions ; i++)
	        		{
	        			Network.Region region = m_regions.m_regions[i];
	        			
    					g.setColor(new Color(0.0F, 0.5F, 0.0F));
    					
						g2.drawString("" + region.m_tagId, s(region.m_corners[0][0]) + 4, s(region.m_corners[0][1]) - 4);
						g2.drawString(String.format("%.1f",  region.m_distance), s(region.m_corners[3][0]), s(region.m_corners[3][1]+50));
	        			
						g.setColor(Color.green);
						
	        			for (int j = 0 ; j < 3 ; j++)
	        			{
	        				g.drawLine(s(region.m_corners[j][0]), s(region.m_corners[j][1]), s(region.m_corners[j+1][0]), s(region.m_corners[j+1][1]));
	        			}
        				g.drawLine(s(region.m_corners[3][0]), s(region.m_corners[3][1]), s(region.m_corners[0][0]), s(region.m_corners[0][1]));
	        		}
	        	}
	        }
	    } 
	    
	    public void SetImage(BufferedImage image)
	    {
	    	m_image	= image;
	    	
	    	repaint();
	    }
	    
	    int countValid(Regions regions)
	    {
	    	int count = 0;
	    	
	    	for (int i = 0 ; i < regions.m_nRegions ; i++)
	    	{
	    		if (regions.m_regions[i].m_tagId >= 0)
	    		{
	    			count++;
	    		}
	    	}
	    	
	    	return(count);
	    }
	    
		Runnable doSetCameraData = new Runnable() 
		{
			public void run() 
			{
				m_shutterSpeed.setValue(m_data.m_shutterSpeed);
				m_captureRate.setValue(m_data.m_captureRate);
				m_brightness.setValue(m_data.m_whiteTarget);
				m_exposure.setValue(m_data.m_exposure);
				
				if (m_contrast != null)
				{
					m_contrast.setValue(m_data.m_contrast);
				}
				
				m_whiteDrop.setValue(m_data.m_whiteDrop);
				m_minBlack.setValue(m_data.m_minBlack);
				m_maxSlope.setValue(m_data.m_maxSlope);
				m_maxParallel.setValue(m_data.m_maxParallel);
				m_maxAspect.setValue(m_data.m_maxAspect);
				
				if (m_data.m_cameraNo == 0)
				{
					m_fps1.setText(String.format("FPS:%.1f (%d,%d) B:%d C:%d V:%d AW:%d dt:%d mc:%d mdt:%d", m_data.m_fps, m_data.m_width, m_data.m_height,
									m_data.m_blobCount, m_data.m_regions.m_nRegions, countValid(m_data.m_regions), m_data.m_avgWhite, deltaTime, maxCount, maxDeltaTime));
				}
				else
				{
					m_fps2.setText(String.format("FPS:%.1f (%d,%d) B:%d C:%d V:%d AW:%d dt:%d mc:%d mdt:%d", m_data.m_fps, m_data.m_width, m_data.m_height,
							m_data.m_blobCount, m_data.m_regions.m_nRegions, countValid(m_data.m_regions), m_data.m_avgWhite, deltaTime, maxCount, maxDeltaTime));					
				}
				m_decimate.setValue(m_data.m_decimate);
				m_fast.setSelected(m_data.m_fastTags);
				m_threads.setValue(m_data.m_nThreads);
				m_blur.setValue(m_data.m_blur);
				m_refine.setSelected(m_data.m_refine);
		     }
		};
		
		long lastTime = 0;
		int deltaTime = 0;
		int maxDeltaTime = 0;
		int maxCount = 0;
		 
	    private void SetCameraData(Network.CameraData data)
	    {
	    	if (data.m_regions != null && data.m_regions.m_nRegions > 1)
	    	{
	    		for (int i = 0 ; i < data.m_regions.m_nRegions ; i++)
	    		{
	    			Network.Region region = data.m_regions.m_regions[i];
//	    			System.out.println(String.format("tag=%d,x=%f,y=%f", region.m_tagId, region.m_corners[0][0], region.m_corners[0][1]));
	    		}
	    	}
	    	
	    	long time = System.currentTimeMillis();
	    	deltaTime = (int) (time - lastTime);
	    	if (lastTime != 0)
	    	{
	    		if (deltaTime > maxDeltaTime)
		    	{
		    		maxDeltaTime = deltaTime;
		    	}
	    		if (deltaTime > 200)
	    		{
	    			maxCount++;
	    		}
	    	}
	    	lastTime = time;

	    	
	    	synchronized(this)
	    	{
	    		m_data	= data;
	    		if (data.m_cameraNo == 0)
	    		{
	    			m_camera1.SetRegions(data.m_regions, 0, 0);
	    		}
	    		else
	    		{
	    			m_camera2.SetRegions(data.m_regions,  0,  0);
	    		}
	    	}

	    	SwingUtilities.invokeLater(doSetCameraData);
	    }
		
	}

	private JFrame frame;

	/**
	 * Launch the application.
	 */
	public static void main(final String[] args) {
		EventQueue.invokeLater(new Runnable() {
			public void run() {
				if (args.length != 1)
				{
					JOptionPane.showMessageDialog(null, "Must specify ip address");
				}
				else
				{
					try {
						ImageViewer window = new ImageViewer(args[0]);
						window.frame.setVisible(true);
					} catch (Exception e) {
						e.printStackTrace();
					}
				}
			}
		});
	}

	/**
	 * Create the application.
	 */
	public ImageViewer(String args)
	{
		initialize(args);
	}
	
	public void DisplayImage(byte[] image, Network.CameraData cameraData)
	{
		ByteArrayInputStream bis = new ByteArrayInputStream(image);
		
        try 
        {
            BufferedImage img = ImageIO.read(bis);
            
            if (cameraData.m_cameraNo == 0)
            {
	            m_camera1.SetImage(img);
	            m_camera1.SetCameraData(cameraData);
            }
            else
            {
	            m_camera2.SetImage(img);
	            m_camera2.SetCameraData(cameraData);            	
            }
        } 
        catch (IOException e) 
        {
        	System.out.println("Exception loading image: " + e);
        }
	}
	
	private void SendCommand(String command)
	{
		System.out.println("SendCommand: " + command);
		m_network.SendMessage(command);
	}
	
	private void SpinnerChanged(JSpinner spinner, int curValue, String command)
	{
		int	value = (int) spinner.getValue();
//		Object obj = spinner.getValue();
//		
//		if (obj instanceof Double)
//		{
//			double v = (Double) obj;
//			value = (int) (v * 100);
//		}
//		else
//		{
//			value = ((Integer) obj);
//		}
		
		if (value != curValue)
		{
			SendCommand(command + value);
		}
		
	}

	private void SpinnerChanged(JSpinner spinner, double curValue, String command)
	{
		double	value	= (double) spinner.getValue();
		
		if (value != curValue)
		{
			SendCommand(command + value);
		}		
	}
	
//	 PiCamera m_camera = new PiCamera();
	 
	 boolean m_controlsInit = false;
	 
//	 private void initBWCameraControls()
//	 {
//		if (!m_controlsInit)
//		{
//			int dy = 272-35;
//			
//			JLabel lblCameraSettings = new JLabel("Camera Settings");
//			lblCameraSettings.setBounds(655, 10+dy, 104, 20);
//			lblCameraSettings.setBackground(new Color(238, 238, 238));
//			lblCameraSettings.setOpaque(true);
//			lblCameraSettings.setHorizontalAlignment(SwingConstants.CENTER);
//			lblCameraSettings.setVerticalAlignment(SwingConstants.TOP);
//			frame.getContentPane().add(lblCameraSettings);
//			
//			JLabel lblCameraSettingsBox = new JLabel("");
//			lblCameraSettingsBox.setBounds(648, 20+dy, 240, 125);
//			lblCameraSettingsBox.setHorizontalAlignment(SwingConstants.LEFT);
//			lblCameraSettingsBox.setVerticalAlignment(SwingConstants.TOP);
//			lblCameraSettingsBox.setBorder(new RoundedLineBorder(Color.black, 2, 10, true));
//			frame.getContentPane().add(lblCameraSettingsBox);
//			
//			JLabel lblShutterspeed = new JLabel("Shutter Speed");
//			lblShutterspeed.setBounds(665, 32+dy, 100, 14);
//			frame.getContentPane().add(lblShutterspeed);
//			
//			m_shutterSpeed = new JSpinner();
//			m_shutterSpeed.setModel(new SpinnerNumberModel(Integer.valueOf(300), Integer.valueOf(1), Integer.valueOf(855), Integer.valueOf(50)));
//			m_shutterSpeed.addChangeListener(new ChangeListener() 
//			{
//				public void stateChanged(ChangeEvent arg0) 
//				{
//					SpinnerChanged(m_shutterSpeed, m_data.m_shutterSpeed, "s ");
//				}
//			});
//			m_shutterSpeed.setBounds(665, 46+dy, 90, 20);
//			frame.getContentPane().add(m_shutterSpeed);
//			
//			JLabel lblGain = new JLabel("Gain");
//			lblGain.setBounds(775, 32+dy, 100, 14);
//			frame.getContentPane().add(lblGain);
//			
//			m_gain = new JSpinner();
//			m_gain.setModel(new SpinnerNumberModel(Integer.valueOf(400), Integer.valueOf(0), Integer.valueOf(800), Integer.valueOf(10)));
//			m_gain.addChangeListener(new ChangeListener() 
//			{
//				public void stateChanged(ChangeEvent arg0) 
//				{
//					SpinnerChanged(m_gain, m_data.m_gain, "g ");
//				}
//			});
//			m_gain.setBounds(775, 46+dy, 90, 20);
//			frame.getContentPane().add(m_gain);
//			
//			JLabel lblCaptureRate = new JLabel("Capture Rate");
//			lblCaptureRate.setBounds(665, 66+dy, 100, 14);
//			frame.getContentPane().add(lblCaptureRate);
//			
//			m_captureRate = new JSpinner();
//			m_captureRate.setModel(new SpinnerNumberModel(Integer.valueOf(60), Integer.valueOf(1), Integer.valueOf(140), Integer.valueOf(10)));
//			m_captureRate.addChangeListener(new ChangeListener() 
//			{
//				public void stateChanged(ChangeEvent arg0) 
//				{
//					SpinnerChanged(m_captureRate, m_data.m_captureRate, "c ");
//				}
//			});
//			m_captureRate.setBounds(665, 80+dy, 90, 20);
//			frame.getContentPane().add(m_captureRate);
//	
//			JLabel lblWhiteTarget = new JLabel("White Target");
//			lblWhiteTarget.setBounds(775, 66+dy, 100, 14);
//			frame.getContentPane().add(lblWhiteTarget);
//			
//			m_whiteTarget = new JSpinner();
//			m_whiteTarget.setModel(new SpinnerNumberModel(Integer.valueOf(100), Integer.valueOf(0), Integer.valueOf(255), Integer.valueOf(10)));
//			m_whiteTarget.addChangeListener(new ChangeListener() 
//			{
//				public void stateChanged(ChangeEvent arg0) 
//				{
//					SpinnerChanged(m_whiteTarget, m_data.m_whiteTarget, "T ");
//				}
//			});
//			m_whiteTarget.setBounds(775, 80+dy, 90, 20);
//			frame.getContentPane().add(m_whiteTarget);
//			
//			JLabel lblSampleRegion = new JLabel("Sample Region");
//			lblSampleRegion.setBounds(665, 101+dy, 100, 14);
//			frame.getContentPane().add(lblSampleRegion);
//			
//			m_sampleRegion = new JSpinner();
//			m_sampleRegion.setModel(new SpinnerNumberModel(Integer.valueOf(50), Integer.valueOf(1), Integer.valueOf(100), Integer.valueOf(10)));
//			m_sampleRegion.addChangeListener(new ChangeListener() 
//			{
//				public void stateChanged(ChangeEvent arg0) 
//				{
//					SpinnerChanged(m_sampleRegion, m_data.m_sampleRegion, "R ");
//				}
//			});
//			m_sampleRegion.setBounds(665, 115+dy, 90, 20);
//			frame.getContentPane().add(m_sampleRegion);
//	
//			frame.invalidate();
//			frame.repaint();
//			
//			m_controlsInit = true;
//		}
//	 }
	 
	 private void initPiCameraControls()
	 {
		if (!m_controlsInit)
		{
			int dy = 234;
			int dx = 640;
			
			JLabel lblCameraSettings = new JLabel("Camera Settings");
			lblCameraSettings.setBounds(655 - dx, 10+dy, 104, 20);
			lblCameraSettings.setBackground(new Color(238, 238, 238));
			lblCameraSettings.setOpaque(true);
			lblCameraSettings.setHorizontalAlignment(SwingConstants.CENTER);
			lblCameraSettings.setVerticalAlignment(SwingConstants.TOP);
			frame.getContentPane().add(lblCameraSettings);
			
			JLabel lblCameraSettingsBox = new JLabel("");
			lblCameraSettingsBox.setBounds(648 - dx, 20+dy, 240, 125);
			lblCameraSettingsBox.setHorizontalAlignment(SwingConstants.LEFT);
			lblCameraSettingsBox.setVerticalAlignment(SwingConstants.TOP);
			lblCameraSettingsBox.setBorder(new RoundedLineBorder(Color.black, 2, 10, true));
			frame.getContentPane().add(lblCameraSettingsBox);
			
			JLabel lblShutterspeed = new JLabel("Shutter Speed");
			lblShutterspeed.setBounds(665 - dx, 32+dy, 100, 14);
			frame.getContentPane().add(lblShutterspeed);
			
			m_shutterSpeed = new JSpinner();
			m_shutterSpeed.setModel(new SpinnerNumberModel(Integer.valueOf(20), Integer.valueOf(1), Integer.valueOf(500), Integer.valueOf(1)));
			m_shutterSpeed.addChangeListener(new ChangeListener() 
			{
				public void stateChanged(ChangeEvent arg0) 
				{
					SpinnerChanged(m_shutterSpeed, m_data.m_shutterSpeed, "s ");
				}
			});
			m_shutterSpeed.setBounds(665 - dx, 46+dy, 90, 20);
			frame.getContentPane().add(m_shutterSpeed);
			
			JLabel lblExposure = new JLabel("Exposure");
			lblExposure.setBounds(775 - dx, 32+dy, 100, 14);
			frame.getContentPane().add(lblExposure);
			
			m_exposure = new JSpinner();
			m_exposure.setModel(new SpinnerNumberModel(Integer.valueOf(0), Integer.valueOf(-400), Integer.valueOf(400), Integer.valueOf(10)));
			m_exposure.addChangeListener(new ChangeListener() 
			{
				public void stateChanged(ChangeEvent arg0) 
				{
					SpinnerChanged(m_exposure, m_data.m_exposure, "e ");
				}
			});
			m_exposure.setBounds(775 - dx, 46+dy, 90, 20);
			frame.getContentPane().add(m_exposure);
			
			JLabel lblCaptureRate = new JLabel("Capture Rate");
			lblCaptureRate.setBounds(665 - dx, 66+dy, 100, 14);
			frame.getContentPane().add(lblCaptureRate);
			
			m_captureRate = new JSpinner();
			m_captureRate.setModel(new SpinnerNumberModel(Integer.valueOf(50), Integer.valueOf(1), Integer.valueOf(100), Integer.valueOf(5)));
			m_captureRate.addChangeListener(new ChangeListener() 
			{
				public void stateChanged(ChangeEvent arg0) 
				{
					SpinnerChanged(m_captureRate, m_data.m_captureRate, "c ");
				}
			});
			m_captureRate.setBounds(665 - dx, 80+dy, 90, 20);
			frame.getContentPane().add(m_captureRate);
	
			JLabel lblBrightness = new JLabel("Brightness");
			lblBrightness.setBounds(775 - dx, 66+dy, 100, 14);
			frame.getContentPane().add(lblBrightness);
			
			m_brightness = new JSpinner();
			m_brightness.setModel(new SpinnerNumberModel(Integer.valueOf(0), Integer.valueOf(-100), Integer.valueOf(100), Integer.valueOf(5)));
			m_brightness.addChangeListener(new ChangeListener() 
			{
				public void stateChanged(ChangeEvent arg0) 
				{
					SpinnerChanged(m_brightness, m_data.m_whiteTarget, "T ");
				}
			});
			m_brightness.setBounds(775 - dx, 80+dy, 90, 20);
			frame.getContentPane().add(m_brightness);
					
			JLabel lblContrast = new JLabel("Contrast");
			lblContrast.setBounds(665 - dx, 100+dy, 100, 14);
			frame.getContentPane().add(lblContrast);
			
			m_contrast = new JSpinner();
			m_contrast.setModel(new SpinnerNumberModel(Integer.valueOf(100), Integer.valueOf(0), Integer.valueOf(300), Integer.valueOf(5)));
			m_contrast.addChangeListener(new ChangeListener() 
			{
				public void stateChanged(ChangeEvent arg0) 
				{
					SpinnerChanged(m_contrast, m_data.m_contrast, "C ");
				}
			});
			m_contrast.setBounds(665 - dx, 114+dy, 90, 20);
			frame.getContentPane().add(m_contrast);
			
//			m_autoExposure = new JCheckBox("Auto Exposure");
//			m_autoExposure.addActionListener(new ActionListener() {
//				public void actionPerformed(ActionEvent arg0) {
//					SendCommand(String.format("a %d", m_autoExposure.isSelected() ? 1 : 0));				
//				}
//			});
//			m_autoExposure.setBounds(773 - dx, 114+dy, 110, 23);
//			frame.getContentPane().add(m_autoExposure);
//			m_autoExposure.setSelected(true);

			
			frame.invalidate();
			frame.repaint();
			
			m_controlsInit = true;
		}
	 }

	/**
	 * Initialize the contents of the frame.
	 */
	private void initialize(String host) {
		frame = new JFrame();
		frame.setBounds(100, 100, 1260, 568);
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.getContentPane().setLayout(null);
		
//		m_camera.connect("192.168.1.50", 5800);		// Testing
		
		JButton btnNewButton = new JButton("New button");
		btnNewButton.setBounds(0, 0, 640, 480);
		
		m_camera1 = new CustomPanel();
		m_camera1.addMouseListener(new MouseAdapter() 
		{
			@Override
			public void mouseReleased(MouseEvent event) 
			{
				int	x	= event.getX();
				int	y	= event.getY();
				
				int displayWidth = m_camera1.getWidth();
				int displayHeight = m_camera1.getHeight();
				
				x = (x * ImageWidth) / displayWidth;
				y = (y * ImageHeight) / displayHeight;

				if (x < 0)
				{
					x = 0;
				}
				else if (x >= ImageWidth)
				{
					x	= ImageWidth - 1;
				}

				if (y < 0)
				{
					y = 0;
				}
				else if (y >= ImageHeight)
				{
					y	= ImageHeight - 1;
				}

				m_network.SendMessage("t " + x + " " + y);
			}
		});
		int view1x = 260;
		int view1y = 10;
		int view1wid = 480;
		int view1hgt = 360;
		m_camera1.setBounds(view1x, view1y, view1wid, view1hgt);
		frame.getContentPane().add(m_camera1);
		
		m_camera2 = new CustomPanel();
		int view2x = view1x + view1wid + 10;
//		int view2y = 10;
//		int view1wid = 480;
//		int view1hgt = 360;
		m_camera2.setBounds(view2x, view1y, view1wid, view1hgt);
		frame.getContentPane().add(m_camera2);
		
//		initPiCameraControls();
//		initBWCameraControls();
		
		int dy = -80;
		int dx = 640;
		
		JLabel lblFastTags = new JLabel("Fast Apriltags");
		lblFastTags.setBounds(655 - dx, dy+90, 104, 20);
		lblFastTags.setBackground(new Color(238, 238, 238));
		lblFastTags.setOpaque(true);
		lblFastTags.setHorizontalAlignment(SwingConstants.CENTER);
		lblFastTags.setVerticalAlignment(SwingConstants.TOP);
		frame.getContentPane().add(lblFastTags);
		
		JLabel lblFastTagsbox = new JLabel("");
		lblFastTagsbox.setBounds(648 - dx, dy+100, 240, 125);
		lblFastTagsbox.setHorizontalAlignment(SwingConstants.LEFT);
		lblFastTagsbox.setVerticalAlignment(SwingConstants.TOP);
		lblFastTagsbox.setBorder(new RoundedLineBorder(Color.black, 2, 10, true));
		frame.getContentPane().add(lblFastTagsbox);
		
		JLabel lblBlackColor = new JLabel("White Drop");
		lblBlackColor.setBounds(665 - dx, dy+112, 100, 14);
		frame.getContentPane().add(lblBlackColor);
		
		m_whiteDrop = new JSpinner();
		m_whiteDrop.setModel(new SpinnerNumberModel(Integer.valueOf(80), Integer.valueOf(0), Integer.valueOf(100), Integer.valueOf(5)));
		m_whiteDrop.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_whiteDrop, m_data.m_whiteDrop, "W ");
			}
		});
		m_whiteDrop.setBounds(665 - dx, dy+126, 90, 20);
		frame.getContentPane().add(m_whiteDrop);
		
		JLabel lblMinSize = new JLabel("Min Size");
		lblMinSize.setBounds(775 - dx, dy+112, 100, 14);
		frame.getContentPane().add(lblMinSize);
		
		m_minSize = new JSpinner();
		m_minSize.setModel(new SpinnerNumberModel(Integer.valueOf(32), Integer.valueOf(4), Integer.valueOf(100), Integer.valueOf(1)));
		m_minSize.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_minSize, m_data.m_minSize, "m ");
			}
		});
		m_minSize.setBounds(775 - dx, dy+126, 90, 20);
		frame.getContentPane().add(m_minSize);

		JLabel lblMinBlack = new JLabel("Min Black");
		lblMinBlack.setBounds(665 - dx, dy+146, 100, 14);
		frame.getContentPane().add(lblMinBlack);
		
		m_minBlack = new JSpinner();
		m_minBlack.setModel(new SpinnerNumberModel(Integer.valueOf(4), Integer.valueOf(1), Integer.valueOf(100), Integer.valueOf(1)));
		m_minBlack.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_minBlack, m_data.m_minBlack, "B ");
			}
		});
		m_minBlack.setBounds(665 - dx, dy+160, 90, 20);
		frame.getContentPane().add(m_minBlack);

		JLabel lblmaxSlope = new JLabel("Max Slope");
		lblmaxSlope.setBounds(665 - dx, dy+180, 100, 14);
		frame.getContentPane().add(lblmaxSlope);
		
		m_maxSlope = new JSpinner();
		m_maxSlope.setModel(new SpinnerNumberModel(Double.valueOf(0.1), Double.valueOf(0), Double.valueOf(1), Double.valueOf(0.01)));
		m_maxSlope.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_maxSlope, m_data.m_maxSlope, "S ");
			}
		});
		m_maxSlope.setBounds(665 - dx, dy+194, 90, 20);
		frame.getContentPane().add(m_maxSlope);
		
		JLabel lblmaxParallel = new JLabel("Max Parallel");
		lblmaxParallel.setBounds(775 - dx, dy+180, 100, 14);
		frame.getContentPane().add(lblmaxParallel);
		
		m_maxParallel = new JSpinner();
		m_maxParallel.setModel(new SpinnerNumberModel(Double.valueOf(0.2), Double.valueOf(0), Double.valueOf(1), Double.valueOf(0.01)));
		m_maxParallel.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_maxParallel, m_data.m_maxParallel, "P ");
			}
		});
		m_maxParallel.setBounds(775 - dx, dy+194, 90, 20);
		frame.getContentPane().add(m_maxParallel);
		
		JLabel lblmaxAspect = new JLabel("Max Aspect");
		lblmaxAspect.setBounds(775 - dx, dy+146, 100, 14);
		frame.getContentPane().add(lblmaxAspect);
		
		m_maxAspect = new JSpinner();
		m_maxAspect.setModel(new SpinnerNumberModel(Double.valueOf(0.1), Double.valueOf(0), Double.valueOf(1), Double.valueOf(0.01)));
		m_maxAspect.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_maxAspect, m_data.m_maxAspect, "A ");
			}
		});
		m_maxAspect.setBounds(775 - dx, dy+160, 90, 20);
		frame.getContentPane().add(m_maxAspect);
		
		dy += 270 - 35;
		
		JLabel lblAprilTags = new JLabel("Apriltags");
		lblAprilTags.setBounds(655 - dx, dy-8, 70, 20);
		lblAprilTags.setBackground(new Color(238, 238, 238));
		lblAprilTags.setOpaque(true);
		lblAprilTags.setHorizontalAlignment(SwingConstants.CENTER);
		lblAprilTags.setVerticalAlignment(SwingConstants.TOP);
		frame.getContentPane().add(lblAprilTags);
		
		JLabel lblAprilTagsBox = new JLabel("");
		lblAprilTagsBox.setBounds(648 - dx, dy, 240, 90);
		lblAprilTagsBox.setHorizontalAlignment(SwingConstants.LEFT);
		lblAprilTagsBox.setVerticalAlignment(SwingConstants.TOP);
		lblAprilTagsBox.setBorder(new RoundedLineBorder(Color.black, 2, 10, true));
		frame.getContentPane().add(lblAprilTagsBox);
		
		JLabel lblDecimate = new JLabel("Decimate");
		lblDecimate.setBounds(665 - dx, dy+10, 100, 14);
		frame.getContentPane().add(lblDecimate);
		
		m_decimate = new JSpinner();
		m_decimate.setModel(new SpinnerNumberModel(Double.valueOf(1), Double.valueOf(1), Double.valueOf(16), Double.valueOf(1)));
		m_decimate.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_decimate, m_data.m_decimate, "d ");
			}
		});
		m_decimate.setBounds(665 - dx, dy+25, 90, 20);
		frame.getContentPane().add(m_decimate);

		JLabel lblThreads = new JLabel("# Threads");
		lblThreads.setBounds(775 - dx, dy+10, 100, 14);
		frame.getContentPane().add(lblThreads);
		
		m_threads = new JSpinner();
		m_threads.setModel(new SpinnerNumberModel(Integer.valueOf(1), Integer.valueOf(1), Integer.valueOf(4), Integer.valueOf(1)));
		m_threads.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_threads, m_data.m_nThreads, "n ");
			}
		});
		m_threads.setBounds(775 - dx, dy+25, 90, 20);
		frame.getContentPane().add(m_threads);
		
		JLabel lblBlur = new JLabel("Blur");
		lblBlur.setBounds(665 - dx, dy+45, 100, 14);
		frame.getContentPane().add(lblBlur);
		
		m_blur = new JSpinner();
		m_blur.setModel(new SpinnerNumberModel(Double.valueOf(0), Double.valueOf(0), Double.valueOf(20), Double.valueOf(0.5)));
		m_blur.addChangeListener(new ChangeListener() 
		{
			public void stateChanged(ChangeEvent arg0) 
			{
				SpinnerChanged(m_blur, m_data.m_blur, "x ");
			}
		});
		m_blur.setBounds(665 - dx, dy+60, 90, 20);
		frame.getContentPane().add(m_blur);

		
		m_refine = new JCheckBox("Refine Edges");
		m_refine.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				SendCommand(String.format("r %d", m_refine.isSelected() ? 1 : 0));				
			}
		});
		m_refine.setBounds(775 - dx, dy+60, 133, 23);
		frame.getContentPane().add(m_refine);
		m_refine.setSelected(true);
		
		m_fps1 = new JLabel("FPS:");
		m_fps1.setBounds(view1x, view1y+view1hgt, 400, 14);
		frame.getContentPane().add(m_fps1);
		
		m_fps2 = new JLabel("FPS:");
		m_fps2.setBounds(view2x, view1y+view1hgt, 400, 14);
		frame.getContentPane().add(m_fps2);
		
		initPiCameraControls();
		
		JButton btnSave = new JButton("Save");
		btnSave.addActionListener(new ActionListener() 
		{
			public void actionPerformed(ActionEvent arg0) 
			{
				SendCommand("w");
			}
		});
		btnSave.setBounds(790, 501, 100, 23);
		frame.getContentPane().add(btnSave);
		
		JButton btnSaveImage = new JButton("Save Image");
		btnSaveImage.addActionListener(new ActionListener() 
		{
			public void actionPerformed(ActionEvent arg0) 
			{
				SendCommand("I");
			}
		});
		btnSaveImage.setBounds(680, 501, 100, 23);
		frame.getContentPane().add(btnSaveImage);
		
		m_fast = new JCheckBox("Fast Apriltags");
		m_fast.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				SendCommand(String.format("F %d", m_fast.isSelected() ? 1 : 0));				
			}
		});
		m_fast.setBounds(773 - dx, 420, 133, 23);
		frame.getContentPane().add(m_fast);
		m_fast.setSelected(true);

		try 
		{
			PrintWriter log	= new PrintWriter("Test.txt");
			log.println("Now is the time");
			log.close();
		} 
		catch (FileNotFoundException e) 
		{
			e.printStackTrace();
		}
		
		System.out.println("host = " + host);
		
		m_network.Connect(host, 5801);
	}
}

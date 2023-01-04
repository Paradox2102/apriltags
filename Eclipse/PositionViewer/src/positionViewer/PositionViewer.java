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

import java.awt.*;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.EventQueue;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JButton;
import javax.swing.border.AbstractBorder;
import javax.swing.JOptionPane;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;

public class PositionViewer 
{
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
	
	class Position
	{
		public int m_x;
		public int m_y;
		
		public Position(int x, int y)
		{
			m_x = x;
			m_y = y;
		}
	}
	
	@SuppressWarnings("serial")
	public class ArrayListExt<E> extends ArrayList<E> {

		  public void removeRange(int fromIndex, int toIndex) {
		    super.removeRange(fromIndex, toIndex);
		  }

		}
	
	private Network m_network = new Network(this);
	private CustomPanel m_panel;
	private int m_yaw = 0;
	private int m_xPos = 0;
	private int m_yPos = 0;
	private ArrayListExt<Position> m_posList = new ArrayListExt<Position>();
	private static int m_maxList = 1000;
	private Object m_lock = new Object();
	private boolean m_visible = false;
	    
   	
	@SuppressWarnings("serial")
	private class CustomPanel extends JPanel
	{
		private BufferedImage m_image = null;
		
	    public CustomPanel() {
	        setBorder(BorderFactory.createLineBorder(Color.black));

			try {
			    m_image = ImageIO.read(new File("field.jpg"));
			} catch (IOException e) {
			}
			
	    }

	    public Dimension getPreferredSize() {
	        return new Dimension(250,200);
	    }
	    
	    int viewWidth;
	    int viewHeight;

	    double m_scale = 1;
	    
	    int s(double v)
	    {
	    	return (int) (v * m_scale);
	    }
	    
	    int m_fieldLength = 0;
	    int m_fieldWidth = 0;
	    
	    public void computeScale()
	    {
        	int imageWidth = m_image.getWidth();
        	int imageHeight = m_image.getHeight();
        	viewWidth	= getWidth();
        	viewHeight  = getHeight();
        	
        	double sx = (double) viewWidth / imageWidth;
        	double sy = (double) viewHeight / imageHeight;
        	
        	m_scale = (sx < sy) ? sx : sy;
        	m_fieldLength = s(imageWidth);
        	m_fieldWidth = s(imageHeight);	    	
	    }

	    public void paintComponent(Graphics g) 
	    {
	        super.paintComponent(g);       

        	g.drawImage(m_image, 0, 0, m_fieldLength, m_fieldWidth, null);
        
        	g.setColor(Color.green);
        	synchronized(m_lock)
        	{
        		for (Position pos : m_posList)
        		{
        			g.fillRect(pos.m_x - 2,  pos.m_y - 2, 4, 4);
        		}
        		
        		if (m_visible)
        		{
		        	g.setColor(Color.red);
		        	g.fillRect(m_xPos - 2, m_yPos - 2, 5, 5);
		        	
		        	int dx = (int) (15 * Math.sin(m_yaw * Math.PI / 180));
		        	int dy = (int) (15 * Math.cos(m_yaw * Math.PI / 180));
		        	g.drawLine(m_xPos, m_yPos, m_xPos - dx, m_yPos + dy);
        		}
        	}
	    }
	    
		private void setPosition(double yaw, double x, double y)
		{
			boolean paint = false;
			int yPos = (int) (x * m_fieldWidth / (27*12)) + m_fieldWidth / 2;
			int xPos = (int) (y * m_fieldLength / (54*12));
			
			synchronized(m_lock)
			{
				if ((xPos != m_xPos) || (yPos != m_yPos))
				{
					m_posList.add(new Position(m_xPos, m_yPos));
					
					if (m_posList.size() >= m_maxList)
					{
						System.out.println("Shrink list");
						m_posList.removeRange(0, m_maxList/10);
					}
				
					m_xPos = xPos;
					m_yPos = yPos;
					
					paint = true;
				}
				
				int iYaw = (int) (yaw + 0.5);
				if (iYaw != m_yaw)
				{
					m_yaw = iYaw;
					paint = true;
				}
			}
			
			if (paint)
			{
				repaint();
			}
			
			m_visible = true;
		}
		
		private void notVisible()
		{
			m_visible = false;
			repaint();
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
						PositionViewer window = new PositionViewer(args[0]);
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
	public PositionViewer(String args)
	{
		initialize(args);
	}
	
	private double[] parseDouble(String str, int count) {
		double[] args = new double[count];
		int i = 0;

		String[] tokens = str.trim().split(" ");

		for (String token : tokens) {
			try {
				args[i] = Double.parseDouble(token);

			} catch (NumberFormatException nfe) {
				break;
			}

			if (++i >= count) {
				break;
			}
		}

		if (i == count) {
			return (args);
		}

		return (null);
	}

	public void commandReceived(String command)
	{
		if (command.charAt(0) == '+')
		{
			double[] args;
			
			if ((args = parseDouble(command.substring(1), 3)) != null)
			{
				m_panel.setPosition(args[0], args[1], args[2]);
			}
		}
		else
		{
			m_panel.notVisible();
		}
	}
	
	/**
	 * Initialize the contents of the frame.
	 */
	private void initialize(String host) {
		frame = new JFrame();
		frame.setBounds(100, 100, 912, 568);
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.getContentPane().setLayout(null);
		
		JButton btnSave = new JButton("Clear");
		btnSave.addActionListener(new ActionListener() 
		{
			public void actionPerformed(ActionEvent arg0) 
			{
				synchronized(m_lock)
				{
					m_posList.clear();
					m_panel.repaint();
				}
			}
		});
		btnSave.setBounds(790, 501, 100, 23);
		frame.getContentPane().add(btnSave);
		
		m_panel = new CustomPanel();
		m_panel.addMouseListener(new MouseAdapter() 
		{
			@Override
			public void mouseReleased(MouseEvent event) 
			{
//				int	x	= event.getX();
//				int	y	= event.getY();
			}
		});
		m_panel.setBounds(0, 0, 900, 450);
		frame.getContentPane().add(m_panel);
		m_panel.computeScale();
					
		System.out.println("host = " + host);
		
		m_network.Connect(host, 5802);
	}
}

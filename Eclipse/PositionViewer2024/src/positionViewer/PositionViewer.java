package positionViewer;

import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.Timer;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFileChooser;
import javax.swing.border.AbstractBorder;
import javax.swing.JOptionPane;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseWheelEvent;
import java.awt.geom.AffineTransform;
import java.awt.geom.Point2D;
import java.awt.geom.Rectangle2D;
import java.awt.event.ActionListener;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.ActionEvent;

public class PositionViewer {
//	private static double k_fieldLength = 651.25; // inches
//	private static double k_fieldWidth = (315.0); // inches;
	private static double k_fieldLength = 1200 / 2.54; // inches
	private static double k_fieldWidth = 600 / 2.54; // inches;
	private static int k_maxTags = 16;

	private class Replay {
		private class ReplayPoint {
			private int m_time;
			private int m_tag;
			private double m_yaw;
			private double m_estYaw;
			private double m_x;
			private double m_estX;
			private double m_y;
			private double m_estY;
			
			ReplayPoint(int time, int tag, double yaw, double estYaw, double x, double estX, double y, double estY)
			{
				m_time = time;
				m_tag = tag;
				m_yaw = yaw;
				m_estYaw = estYaw;
				m_x = x;
				m_estX = estX;
				m_y = y;
				m_estY = estY;
			}
			
		}
		
		ArrayList<ReplayPoint> m_points = new ArrayList<ReplayPoint>();
		
		long m_startTime = 0;
		int m_currentPoint = 0;
		int m_currentTime = 0;
		boolean m_showEstimated = true;
		boolean m_pause = false;
		Color m_colors[] = new Color[k_maxTags];
		Color m_colorSet[] = { Color.green, Color.orange, Color.magenta, Color.cyan };
		boolean m_enabled[] = new boolean[k_maxTags];
		
		public void setColors() {
			int colorNo = 0;
			
			for (int i = 0 ; i < m_colors.length ; i++) {
				m_colors[i] = null;
			}
			
			for (ReplayPoint point : m_points) {
				if (point.m_tag >= 1 && point.m_tag <= m_colors.length) {
					if (m_enabled[point.m_tag-1]) {
						if (m_colors[point.m_tag-1]== null) {
							m_colors[point.m_tag-1] = m_colorSet[colorNo];
							colorNo = (colorNo + 1) % m_colorSet.length;
						}
					}
				}
			}
		}
		
		boolean isEnabled(int tag)
		{
			if ((tag >= 1) && (tag <= m_enabled.length))
			{
				return m_enabled[tag-1];
			}
			
			return false;
		}
		
		void setEnabled(boolean enable) {
			for (int i = 0 ; i < m_enabled.length ; i++)
			{
				m_enabled[i] = enable;
			}
		}
		
		void setEnabled(int tag, boolean enable) {
			if ((tag >= 1) && (tag <= m_enabled.length)) {
				m_enabled[tag-1] = enable;
			}
			setColors();
		}
		
		public Color getColor(int tag) {
			Color color = null;
			if ((tag >= 1) && (tag <= m_colors.length)) {
				color = m_colors[tag-1];
			}
			
			return color == null ? Color.black : color;
		}
		
		public void addPoint(String line) {
			String[] fields = line.split(",");
			String[] timeStr = fields[0].split(":");
			double[] values = new double[fields.length - 1];
			long time = Long.parseLong(timeStr[0]);
			
			if (m_points.size() == 0) {
				m_startTime = time;
			}
			
			for (int i = 1 ; i < fields.length ; i++)
			{
			//   0     1        2       3       4           5     6   7   8   9    10
//				tag,last yaw,cam yaw,calc yaw,update yaw, est yaw,x,est x,y,est y,adjust	
				if (fields[i].compareTo("") == 0) {
					values[i - 1] = -1;
				}
				else {
					values[i - 1] = Double.parseDouble(fields[i]);
				}
			}
			
			m_points.add(new ReplayPoint((int) (time - m_startTime), (int) values[0], values[2], values[5], values[6], values[7], values[8], values[9] ));
		}
		
		public void clear() {
//			m_currentPoint = 0;
			m_currentTime = 0;
			m_points.clear();
		}
	}
	
	Replay m_replay = new Replay();

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
			aaHint = antiAlias ? RenderingHints.VALUE_ANTIALIAS_ON : RenderingHints.VALUE_ANTIALIAS_OFF;
		}

		@Override
		public Insets getBorderInsets(Component c, Insets insets) {
			int size = Math.max(lineSize, cornerSize);
			if (insets == null)
				insets = new Insets(size, size, size, size);
			else
				insets.left = insets.top = insets.right = insets.bottom = size;
			return insets;
		}

		@Override
		public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
			Graphics2D g2d = (Graphics2D) g;
			Paint oldPaint = g2d.getPaint();
			Stroke oldStroke = g2d.getStroke();
			Object oldAA = g2d.getRenderingHint(RenderingHints.KEY_ANTIALIASING);
			try {
				g2d.setPaint(fill != null ? fill : c.getForeground());
				g2d.setStroke(stroke);
				if (aaHint != null)
					g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, aaHint);
				int off = lineSize >> 1;
				g2d.drawRoundRect(x + off, y + off, width - lineSize, height - lineSize, cornerSize, cornerSize);
			} finally {
				g2d.setPaint(oldPaint);
				g2d.setStroke(oldStroke);
				if (aaHint != null)
					g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, oldAA);
			}
		}
	}

	class Position {
		public int m_x;
		public int m_y;

		public Position(int x, int y) {
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
	private double m_yaw = 90;	//-90;
	private double m_xPos = -200 / 2.54; //-5 * 12;
	private double m_yPos = 100 / 2.54; //15 * 12;
	private int m_lastXPos = 0;
	private int m_lastYPos = 0;

	private ArrayListExt<Position> m_posList = new ArrayListExt<Position>();
	private static int m_maxList = 1000;
	private Object m_lock = new Object();
	private boolean m_visible = true;

	Dimension size = Toolkit.getDefaultToolkit().getScreenSize();
	private int m_screenHeight = (int) size.getHeight();
	private int m_screenWidth = (int) size.getWidth();

	@SuppressWarnings("serial")
	private class CustomPanel extends JPanel implements Control {
		private BufferedImage m_image = null;

		public CustomPanel() {

			setBorder(BorderFactory.createLineBorder(Color.black));

			try {
				m_image = ImageIO.read(new File("field.png"));
			} catch (IOException e) {
			}

			m_controls.add(this);
		}

		int m_x;
		int m_y;
		int m_width;
		int m_height;

		@Override
		public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;

			super.setBounds(x, y, width, height);
		}

		@Override
		public void resize(int width, int height) {
			double scale = (double) width / k_designWidth;
			double scaley = (double) height / k_designHeight;

			// Maintain aspect ratio
			if (scaley < scale) {
				scale = scaley;
			}
			super.setBounds((int) (m_x * scale),
					(int) (m_y * scale),
					(int) (m_width * scale),
					(int) (m_height * scale));

			computeScale();
		}

		public Dimension getPreferredSize() {
			return new Dimension(250, 200);
		}

		int viewWidth;
		int viewHeight;

		double m_sx = 1;
		double m_sy = 1;

		public void computeScale() {
			viewWidth = getWidth();
			viewHeight = getHeight();

			m_sy = (double) viewWidth / k_fieldLength;
			m_sx = m_sy; // viewHeight / m_fieldWidth;
			System.out.println(String.format("sy = %f, sx = %f, viewHeight = %d, viewFieldWidth = %f", m_sy, m_sx,
					viewHeight, k_fieldWidth));
		}

		// Converts field y pos (in inches) to the view x pos (in pixels)
		private int sx(double y) {
			return (int) (y * m_sx);
		}

		// Converts the field x pos (in inches) to the view y pos (in pixels)
		private int sy(double x) {
			return ((int) (x * m_sy)) + viewHeight; // / 2;
		}

		// Converts the view y pos (in pixels) to the field x pos (in inches)
		public double isx(int y) {
			return (y - (double) viewHeight / 2) / m_sy;
		}

		// Converts the view x pos (in pixels) to the field y pos (in inches)
		public double isy(int x) {
			return (x / m_sx);
		}
		
		public int[] transformPoint(double x, double y) {
			double yy = (x / 0.0254); // Convert to feet
			double xx = (-y / 0.0254);
			
			int[] ret = new int[2];
			
			ret[0] = sx(yy);
			ret[1] = sy(xx);
			
			return(ret);
		}
		
		boolean m_displayCurrent = false;
		
		public void paintReplay(Graphics g) {
			// Paint estimated position
			if (m_replay.m_showEstimated) {
				for (Replay.ReplayPoint point : m_replay.m_points) {
					int[] pos = transformPoint(point.m_estX, point.m_estY);
					
					g.setColor(Color.blue);
	
					fillZoomedRect(g, pos[0] - 2, pos[1] - 2, 5, 5);
					
					if (point.m_time >= m_replay.m_currentTime) {
						break;
					}
				}
			}
			
			// Paint camera position
			m_replay.m_currentPoint = 0;
			for (Replay.ReplayPoint point : m_replay.m_points) {
				if (m_replay.isEnabled(point.m_tag)) {
					int[] pos = transformPoint(point.m_x, point.m_y);

					g.setColor(m_replay.getColor(point.m_tag));

					fillZoomedRect(g, pos[0] - 2, pos[1] - 2, 3, 3);
				}
				
				if (point.m_time >= m_replay.m_currentTime) {
					break;
				}
				m_replay.m_currentPoint++;
			}
			
			if (m_replay.m_currentPoint >= m_replay.m_points.size()) {
				m_replay.m_currentPoint = m_replay.m_points.size() - 1;
			}
			
			Replay.ReplayPoint point = m_replay.m_points.get(m_replay.m_currentPoint);
			
			g.setColor(Color.red);
			paintRobot(g, -point.m_estY / 0.0254, point.m_estX / 0.0254, point.m_estYaw + 90);
			
			m_btnCurPoint.setText(String.format("%d", m_replay.m_currentPoint));

			if (m_displayCurrent)
			{
				displayCurrentPoint();
				m_displayCurrent = false;
			}
		}
		
		boolean m_showReplay = false;
		
		void paintRobot(Graphics g, double xPos, double yPos, double yaw) {
			int x = sx(yPos);
			int y = sy(xPos);
			g.fillRect(x - 2, y - 2, 5, 5);

			int dx = (int) (15 * Math.sin((yaw + 180) * Math.PI / 180));
			int dy = (int) (15 * Math.cos((yaw + 180) * Math.PI / 180));
			g.drawLine(x, y, x + dx, y + dy);
		}
		
		double m_zoom = 1.0;
		int m_zoomX = 0;
		int m_zoomY = 0;
		
		private void setZoom(Graphics g, double zoom, int x, int y) {
			Graphics2D g2d = (Graphics2D) g;

			AffineTransform curAt = g2d.getTransform();
			double sx = curAt.getScaleX();
			double sy = curAt.getScaleX();
			
			Point2D src = new Point2D.Double(x, y);
			Point2D dst = new Point2D.Double(0, 0);
			
			curAt.transform(src, dst);

            AffineTransform at = new AffineTransform();
            at.translate(dst.getX(), dst.getY());
            at.scale(zoom * sx, zoom * sy);
            at.translate(-x, -y);

            g2d.setTransform(at);
		}
		
		void fillZoomedRect(Graphics g, int x, int y, int width, int height)
		{
			double zoom = (m_zoom > 2.0 ? 2.0 : m_zoom);
			
			Rectangle2D rect = new Rectangle2D.Double(x, y, width / zoom, height / zoom);
			((Graphics2D) g).fill(rect);
		}
		
		void paintGrid(Graphics g) {
			g.setColor(Color.gray);
			for (double x = 0 ; x < 17.0 ; x += 1.0) {
				g.drawLine(sx(x/ 0.0254), sy(-0 / 0.0254), sx(x / 0.0254), sy(-9 / 0.0254));
			}
			
			for (double y = 0 ; y < 9.0 ; y += 1.0) {
				g.drawLine(sx(0/ 0.0254), sy(-y / 0.0254), sx(17 / 0.0254), sy(-y / 0.0254));
			}

		}

		public void paintComponent(Graphics g) {
			super.paintComponent(g);
			
			if (m_zoom != 1.0) {
				setZoom(g, m_zoom, m_zoomX, m_zoomY);
			}
			
			viewHeight = (int) (viewWidth * k_fieldWidth / k_fieldLength);
			if (!m_allianceRed) {
				g.drawImage(m_image, 0, 0, viewWidth, viewHeight, null);

			} else {

				g.drawImage(m_image, viewWidth, viewHeight, -viewWidth, -viewHeight, null);
			}
			
			paintGrid(g);
			
			if (m_showReplay) {
				paintReplay(g);
			}
			else {
				g.setColor(Color.green);
				synchronized (m_lock) {
					for (Position pos : m_posList) {
						g.fillRect(pos.m_x - 2, pos.m_y - 2, 4, 4);
					}
	
					if (m_visible) {
						g.setColor(Color.red);
						paintRobot(g, m_xPos, m_yPos, m_yaw);
					}
				}
			}
		}

		private void setPosition(double yaw, double x0, double y0) {
			boolean paint = false;
			double y = (x0 / 0.0254); // Convert to inches
			double x = (-y0 / 0.0254);
			yaw += 90;
			if (yaw > 180) {
				yaw -= 360;
			}
			
			int yPos = sy(x);
			int xPos = sx(y);

			synchronized (m_lock) {
				if (!m_panel.m_showReplay) {
					m_estYaw.setText(String.format("%f", normalizeAngle(yaw-90)));
					m_estX.setText(String.format("%f", x0));
					m_estY.setText(String.format("%f", y0));
				}

				if ((xPos != m_lastXPos) || (yPos != m_lastYPos)) {
					m_posList.add(new Position(xPos, yPos));

					if (m_posList.size() >= m_maxList) {
						System.out.println("Shrink list");
						m_posList.removeRange(0, m_maxList / 10);
					}

					m_xPos = x;// Pos;
					m_yPos = y;// Pos;
					
					System.out.println(String.format("xPos=%f,yPos=%f", m_xPos, m_yPos));

					paint = true;
				}

				int iYaw = (int) (yaw + 0.5);
				if (iYaw != m_yaw) {
					m_yaw = iYaw;
					paint = true;
				}
			}

			if (paint) {
				repaint();
			}

			m_visible = true;
		}

		private void notVisible() {
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
				if (args.length != 1) {
					JOptionPane.showMessageDialog(null, "Must specify ip address");
				} else {
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
	public PositionViewer(String args) {
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

	public void disconnected() {
		connected.setText("Disconnected");
		connected.setForeground(Color.red);
	}

	public void connected() {
		connected.setText("Connected");
		connected.setForeground(Color.green);
	}

	public void commandReceived(String command) {
		switch (command.charAt(0)) {
			case '+':
				double[] args;

				if ((args = parseDouble(command.substring(1), 3)) != null) {
					m_panel.setPosition(args[0], args[1], args[2]);
				}
				break;

			case 'c':

				setAllianceColor(command.charAt(1) == 'r');
				break;

		}

	}

	/**
	 * Initialize the contents of the frame.
	 */
	int m_mouseX;
	int m_mouseY;

	double m_pathEndx, m_pathEndy;

	class BezierPoints {
		double m_p0x;
		double m_p0y;
		double m_angle1;
		double m_l1;
		double m_p3x;
		double m_p3y;
		double m_angle2;
		double m_l2;

		public BezierPoints(double p0x, double p0y, double angle1, double l1, double p3x, double p3y, double angle2,
				double l2) {
			m_p0x = p0x;
			m_p0y = p0y;
			m_angle1 = angle1;
			m_l1 = l1;
			m_p3x = p3x;
			m_p3y = p3y;
			m_angle2 = angle2;
			m_l2 = l2;
		}
	}

	ArrayList<BezierPoints> m_lastPath;

	private void onClick(MouseEvent event) {
		// m_mouseX = event.getX();
		// m_mouseY = event.getY();
		// Point mousePos = new Point(m_mouseX, m_mouseY);
		//
		// System.out.println(String.format("mouse: (%d,%d)", m_mouseX, m_mouseY));
		// System.out.println(String.format("Field: (%f,%f)", m_xPos, m_yPos));
	}

	boolean m_allianceRed = false;

	private void setAllianceColor(boolean red) {
		if (m_allianceRed != red) {

			m_allianceRed = red;
			// yFlipCoordinates();
			redCheck.setSelected(red);
			// teeterCordinates();

			m_panel.repaint();

		}
	}

	private double normalizeAngle(double angle) {
		angle = angle % 360;
		if (angle > 180) {
			angle -= 360;

		} else if (angle < -180) {
			angle += 360;
		}
		return angle;
	}


	JCheckBoxSZ redCheck;
	JLabelSZ message;

	JCheckBoxSZ m_tags[] = new JCheckBoxSZ[k_maxTags];
	JCheckBoxSZ m_estimated;
	JLabelSZ m_tagYaw[] = new JLabelSZ[k_maxTags];
	JLabelSZ m_tagX[] = new JLabelSZ[k_maxTags];
	JLabelSZ m_tagY[] = new JLabelSZ[k_maxTags];
	JLabelSZ m_estYaw;
	JLabelSZ m_estX;
	JLabelSZ m_estY;
	JLabelSZ m_deltaYaw;
	JLabelSZ m_deltaX;
	JLabelSZ m_deltaY;
	
	JButtonSZ m_loadReplay;
	JLabelSZ connected;

	private interface Control {
		void resize(int width, int height);
	}

	private ArrayList<Control> m_controls = new ArrayList<Control>();

	private class JLabelSZ extends JLabel implements Control {
		/**
		 * 
		 */
		private static final long serialVersionUID = -4361235814923354046L;
		int m_x;
		int m_y;
		int m_width;
		int m_height;
		int m_fontHeight;

		JLabelSZ(String title) {
			super(title);

			m_controls.add(this);
		}

		@Override
		public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;

			super.setBounds(x, y, width, height);
		}

		@Override
		public void setFont(Font font) {
			m_fontHeight = font.getSize();
		}

		@Override
		public void resize(int width, int height) {
			super.setBounds(m_x * width / k_designWidth,
					m_y * height / k_designHeight,
					m_width * width / k_designWidth,
					m_height * height / k_designHeight);

			super.setFont(new Font(Font.SERIF, Font.PLAIN, m_fontHeight * height / k_designHeight));
		}
	}

	private class JRadioButtonSZ extends JRadioButton implements Control {
		/**
		 * 
		 */
		private static final long serialVersionUID = -837154600923199949L;

		int m_x;
		int m_y;
		int m_width;
		int m_height;
		int m_fontHeight;

		public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;

			super.setBounds(x, y, width, height);
		}

		@Override
		public void setFont(Font font) {
			super.setFont(font);

			m_fontHeight = font.getSize();
		}

		@Override
		public void resize(int width, int height) {
			super.setBounds(m_x * width / k_designWidth,
					m_y * height / k_designHeight,
					m_width * width / k_designWidth,
					m_height * height / k_designHeight);

			super.setFont(new Font(Font.SERIF, Font.PLAIN, m_fontHeight * height / k_designHeight));
		}
	}

	private class JCheckBoxSZ extends JCheckBox implements Control {
		/**
		 * 
		 */
		private static final long serialVersionUID = -6986813249292553889L;
		int m_x;
		int m_y;
		int m_width;
		int m_height;
		int m_tag = 0;

		JCheckBoxSZ(String title) {
			super(title);

			m_controls.add(this);
		}

		public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;

			super.setBounds(x, y, width, height);
		}

		@Override
		public void resize(int width, int height) {
			super.setBounds(m_x * width / k_designWidth,
					m_y * height / k_designHeight,
					m_width * width / k_designWidth,
					m_height * height / k_designHeight);
		}

	}

	private class JButtonSZ extends JButton implements Control {
		/**
		 * 
		 */
		private static final long serialVersionUID = -6986813249292553889L;
		int m_x;
		int m_y;
		int m_width;
		int m_height;

		JButtonSZ(String title) {
			super(title);

			m_controls.add(this);
		}

		public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;

			super.setBounds(x, y, width, height);
		}

		@Override
		public void resize(int width, int height) {
			super.setBounds(m_x * width / k_designWidth,
					m_y * height / k_designHeight,
					m_width * width / k_designWidth,
					m_height * height / k_designHeight);
		}

	}
	
	//Create a file chooser
	final JFileChooser fc = new JFileChooser();
	
	int m_replaySpeed = 1;
	long m_replayStartTime = 0;
    Timer m_replayTimer = new Timer(20, new ActionListener() {
        @Override
        public void actionPerformed(ActionEvent e) {
        	if (!m_replay.m_pause) { 
	        	m_replay.m_currentTime = (int) (System.currentTimeMillis() - m_replayStartTime) * m_replaySpeed;
	        	if (m_replay.m_currentTime < m_replay.m_points.get(m_replay.m_points.size() - 1).m_time) {
	            	m_panel.repaint();
	            }
        	}
        }
    });
    
    private void enableTags()
    {
    	m_replay.setEnabled(true);
    	
    	for (int i = 0 ; i < m_tags.length ; i++) {
    		m_tags[i].setSelected(true);
    	}
    }
    
    private void setTagCheckboxColors() {
    	for (int i = 0 ; i < m_tags.length ; i++) {
    		m_tags[i].setForeground(m_replay.getColor(i + 1));
    	}
    }
	
	private void loadReplay(File file) {
        System.out.println("Opening: " + file.getName() + ".");	
        try {
			BufferedReader reader = new BufferedReader(new FileReader(file));
			
//			m_panel.clearTagColors();
			m_replay.clear();
			m_replayStartTime = System.currentTimeMillis();
			
			try {
				reader.readLine();	// Skip header line
				while (true) {
					String line = reader.readLine();
					if (line == null) {
						break;
					}
					System.out.println(line);
					m_replay.addPoint(line);
				}

				enableTags();
				m_replay.setColors();
				setTagCheckboxColors();

				m_lastFolder = file.getParent();
			} catch (IOException e) {
				e.printStackTrace();
			}
			
			try {
				reader.close();
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}			
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
	}
	
	String m_lastFolder = null;
	
	private void loadReplay() {
		if (m_panel.m_showReplay) {
			m_replayTimer.stop();
			m_panel.m_showReplay = false;
			
			m_loadReplay.setText("Load Replay");
		} else {
			if (m_lastFolder != null) {
				fc.setCurrentDirectory(new File(m_lastFolder));
			} else {
				fc.setCurrentDirectory(new File(System.getProperty("user.dir")));
			}
	        int returnVal = fc.showOpenDialog(frame);
	
	        if (returnVal == JFileChooser.APPROVE_OPTION) {
	            File file = fc.getSelectedFile();
	            
	            loadReplay(file);
	            
				m_panel.m_showReplay = true;
				m_replayTimer.start();
		        m_loadReplay.setText("End Replay");
	        } else {
	            System.out.println("Open command cancelled by user.");
	        }
		}
		
		m_panel.repaint();		
	}
	
	JButtonSZ m_btnStop;
	JLabelSZ m_btnCurPoint;
	
	void displayCurrentPoint() {
//		m_btnCurPoint.setText(String.format("%d", m_replay.m_currentPoint));
		
		for (int i = 0 ; i < m_tagYaw.length ; i++) {
			m_tagYaw[i].setText("-");
			m_tagX[i].setText("-");
			m_tagY[i].setText("-");
		}
		
		if ((m_replay.m_currentPoint >= 0) && (m_replay.m_currentPoint < m_replay.m_points.size())) {
			Replay.ReplayPoint point = m_replay.m_points.get(m_replay.m_currentPoint);
			
			m_estYaw.setText(String.format("%f", point.m_estYaw));
			m_estX.setText(String.format("%f", point.m_estX));
			m_estY.setText(String.format("%f", point.m_estY));
			
			if ((point.m_tag >= 1) && (point.m_tag < m_tagYaw.length)) {
				m_tagYaw[point.m_tag-1].setText(String.format("%f", point.m_yaw));
				m_tagX[point.m_tag-1].setText(String.format("%f", point.m_x));
				m_tagY[point.m_tag-1].setText(String.format("%f", point.m_y));
				
				m_deltaYaw.setText(String.format("%f", point.m_yaw - point.m_estYaw));
				m_deltaX.setText(String.format("%f", point.m_x - point.m_estX));
				m_deltaY.setText(String.format("%f", point.m_y - point.m_estY));
			}
			else {
				m_deltaYaw.setText("-");
				m_deltaX.setText("-");
				m_deltaY.setText("-");
			}
		}
	}
	
	void pauseReplay(boolean pause) {
		m_replay.m_pause = pause;
		m_btnStop.setText(m_replay.m_pause ? ">" : "||");
		if (m_replay.m_pause) {
			m_panel.m_displayCurrent = true;
			m_panel.repaint();
		}
		else {
			if ((m_replay.m_currentPoint >= 0) && (m_replay.m_currentPoint < m_replay.m_points.size())) {
				m_replayStartTime = System.currentTimeMillis() - (int) ((m_replay.m_points.get(m_replay.m_currentPoint).m_time / m_replaySpeed));
			}
		}		
	}

	private static final int k_fieldDisplayWidth = 1440;
	private static final int k_designWidth = 1800;
	private static final int k_designHeight = 800;
	private boolean m_camerasDisabled = false;

	private void initialize(String host) {

		Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();

		frame = new JFrame();
		frame.setBounds(0, 0, screenSize.width - 10, screenSize.height - 50); // sets the frame of the window created
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.getContentPane().setLayout(null);

		frame.addComponentListener(new ComponentListener() {
			@Override
			public void componentResized(ComponentEvent e) {
				// TODO Auto-generated method stub
				int w = e.getComponent().getWidth();
				int h = e.getComponent().getHeight();

				for (Control control : m_controls) {
					control.resize(w, h);
				}
			}

			@Override
			public void componentMoved(ComponentEvent e) {
				// TODO Auto-generated method stub

			}

			@Override
			public void componentShown(ComponentEvent e) {
				// TODO Auto-generated method stub

			}

			@Override
			public void componentHidden(ComponentEvent e) {
				// TODO Auto-generated method stub

			}
		});

		connected = new JLabelSZ("Disconnected");
		connected.setBounds(k_fieldDisplayWidth + 10, 10, 400, 54);
		connected.setFont(new Font(Font.SERIF, Font.PLAIN, 48));
		connected.setForeground(Color.red);
		frame.getContentPane().add(connected);
		
		int yPos = 0;
		for (int i = 0 ; i < k_maxTags ; i++) {
			m_tags[i] = new JCheckBoxSZ(String.format("Tag %d", i + 1));
			m_tags[i].m_tag = i + 1;
			m_tags[i].setSelected(true);
			m_tags[i].addActionListener(new ActionListener() {
				public void actionPerformed(ActionEvent arg0) {
					JCheckBoxSZ checkBox = ((JCheckBoxSZ) arg0.getSource());
					synchronized (m_lock) {
						m_replay.setEnabled(checkBox.m_tag, checkBox.isSelected());
						m_panel.repaint();
						setTagCheckboxColors();
					}
				}
			});
			m_tags[i].setBounds(k_fieldDisplayWidth + 10, yPos = 10 + 60 + i*25, 100, 23);
			frame.getContentPane().add(m_tags[i]);
			
			m_tagYaw[i] = new JLabelSZ("yaw");
			m_tagYaw[i].setBounds(k_fieldDisplayWidth + 10 + 100, yPos, 90, 23);
			frame.getContentPane().add(m_tagYaw[i]);
			
			m_tagX[i] = new JLabelSZ("x");
			m_tagX[i].setBounds(k_fieldDisplayWidth + 10 + 190, yPos, 90, 23);
			frame.getContentPane().add(m_tagX[i]);
			
			m_tagY[i] = new JLabelSZ("y");
			m_tagY[i].setBounds(k_fieldDisplayWidth + 10 + 280, yPos, 90, 23);
			frame.getContentPane().add(m_tagY[i]);
		}
		
		yPos += 25;
		m_estimated = new JCheckBoxSZ("Estimated");
		m_estimated.setSelected(true);
		m_estimated.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				JCheckBoxSZ checkBox = ((JCheckBoxSZ) arg0.getSource());
				synchronized (m_lock) {
					m_replay.m_showEstimated = checkBox.isSelected();
					m_panel.repaint();
					setTagCheckboxColors();
				}
			}
		});
		m_estimated.setBounds(k_fieldDisplayWidth + 10, yPos, 100, 23);
		frame.getContentPane().add(m_estimated);

		
//		JLabelSZ lblEstimated = new JLabelSZ("Estimated:");
//		lblEstimated.setBounds(k_fieldDisplayWidth + 10, yPos, 100, 23);
//		frame.getContentPane().add(lblEstimated);

		m_estYaw = new JLabelSZ("yaw");
		m_estYaw.setBounds(k_fieldDisplayWidth + 10 + 100, yPos, 90, 23);
		frame.getContentPane().add(m_estYaw);
		
		m_estX = new JLabelSZ("x");
		m_estX.setBounds(k_fieldDisplayWidth + 10 + 190, yPos, 90, 23);
		frame.getContentPane().add(m_estX);
		
		m_estY = new JLabelSZ("y");
		m_estY.setBounds(k_fieldDisplayWidth + 10 + 280, yPos, 90, 23);
		frame.getContentPane().add(m_estY);

		yPos += 25;
		JLabelSZ lblDelta = new JLabelSZ("Delta:");
		lblDelta.setBounds(k_fieldDisplayWidth + 10, yPos, 100, 23);
		frame.getContentPane().add(lblDelta);

		m_deltaYaw = new JLabelSZ("yaw");
		m_deltaYaw.setBounds(k_fieldDisplayWidth + 10 + 100, yPos, 90, 23);
		frame.getContentPane().add(m_deltaYaw);
		
		m_deltaX = new JLabelSZ("x");
		m_deltaX.setBounds(k_fieldDisplayWidth + 10 + 190, yPos, 90, 23);
		frame.getContentPane().add(m_deltaX);
		
		m_deltaY = new JLabelSZ("y");
		m_deltaY.setBounds(k_fieldDisplayWidth + 10 + 280, yPos, 90, 23);
		frame.getContentPane().add(m_deltaY);
		
		yPos += 25;
		m_loadReplay = new JButtonSZ("Load Replay");
		m_loadReplay.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				loadReplay();
			}
		});
		m_loadReplay.setBounds(k_fieldDisplayWidth + 10, yPos, 160, 30);
		frame.getContentPane().add(m_loadReplay);
		
		yPos += 32;
		int xPos = k_fieldDisplayWidth + 10;
		JButtonSZ btnRestart = new JButtonSZ("|<");
		btnRestart.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				m_replayStartTime = System.currentTimeMillis();
				m_replay.m_currentTime = 0;
				m_panel.repaint();
			}
		});
		btnRestart.setMargin(new Insets(3,3,3,3));
		btnRestart.setBounds(xPos, yPos, 40, 30);
		frame.getContentPane().add(btnRestart);

		xPos += 40;
		JButtonSZ btnBack = new JButtonSZ("<");
		btnBack.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				pauseReplay(true);
				if (m_replay.m_currentPoint > 0)
				{
					m_replay.m_currentTime = m_replay.m_points.get(m_replay.m_currentPoint - 1).m_time;
					m_panel.repaint();
				}
			}
		});
		btnBack.setMargin(new Insets(3,3,3,3));
		btnBack.setBounds(xPos, yPos, 40, 30);
		frame.getContentPane().add(btnBack);

		xPos += 40;
		m_btnStop = new JButtonSZ("||");
		m_btnStop.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				pauseReplay(!m_replay.m_pause);
			}
		});
		m_btnStop.setMargin(new Insets(3,3,3,3));
		m_btnStop.setBounds(xPos, yPos, 40, 30);
		frame.getContentPane().add(m_btnStop);

		xPos += 40;
		JButtonSZ btnForward = new JButtonSZ(">");
		btnForward.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				pauseReplay(true);
				if (m_replay.m_currentPoint < m_replay.m_points.size() - 2)
				{
					m_replay.m_currentTime = m_replay.m_points.get(m_replay.m_currentPoint + 1).m_time;
					m_panel.repaint();
				}				
			}
		});
		btnForward.setMargin(new Insets(3,3,3,3));
		btnForward.setBounds(xPos, yPos, 40, 30);
		frame.getContentPane().add(btnForward);

		xPos += 40;
		JButtonSZ btnEnd = new JButtonSZ(">|");
		btnEnd.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				pauseReplay(true);
				m_replay.m_currentTime = m_replay.m_points.get(m_replay.m_points.size() - 1).m_time + 1;
				m_panel.repaint();
			}
		});
		btnEnd.setMargin(new Insets(3,3,3,3));
		btnEnd.setBounds(xPos, yPos, 40, 30);
		frame.getContentPane().add(btnEnd);
		
		xPos += 50;
		m_btnCurPoint = new JLabelSZ("current");
		m_btnCurPoint.setBounds(xPos, yPos, 100, 30);
		frame.getContentPane().add(m_btnCurPoint);


		message = new JLabelSZ("Message");
		message.setBounds(10, k_designHeight - 70, 1500, 24);
		message.setFont(new Font(Font.SERIF, Font.PLAIN, 16));
		frame.getContentPane().add(message);

		redCheck = new JCheckBoxSZ("Red");
		redCheck.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					setAllianceColor(!m_allianceRed);
				}
			}
		});
		redCheck.setBounds(10, (k_designHeight - 100), 100, 23);
		frame.getContentPane().add(redCheck);

		JButtonSZ btnClear = new JButtonSZ("Clear");
		btnClear.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {

					m_posList.clear();
					m_panel.repaint();
				}
			}
		});
		btnClear.setBounds(10, 640, 160, 30);
		frame.getContentPane().add(btnClear);

		JButtonSZ btnDisable = new JButtonSZ("Disable Cameras");
		btnDisable.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					m_camerasDisabled = !m_camerasDisabled;
					
					if (m_camerasDisabled) {
						m_network.SendMessage("C 1");
						btnDisable.setText("Enable Cameras");
					} else {
						m_network.SendMessage("C 0");
						btnDisable.setText("Disable Cameras");
					}
				}
			}
		});
		btnDisable.setBounds(10 + 160 + 10, 640, 160, 30);
		frame.getContentPane().add(btnDisable);

		// Command buttons
		JButtonSZ button1 = new JButtonSZ("Start Log");
		button1.setBounds(10, 600, 160, 30);
		frame.getContentPane().add(button1);
		button1.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					m_network.SendMessage("B 1");
				}
			}
		});

		JButtonSZ button2 = new JButtonSZ("End Log");
		button2.setBounds(180, 600, 160, 30);
		frame.getContentPane().add(button2);
		button2.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					m_network.SendMessage("B 2");
				}
			}
		});

		JButtonSZ button3 = new JButtonSZ("Start Dashboard");
		button3.setBounds(350, 600, 160, 30);
		frame.getContentPane().add(button3);
		button3.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					m_network.SendMessage("B 3");
				}
			}
		});

		JButtonSZ button4 = new JButtonSZ("End Dashboard");
		button4.setBounds(520, 600, 160, 30);
		frame.getContentPane().add(button4);
		button4.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				synchronized (m_lock) {
					m_network.SendMessage("B 4");
				}
			}
		});

		m_panel = new CustomPanel();
		m_panel.addMouseListener(new MouseAdapter() {
			@Override
			public void mouseReleased(MouseEvent event) {
				if (event.getButton() == MouseEvent.BUTTON3) {
					int x = event.getX();
					int y = event.getY();
					double dx = x - m_mouseX;
					double dy = y - m_mouseY;

					if (dx != 0) {
						m_yaw = (int) (Math.atan2(dx, dy) * 180 / Math.PI);
					}
					m_xPos = m_panel.isx(m_mouseY);
					m_yPos = m_panel.isy(m_mouseX);
					m_visible = true;

					m_panel.repaint();

				} else {
					onClick(event);
					message.setText(String.format("%d,%d", event.getX(), event.getY()));
				}
			}

			@Override
			public void mouseMoved(MouseEvent event) {
				message.setText(String.format("%d,%d", event.getX(), event.getY()));
			}

			@Override
			public void mousePressed(MouseEvent event) {
				m_mouseX = event.getX();
				m_mouseY = event.getY();
			}

			@Override
			public void mouseClicked(MouseEvent event) {

			}
		});
		
		m_panel.addMouseWheelListener(new MouseAdapter() {
			@Override
		    public void mouseWheelMoved(MouseWheelEvent e) {
				m_panel.m_zoom = m_panel.m_zoom - e.getWheelRotation() * 0.1;
				m_panel.m_zoomX = e.getX();
				m_panel.m_zoomY = e.getY();
				if (m_panel.m_zoom < 1.0)
				{
					m_panel.m_zoom = 1.0;
				}
				m_panel.repaint();
		    	System.out.println(String.format("zoom=%f,zoomX=%d,zoomY=%d", m_panel.m_zoom, m_panel.m_zoomX, m_panel.m_zoomY));
		    }
		});

		m_panel.setBounds(0, 0, (k_fieldDisplayWidth), (int) ((k_fieldDisplayWidth) * k_fieldWidth / k_fieldLength));
		frame.getContentPane().add(m_panel);
		m_panel.computeScale();

		System.out.println("host = " + host);

		m_network.Connect(host, 5803);
	}
}

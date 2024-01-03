package positionViewer;

import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.border.AbstractBorder;
import javax.swing.JOptionPane;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.ActionListener;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.ActionEvent;



public class PositionViewer 
{
	private static double k_fieldLength = 651.25;		// inches
	private static double k_fieldWidth = (315.0);		// inches;
	
	// Specify the positions of all the possible targets.
	// All values are in inches
	private static double[] k_targetX = {  11.41, 9.63, 7.85, 5.91, 4.13, 2.35, 0.41,-1.37,-3.15,-9.00 };
	private static double[] k_targetY = { 4.02,2.79,1.38,53.67 };
//	private static double[] k_targetHCone = {  -5.4, 29.3, 37.5, 4.38 };
	private static double[] k_targetHCone = {  -5.4, 40, 37.5, 4.38 };
	private static double[] k_targetHCube = {  -5.4, 28.8, 45, 4.38 };
	private static double[] k_targetExtCone = { 0, -10, 0, 0 };		// Extent adjustment
	private static double[] k_targetExtCube = { 0, 0, 0, 0 };		// Extent adjustment
	
	class Endpoint
	{
		double m_x0;		// Horz position used to determing if the robot is left or right
		double m_leftX;		// Horz location of end point in feet when approaching from the left
		double m_leftAngle;	// Approach angle from the left in degrees
		double m_rightX;
		double m_rightAngle;
		double m_p;						// Distance to control point
		
		public Endpoint(double x0, double leftX, double leftAngle, double rightX, double rightAngle, double p)
		{
			m_x0 = x0;
			m_leftX = leftX;
			m_leftAngle = leftAngle;
			m_rightX = rightX;
			m_rightAngle = rightAngle;
			m_p = p;
		}
	}
	
    final static double k_p0 = 0.3;	// Distance to first control point from robot position as a fraction of total distance
	
	private final double k_p1 = 0.3;		// Default control point distance as a fraction of total distance
	
	Endpoint k_endpoints[] = new Endpoint[]  
	{ 
		new Endpoint( 7,    11.41,    0, 11.41,  -90, k_p1),
		new Endpoint( 5,     9.63,    0,  9.63,  -90, k_p1),
		new Endpoint( 3,     7.85,    0,  7.85,  -120, k_p1),
		new Endpoint( 3,     5.91,    0,  5.91,  -180, k_p1),
		new Endpoint( 3,     4.13,    0,  4.13,  -180, k_p1),
		new Endpoint( 2.35,  2.35,    0,  2.35,  -180, k_p1),
		new Endpoint( 0.41,  0.41,  -60,  0.41,  -180, k_p1),
		new Endpoint( 5,    -1.37,  -90, -1.37,  -180, k_p1),
		new Endpoint( 5,    -3.15,  -90, -3.15,  -180, k_p1),
		new Endpoint(-9,    -9,      90, -9,       90, k_p1)
	};
	
	double k_endpointsY[] = { 7, 7, 7 };		// end point y pos vs level
	double k_pickupY = 49.0;

	int m_targetNo = 0;
	int m_targetLevel = -1;
	TargetType m_targetType = TargetType.None;
	
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
	private double m_yaw = -90;
	private double m_xPos = -5*12;
	private double m_yPos = 15*12;
	private int m_lastXPos = 0;
	private int m_lastYPos = 0;
	
	private ArrayListExt<Position> m_posList = new ArrayListExt<Position>();
	private static int m_maxList = 1000;
	private Object m_lock = new Object();
	private boolean m_visible = true;
	
	Dimension size = Toolkit.getDefaultToolkit().getScreenSize();
	private int m_screenHeight = (int)size.getHeight();
	private int m_screenWidth = (int)size.getWidth();
	    
   	
	@SuppressWarnings("serial")
	private class CustomPanel extends JPanel implements Control
	{
		private BufferedImage m_image = null;
		
	    public CustomPanel() {
	    	
	        setBorder(BorderFactory.createLineBorder(Color.black));
			
			try {
			    m_image = ImageIO.read(new File("field.jpg"));
			} catch (IOException e) {
			}
			
			
			m_controls.add(this);
	    } 
	    
		int m_x;
		int m_y;
		int m_width;
		int m_height;
		
		@Override public void setBounds(int x, int y, int width, int height) {
			m_x = x;
			m_y = y;
			m_width = width;
			m_height = height;
			
			super.setBounds(x,  y, width, height);			
		}
		
		@Override
		public void resize(int width, int height) {
			double scale = (double) width / k_designWidth;
			double scaley = (double) height / k_designHeight;
			
			// Maintain aspect ratio
			if (scaley < scale)
			{
				scale = scaley;
			}
			super.setBounds( (int) (m_x * scale),
							 (int) (m_y * scale),
							 (int) (m_width  * scale),
							 (int) (m_height * scale));
			
			computeScale();
		}	

	    public Dimension getPreferredSize() {
	        return new Dimension(250,200);
	    }
	    
	    int viewWidth;
	    int viewHeight;

	    double m_scale = 1;
	    double m_sx	= 1;
	    double m_sy = 1;
	    
//	    int s(double v)
//	    {
//	    	return (int) (v * m_scale);
//	    }
	    
//	    int m_fieldLength = 0;
//	    int m_fieldWidth = 0;
	    
	    public void computeScale()
	    {
        	int imageWidth = m_image.getWidth();
        	int imageHeight = m_image.getHeight();
        	viewWidth	= getWidth();
        	viewHeight  = getHeight();
        	
        	m_scale = (double) viewWidth / m_width;
        	m_sy = (double) viewWidth / k_fieldLength;
        	m_sx = m_sy; //viewHeight / m_fieldWidth;
        	System.out.println(String.format("sy = %f, sx = %f, viewHeight = %d, viewFieldWidth = %f", m_sy, m_sx, viewHeight, k_fieldWidth));    	
	    }
	    
	    // Converts field y pos (in inches) to the view x pos (in pixels)
	    private int sx(double y)
	    {
	    	return (int) (y * m_sx); 
	    }
	    
	    // Converts the field x pos (in inches) to the view y pos (in pixels)
	    private int sy(double x)
	    {
	    	return((int) (x * m_sy)) + viewHeight / 2;
	    }
	    
	    // Converts the view y pos (in pixels) to the field x pos (in inches)
	    public double isx(int y)
	    {
	    	return (y - (double) viewHeight/2) / m_sy;
	    }
	    
	    // Converts the view x pos (in pixels) to the field y pos (in inches)
	    public double isy(int x)
	    {
	    	return(x / m_sx);
	    }
	    
	    //Needs to be fixed
	    /*public boolean intersects(int xCoordinate, int yCoordinate, int BoxLeftX, int BoxTopY, int BoxRightX, int BoxBottomY) {
	    	if (xCoordinate > BoxLeftX && xCoordinate < BoxRightX && yCoordinate < BoxTopY && yCoordinate > BoxBottomY) {
	    		return true;
	    	}
	    		return false;
	    	}
	    	*/
	    
	    public double findTargetAngle(double x0, double y0) {
	      double y = m_yPos/12 - y0;
	      double x = x0 - m_xPos/12;
	      double m_targetAngle = -Math.atan2(y, x);
	      return m_targetAngle;
	    }
	    
	    void paintPath(Graphics g, int targetNo, int level)
	    {
//	    	if (targetNo >= 9)
//	    	{
//	    		return;	// mustfix
//	    	}
	    	Endpoint endpoint = k_endpoints[targetNo];
	    	double targetX = endpoint.m_x0;
	    	double robotX = m_xPos / 12;
	    	double robotY = m_yPos / 12;
	    	double yaw = Math.toRadians(m_yaw);
	    	double endX;
	    	double endY = m_targetNo == 9 ? k_pickupY : k_endpointsY[level];
	    	double endAngle;
	    	
	    	if ((robotX < targetX) ^ !m_allianceRed)
	    	{
	    		// to right
		    	endX = endpoint.m_rightX;
		    	endAngle = Math.toRadians(endpoint.m_rightAngle + 180);
	    	}
	    	else
	    	{
	    		// to left
		    	endX = endpoint.m_leftX;
		    	endAngle = Math.toRadians(endpoint.m_leftAngle + 180);	    		
	    	}
	    	
	    	double ead = Math.toDegrees(endAngle);
	    	
	    	double dx = endX - robotX;
	    	double dy = endY - robotY;
	    	double a = Math.toDegrees(Math.atan2(dy, dx));		// Angle to target
	    	double da = normalizeAngle(m_yaw - a);				// Difference between the robot angle and the angle to the target
	    	double dist = Math.sqrt(dx*dx + dy*dy);
	    	
	    	if ((da > 90) || (da < -90))
	    	{
	    		yaw = Math.toRadians(m_yaw + 180);	// Run robot backwards
	    	}
	    	
			BezierQuintic bezierBox = new BezierQuintic(robotX, robotY, yaw, dist * k_p0, 0, endX, endY, endAngle, dist * endpoint.m_p, 0); //

			ArrayList<CurvePoint> curvePoints = bezierBox.ComputeSplinePoints(null, 100);

    		int lx = 0;
    		int ly = -1;
    		
    		g.setColor(Color.blue);
    		
    		for (CurvePoint point : curvePoints)
    		{
    			int x = sx(point.m_y * 12);
    			int y = sy(point.m_x * 12);
    			
    			if (ly >= 0)
    			{
    				g.drawLine(lx,  ly, x, y);
    			}
    			
    			lx = x;
    			ly = y;
    		}
    		g.drawLine(lx, ly, sx(endY * 12), sy(endX * 12));
	    }
	    
	    public void paintComponent(Graphics g) 
	    {
	        super.paintComponent(g);       
	        viewHeight = (int) (viewWidth * k_fieldWidth / k_fieldLength);
	        if(!m_allianceRed) {
	        	g.drawImage(m_image, 0, 0, viewWidth, viewHeight, null);
	        
	        } else {
	 
	        	g.drawImage(m_image, viewWidth, viewHeight, -viewWidth, -viewHeight, null);
	        }
	        
	        if ((m_targetNo >= 0) && (m_targetLevel >= 0))
	        {
		        // Paint current target
		        double tX = targetXPos; //targetX[targetNo];
		        double tY = targetYPos;	//targetNo == 9 ? targetY[3] : targetY[targetLevel];
		        int ty = sy(tX*12);
		        int tx = sx(tY*12);
		        
		        g.setColor(Color.black);
		        g.fillRect(tx-5, ty-5, 10, 10);
		        
		        double targetAngle = findTargetAngle(tX, tY);
		        
		        double ex = m_xPos + 75 * Math.cos(targetAngle);
		        double ey = m_yPos + 75 * Math.sin(targetAngle);
	      
		        g.drawLine(sx(m_yPos), sy(m_xPos), sx(ey), sy(ex));
	        
		        message.setText(String.format("x=%f,y=%f,tx=%f,ty=%f,Angle=%f", m_xPos/12, m_yPos/12, tX, tY, Math.toDegrees(targetAngle)));
		        
	    		paintPath(g, m_targetNo, m_targetLevel);
	        }
        
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
		        	int x = sx(m_yPos);
		        	int y = sy(m_xPos);
		        	g.fillRect(x - 2, y - 2, 5, 5);
		        	
		        	int dx = (int) (15 * Math.sin((m_yaw+180) * Math.PI / 180));
		        	int dy = (int) (15 * Math.cos((m_yaw+180) * Math.PI / 180));
		        	g.drawLine(x, y, x + dx, y + dy);
        		}
        	}
	    }
	    
	   
	    
		private void setPosition(double yaw, double x, double y)
		{
			boolean paint = false;
			int yPos = sy(x);
			int xPos = sx(y);
			
			synchronized(m_lock)
			{
				if ((xPos != m_lastXPos) || (yPos != m_lastYPos))
				{
					m_posList.add(new Position(xPos, yPos));
					
					if (m_posList.size() >= m_maxList)
					{
						System.out.println("Shrink list");
						m_posList.removeRange(0, m_maxList/10);
					}
				
					m_xPos = x;//Pos;
					m_yPos = y;//Pos;
					
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
	
	public void disconnected()
	{
		m_targetNo = -1;
		m_targetLevel = -1;
		
		setTargetType(PositionViewer.TargetType.None);
		
		pieceType.setText("Disconnected");
		pieceType.setForeground(Color.red);
	}

	public void connected()
	{
		pieceType.setText("Connected");
		pieceType.setForeground(Color.green);		
	}
	
	public void commandReceived(String command)
	{
		switch(command.charAt(0))
		{
			case '+' : 
				double[] args;
			
				if ((args = parseDouble(command.substring(1), 3)) != null)
				{
					m_panel.setPosition(args[0], args[1], args[2]);
				}
			break; 
			
			case '-' : 
//				m_panel.notVisible();
			
			break;
			
			case 'c' :
				
				setAllianceColor(command.charAt(1) == 'r');
				break;
				
		}
		
		
	}
	
	/**
	 * Initialize the contents of the frame.
	 */
	int m_mouseX;
	int m_mouseY;
	
	ArrayList<CurvePoint> m_curvePoints;
	
	//added screen dimension finder, changed x,y bounds to be top left of screen for window
	
	private class Target{
		Rectangle m_targetRectangle;
		double m_xEnd;
		double m_yEnd;
		
		public Target(int x, int y, int width, int height, double xEnd, double yEnd) { //xEnd in field coordinates
			m_targetRectangle = new Rectangle(x, y, width, height);
			m_xEnd = xEnd;
			m_yEnd = yEnd;
			
		}
	}
	
	Target[] m_targets = { //blue boxes
							new Target(1337, 0, 100, 200, -130/12.0, 623/12.0),//Blue Human Player
							new Target(0,221,120,72, -41.0/12, 66.0/12), //Blue cone 1
			
							new Target(0,295,120,40,-16.0/12,66.0/12),  //Blue box dropoff 1
							
							new Target(0,334,120,50,  5.0/12,66.0/12), //Blue cone2
							new Target(0,389,120,50,  28.0/12,66.0/12),//Blue cone3
							
							new Target(0,438,120,40,  49.0/12,66.0/12), //Blue box 2
							
							new Target(0,479,120,50,  71.0/12,66.0/12), //Blue cone 4
							new Target(0,531,120,50,  90.0/12,66.0/12), //Blue cone 5
							
							new Target(0,581,120,40,  113.0/12,66.0/12), //Blue box 3
							
							new Target(0,622,120,72,  135.0/12,66.0/12), //Blue cone 6
	
//							new Target(1353,100,87,106,-85/12.0,623/12.0)
//							//add human player pick up
												};
							
							
		
	double m_pathEndx, m_pathEndy;
	
	class BezierPoints{
		double m_p0x;
		double m_p0y;
		double m_angle1;
		double m_l1;
		double m_p3x;
		double m_p3y;
		double m_angle2;
		double m_l2;
		
		public BezierPoints(double p0x, double p0y, double angle1, double l1, double p3x, double p3y, double angle2, double l2) {
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
	
	ArrayList <BezierPoints> m_lastPath; 
	
	
	private BezierQuintic createBezier(double p0x, double p0y, double angle1, double l1, double l3, double p3x, double p3y, double angle2, double l2, double l4) {
		BezierPoints points = new BezierPoints( p0x,  p0y,  angle1,  l1,  p3x,  p3y,  angle2,  l2);
		m_lastPath.add(points);
		
		return new BezierQuintic(p0x, p0y, angle1, l1, l3, p3x, p3y, angle2, l2, l4);
	}
	
	private void createPath() {
		
		m_lastPath = new ArrayList <BezierPoints>();
		
		double xBlueTopCurve = -40/12.0, yBlueTopCurve = 117/12.0; //field Coordinates
		double xBlueBottomCurve = 128/12.0, yBlueBottomCurve = 160/12.0;
//		double xRedTopCurve = -40/12, yRedTopCurve = 651/12.0 - (yBlueTopCurve);
		double xRedBottomCurve = -128/12.0, yRedBottomCurve = (651 / 12.0) - (yBlueBottomCurve);
		double yaw = normalizeAngle(m_yaw);
		double flipPoint = 40;
		double xEndCenter = 49/12;
//		//Map<endX, m_xPos > 
//		HashMap<Point, Double> bezierCurve = new HashMap<>();
//		Point p1 = new Point(0, 0);
//		Point p2 = new Point(1, 1);
//		Point p3 = new Point(2, 0);
//		bezierCurve.put(p1, 0.0);
//		bezierCurve.put(p2, 0.5);
//		bezierCurve.put(p3, 1.0);
		
		
		
		
		if (m_allianceRed) {
			xBlueTopCurve =  -xBlueTopCurve;
			xBlueBottomCurve = -xBlueBottomCurve ;
			xRedBottomCurve = - xRedBottomCurve;
			
			flipPoint = -flipPoint;
			xEndCenter = -xEndCenter;
			
		}
		
		if (yaw < 0 && m_pathEndy > 30 || yaw >= 0 && m_pathEndy <= 30) {
			yaw -= 180;
		}
		
		
		if (m_locked) {
			return;
			
		} else {
			if (m_pathEndy <= 5.5) {
				System.out.println(m_pathEndx);
				if(m_yPos < 140) {
					BezierQuintic bezierBox = createBezier((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 0, m_pathEndx, m_pathEndy,  Math.PI/2, 1,0); //

					m_curvePoints = bezierBox.ComputeSplinePoints(null, 100);
				
				} else if((!m_allianceRed && m_pathEndx < xEndCenter && m_xPos < flipPoint)
						 || (m_allianceRed && m_pathEndx > xEndCenter && m_xPos > flipPoint)) {
					
					BezierQuintic bezierTopCurve = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 1, xBlueTopCurve,yBlueTopCurve,  Math.PI/2, 1,0);
					BezierQuintic bezierBox = createBezier( xBlueTopCurve,yBlueTopCurve, -Math.PI/2,   1,  	2,     m_pathEndx,  m_pathEndy, Math.PI/2, 2, 3); //

					m_curvePoints = bezierTopCurve.ComputeSplinePoints(null, 100);
					m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
					
				} else if (!m_allianceRed && m_pathEndx >= xEndCenter && m_xPos >= flipPoint
						 || m_allianceRed && m_pathEndx <= xEndCenter && m_xPos <= flipPoint) {
					
					BezierQuintic bezierBottomCurve = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 4, xBlueBottomCurve,yBlueBottomCurve,  Math.PI/2, 3,3);
					BezierQuintic bezierBox = createBezier( xBlueBottomCurve,yBlueBottomCurve, -Math.PI/2,   0,  	3,     m_pathEndx,  m_pathEndy, Math.PI/2, 2, 3); //

					m_curvePoints = bezierBottomCurve.ComputeSplinePoints(null, 100);
					m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
					
				} else if (!m_allianceRed && m_pathEndx <= xEndCenter && m_xPos >= flipPoint
						 || m_allianceRed && m_pathEndx >= xEndCenter && m_xPos <= flipPoint) {
					
					BezierQuintic bezierTopCurveExtended = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 4, xBlueTopCurve,yBlueTopCurve,  Math.PI/2, 8,5);
					BezierQuintic bezierBox = createBezier( xBlueTopCurve,yBlueTopCurve, -Math.PI/2,   0,  1,     m_pathEndx,  m_pathEndy, Math.PI/2, 3, 3); //

					m_curvePoints = bezierTopCurveExtended.ComputeSplinePoints(null, 100);
					m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
				} else {
					
					BezierQuintic bezierBottomCurveExtended = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 4, xBlueBottomCurve,yBlueBottomCurve,  Math.PI/2, 0,3);
					BezierQuintic bezierBox = createBezier( xBlueBottomCurve,yBlueBottomCurve, -Math.PI/2,   0,  3,     m_pathEndx,  m_pathEndy, Math.PI/2, 3, 3); //

					m_curvePoints = bezierBottomCurveExtended.ComputeSplinePoints(null, 100);
					m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
				}
					
			}
			
			
//		if(m_pathEndy <= 5.5) { // (66/12) boxes
//			if(m_yPos < 140) { //at 
//			//System.out.println(m_yPos);
//			BezierQuintic bezierBox = createBezier((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 0, m_pathEndx, m_pathEndy,  Math.PI/2, 1,0); //
//
//			m_curvePoints = bezierBox.ComputeSplinePoints(null, 100);
//		
//			}else if((!m_allianceRed && m_xPos <= flipPoint )|| (m_allianceRed && m_xPos >= flipPoint) ) {
//				BezierQuintic bezierTopCurve = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 1, xBlueTopCurve,yBlueTopCurve,  Math.PI/2, 1,0);
//				BezierQuintic bezierBox = createBezier( xBlueTopCurve,yBlueTopCurve, -Math.PI/2,   1,  	2,     m_pathEndx,  m_pathEndy, Math.PI/2, 2, 3); //
//
//				m_curvePoints = bezierTopCurve.ComputeSplinePoints(null, 100);
//				m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
//				//System.out.println(screenWidth);
//			} else {
//				BezierQuintic bezierBottomCurve = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 4, xBlueBottomCurve,yBlueBottomCurve,  Math.PI/2, 3,3);
//				BezierQuintic bezierBox = createBezier( xBlueBottomCurve,yBlueBottomCurve, -Math.PI/2,   0,  	3,     m_pathEndx,  m_pathEndy, Math.PI/2, 2, 3); //
//
//				m_curvePoints = bezierBottomCurve.ComputeSplinePoints(null, 100);
//				m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
//			}
//		}
		//human player intake, change
		if (m_pathEndy >= 601/12.0) { //red side
	
			//if (m_xPos > flipPoint) {
				
			
//			BezierQuintic bezierBox = createBezier((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 0, m_pathEndx, m_pathEndy,  -Math.PI/2, 1,0); //
//
//			m_curvePoints = bezierBox.ComputeSplinePoints(null, 100);
//			//System.out.println("test1");
//			
//				} else {
					BezierQuintic bezierBottomCurve = createBezier ((m_xPos/12), (m_yPos/12),yaw*Math.PI/180,   1, 0, xRedBottomCurve,yRedBottomCurve,  -Math.PI/2, 3,0);
					BezierQuintic bezierBox = createBezier( xRedBottomCurve,yRedBottomCurve, Math.PI/2,   0,  	1,     m_pathEndx,  m_pathEndy, -Math.PI/2, 2, 3); //

					m_curvePoints = bezierBottomCurve.ComputeSplinePoints(null, 100);
					m_curvePoints = bezierBox.ComputeSplinePoints(m_curvePoints, 100);
				//System.out.println("test3");
		 		//}
			}
		}
	}

    double targetXPos;
    double targetYPos;
    double targetHeight;
    double targetExt;
    
    private void sendTargetInfo()
    {	
//    	if (m_targetNo > 8)
//    	{
//    		return;	// MUSTFIX
//    	}
    	Endpoint endpoint = k_endpoints[m_targetNo];
    	double endpointY = m_targetNo == 9 ? k_pickupY : k_endpointsY[m_targetLevel]; //k_endpointsY[m_targetLevel];
    	
        targetXPos = k_targetX[m_targetNo];
        targetYPos = m_targetNo == 9 ? k_targetY[3] : k_targetY[m_targetLevel];
        
        if (isConeTarget())
        {
        	targetHeight = k_targetHCone[m_targetLevel];
        }
        else if (isCubeTarget())
        {
        	targetHeight = k_targetHCube[m_targetLevel];
        }
        else	// Feeder
        {
        	targetHeight = 10;	// MUSTFIX
        }
        
        if (isConeTarget())
        {
        	targetExt = k_targetExtCone[m_targetLevel];
        }
        else if (isCubeTarget())
        {
        	targetExt = k_targetExtCube[m_targetLevel];
        }
        else	// Feeder
        {
        	targetHeight = 0;
        }      
        
//		double m_x0;		// Horz position used to determing if the robot is left or right
//		double m_leftX;		// Horz location of end point in feet when approaching from the left
//		double m_leftAngle;	// Approach angle from the left in degrees
//		double m_rightX;
//		double m_rightAngle;
//		double m_p;
        
		m_network.SendMessage(String.format("T %d %d %f %f %f %d %f %f %f %f %f %f %f %f %f", 
				m_targetNo, m_targetLevel, targetXPos, targetYPos, targetHeight, m_targetType == TargetType.Cone ? 1 : 0, targetExt,
				endpoint.m_x0, endpoint.m_leftX, endpoint.m_leftAngle, endpoint.m_rightX, endpoint.m_rightAngle, endpoint.m_p, endpointY, k_p0));
    }
    
    private boolean isCubeTarget()
    {
    	return (m_targetNo == 1) || (m_targetNo == 4) || (m_targetNo == 7);
    }
    
    private boolean isConeTarget()
    {
    	return (m_targetNo == 0) || (m_targetNo == 2) || (m_targetNo == 3) || (m_targetNo == 5) || (m_targetNo == 6) || (m_targetNo == 8);
    }
    
    private boolean isPlaceTarget()
    {
    	return m_targetNo == 9;
    }
   
    
	private void onClick(MouseEvent event) { 
		m_mouseX = event.getX();
		m_mouseY = event.getY();
		Point mousePos = new Point(m_mouseX, m_mouseY);
		
		System.out.println(String.format("mouse: (%d,%d)", m_mouseX, m_mouseY));
		System.out.println(String.format("Field: (%f,%f)", m_xPos, m_yPos));
		
		int i = 9;
//		int targetNo = -1;
		for (Target target : m_targets) {
			Rectangle r = new Rectangle((int) (target.m_targetRectangle.x*m_panel.m_scale), (int) (target.m_targetRectangle.y*m_panel.m_scale), 
							(int) (target.m_targetRectangle.width*m_panel.m_scale), (int) (target.m_targetRectangle.height*m_panel.m_scale));
			if (r.contains(mousePos)) {
				//System.out.println(String.format("xEnd = %f, yEnd = %f", target.m_xEnd, target.m_yEnd));
				m_pathEndx = target.m_xEnd;
				m_pathEndy = target.m_yEnd;
				m_panel.repaint();
				m_targetNo = i;
				
				if (m_targetLevel == -1)
				{
					setTargetLevel(2);		// Set it to high
				}

				if (isCubeTarget())
				{
					if (m_targetLevel > 0)
					{
						setTargetType(TargetType.Cube);		// Force cube
					}
				}
				else if (isPlaceTarget())
				{
					setTargetType(m_targetType == TargetType.None ? TargetType.Cone : m_targetType);
				}
				else
				{
					if (m_targetLevel > 0)
					{
						setTargetType(TargetType.Cone);		// Force cone
					}
				}

				sendTargetInfo();
								
				break;
			}
			i--;
		}		
	}
	
	boolean m_allianceRed = false;
	private void setAllianceColor(boolean red) {
		if (m_allianceRed != red) {
			
			m_allianceRed = red;
			yFlipCoordinates();
			redCheck.setSelected(red);
//			teeterCordinates();
			
			m_panel.repaint();
			
		}
	}
	
	boolean m_locked = false;
	private void setLocked(boolean lock) {
		if (m_locked != lock) {
			//System.out.print(String.format("m_lock: %f, lock: %f ", m_lock, lock));
			m_locked = lock;
			
			
		}
	}
	
	private void yFlipCoordinates() {
		for (Target target: m_targets) {			
			target.m_xEnd = -target.m_xEnd;			
			target.m_targetRectangle.y = m_panel.m_height - target.m_targetRectangle.y - target.m_targetRectangle.height;			
		}	
		for (int i = 0 ; i < k_targetX.length ; i++)
		{
			k_targetX[i] = -k_targetX[i];
		}
		for (Endpoint endpoint : k_endpoints)
		{
			endpoint.m_x0 = -endpoint.m_x0;
			endpoint.m_leftX = -endpoint.m_leftX;
			endpoint.m_rightX = -endpoint.m_rightX;
			endpoint.m_leftAngle = normalizeAngle(-endpoint.m_leftAngle - 180);
			endpoint.m_rightAngle = normalizeAngle(-endpoint.m_rightAngle - 180);
		}
	}
	
	private double normalizeAngle(double angle) {
		angle = angle % 360;
		if (angle > 180) {
			angle -= 360;
			
		} else if (angle < -180){
			angle += 360;
		}
		return angle;
	}
	
	
	JButtonSZ btnLock;
	JCheckBoxSZ redCheck;
	JLabelSZ message;
	
	private void sendPath(){
		
		m_network.SendMessage(String.format("P %d", m_lastPath.size()));
		for (BezierPoints points: m_lastPath) {
			m_network.SendMessage(String.format("B %f %f %f %f %f %f %f %f", points.m_p0x, points.m_p0y, points.m_angle1, points.m_l1,
																			 points.m_p3x, points.m_p3y, points.m_angle2, points.m_l2));	
		}
		m_network.SendMessage("E");
	}
	
	JRadioButtonSZ level1Button;
	JRadioButtonSZ level2Button;
	JRadioButtonSZ level3Button;
	JRadioButtonSZ coneButton;
	JRadioButtonSZ cubeButton;
	JLabelSZ pieceType;
	
	private void setTargetLevel(int level)
	{
		m_targetLevel = level;
		
		m_panel.repaint();
		
		level1Button.setSelected(level == 0);
		level2Button.setSelected(level == 1);
		level3Button.setSelected(level == 2);
		
		setTargetType(m_targetType);
		
		sendTargetInfo();
	}
	
	enum TargetType
	{
		None,
		Cone,
		Cube
	}
	
	private void setTargetType(TargetType type)
	{
		if (m_targetLevel == -1)
		{
			type = TargetType.None;
		}
		
		if (isConeTarget())
		{
			if (m_targetLevel != 0)
			{
				type = TargetType.Cone;		// Force cone
			}
		}
		else if (isCubeTarget())
		{
			if (m_targetLevel != 0)
			{
				type = TargetType.Cube;		// Force cube
			}
		}
	

		m_targetType = type;
		
		m_panel.repaint();
		
		coneButton.setSelected(type == TargetType.Cone);
		cubeButton.setSelected(type == TargetType.Cube);
		

		if (type == TargetType.None)
		{
			pieceType.setText("None Selected");
			pieceType.setForeground(Color.red);
		}
		else
		{
			if (isPlaceTarget())	// Placing game piece
			{
				pieceType.setText("Pickup " + (type == TargetType.Cone ? "CONE" : "CUBE"));
				pieceType.setForeground(Color.green);
			} 
			else if (type == TargetType.Cone)
			{
				pieceType.setText("Placing CONE");
				pieceType.setForeground(Color.orange);
			}
			else if (type == TargetType.Cube)
			{
				pieceType.setText("Placing CUBE");
				pieceType.setForeground(Color.blue);
			}
			
			sendTargetInfo();
		}
	}
	
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
			
			super.setBounds(x,  y, width, height);			
		}
		
		@Override
		public void setFont(Font font) {
			m_fontHeight = font.getSize();
		}
		
		@Override
		public void resize(int width, int height) {
			super.setBounds( m_x * width / k_designWidth,
							 m_y * height / k_designHeight,
							 m_width  * width / k_designWidth,
							 m_height * height / k_designHeight);
			
			super.setFont(new Font(Font.SERIF, Font.PLAIN,  m_fontHeight * height / k_designHeight));
		}	
	}
	
	private class JRadioButtonSZ extends JRadioButton implements Control
	{
		/**
		 * 
		 */
		private static final long serialVersionUID = -837154600923199949L;
		
		int m_x;
		int m_y;
		int m_width;
		int m_height;
		int m_fontHeight;
		
		JRadioButtonSZ(String title)
		{
			super(title);
			
			m_controls.add(this);
		}
		
	    public void setBounds(int x, int y, int width, int height) {
	    	m_x = x;
	    	m_y = y;
	    	m_width = width;
	    	m_height = height;
	    	
	    	super.setBounds(x,  y,  width, height);
	    }
	    
	    @Override
		public void setFont(Font font) {
	    	super.setFont(font);
	    	
	    	m_fontHeight = font.getSize();
	    }

		@Override
		public void resize(int width, int height) {
			super.setBounds( m_x * width / k_designWidth,
							 m_y * height / k_designHeight,
							 m_width  * width / k_designWidth,
							 m_height * height / k_designHeight);
			
			super.setFont(new Font(Font.SERIF, Font.PLAIN,  m_fontHeight * height / k_designHeight));
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
		
		JCheckBoxSZ(String title)
		{
			super(title);
			
			m_controls.add(this);
		}
		
	    public void setBounds(int x, int y, int width, int height) {
	    	m_x = x;
	    	m_y = y;
	    	m_width = width;
	    	m_height = height;
	    	
	    	super.setBounds(x,  y,  width, height);
	    }
	    
		@Override
		public void resize(int width, int height) {
			super.setBounds( m_x * width / k_designWidth,
					 m_y * height / k_designHeight,
					 m_width  * width / k_designWidth,
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
		
		JButtonSZ(String title)
		{
			super(title);
			
			m_controls.add(this);
		}
		
	    public void setBounds(int x, int y, int width, int height) {
	    	m_x = x;
	    	m_y = y;
	    	m_width = width;
	    	m_height = height;
	    	
	    	super.setBounds(x,  y,  width, height);
	    }
	    
		@Override
		public void resize(int width, int height) {
			super.setBounds( m_x * width / k_designWidth,
					 m_y * height / k_designHeight,
					 m_width  * width / k_designWidth,
					 m_height * height / k_designHeight);			
		}
		
	}
	
	//1536 
	private static final int k_fieldDisplayWidth = 1440;
	private static final int k_designWidth = 1800;
	private static final int k_designHeight = 800;

	private void initialize(String host) {
		//returns size of the screen in pixels, assigns the height and width
		
//       	screenWidth = 1800;
//       	screenHeight = 800;
       	
       	Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
       	
		frame = new JFrame();
		frame.setBounds(0, 0, screenSize.width - 10, screenSize.height - 50); //sets the frame of the window created
//		frame.setBounds(0, 0, k_designWidth, k_designHeight); //sets the frame of the window created
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.getContentPane().setLayout(null);
		
		frame.addComponentListener(new ComponentListener() {
			@Override
			public void componentResized(ComponentEvent e) {
				// TODO Auto-generated method stub
		          int w = e.getComponent().getWidth();
		          int h = e.getComponent().getHeight();	
		          
		          for (Control control : m_controls)
		          {
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
		
//		JLabel label = new JLabel();
//		label.setText(String.format("Field x: %f, Field y: %f", m_xPos, m_yPos));
//		m_panel.add(label);
	
		btnLock = new JButtonSZ("Lock");
		btnLock.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setLocked(!m_locked);
					btnLock.setText(m_locked ? "Unlock" : "Lock");
					if (m_locked) {
						sendPath();
					
					}
					
				}
			}
			
		}); 
		
		btnLock.setBounds(100, (m_screenHeight-100), 140, 23);
		frame.getContentPane().add(btnLock);
		
		level1Button = new JRadioButtonSZ("Low");
		level1Button.setBounds(k_fieldDisplayWidth+10, 130, 300, 60);
		level1Button.setFont(new Font(Font.SERIF, Font.PLAIN,  48));
		level1Button.setSelected(true);
		frame.getContentPane().add(level1Button);
		level1Button.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setTargetLevel(0);			
				}
			}			
		});	
		
		level2Button = new JRadioButtonSZ("Medium");
		level2Button.setBounds(k_fieldDisplayWidth+10, 70, 300, 60);
		level2Button.setFont(new Font(Font.SERIF, Font.PLAIN,  48));
		frame.getContentPane().add(level2Button);
		level2Button.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setTargetLevel(1);			
				}
			}
			
		});	
		
		level3Button = new JRadioButtonSZ("High");
		level3Button.setBounds(k_fieldDisplayWidth+10, 10, 300, 60);
		level3Button.setFont(new Font(Font.SERIF, Font.PLAIN,  48));
		frame.getContentPane().add(level3Button);
		level3Button.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setTargetLevel(2);			
				}
			}
			
		});	
		
		coneButton = new JRadioButtonSZ("Cone");
		coneButton.setBounds(k_fieldDisplayWidth+10, 210, 300, 60);
		coneButton.setFont(new Font(Font.SERIF, Font.PLAIN,  48));
		coneButton.setSelected(false);
		frame.getContentPane().add(coneButton);
		coneButton.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setTargetType(TargetType.Cone);			
				}
			}			
		});	
		
		cubeButton = new JRadioButtonSZ("Cube");
		cubeButton.setBounds(k_fieldDisplayWidth+10, 270, 300, 60);
		cubeButton.setFont(new Font(Font.SERIF, Font.PLAIN,  48));
		cubeButton.setSelected(false);
		frame.getContentPane().add(cubeButton);
		cubeButton.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setTargetType(TargetType.Cube);			
				}
			}			
		});	

		
		pieceType = new JLabelSZ("None Selected");
		pieceType.setBounds(k_fieldDisplayWidth+10, 360, 400, 54);
		pieceType.setFont(new Font(Font.SERIF, Font.PLAIN, 48));
		pieceType.setForeground(Color.red);
		frame.getContentPane().add(pieceType);
		
		JLabelSZ reach = new JLabelSZ("Reach");
		reach.setBounds(80, 600, 100, 30);
		reach.setFont(new Font(Font.SERIF, Font.PLAIN, 24));
		frame.getContentPane().add(reach);
		
		JButtonSZ button1 = new JButtonSZ("++++");
		button1.setBounds(10, 640, 100, 30);
		frame.getContentPane().add(button1);
		button1.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 1");		
				}
			}			
		});	
		
		JButtonSZ button2 = new JButtonSZ("----");
		button2.setBounds(120, 640, 100, 30);
		frame.getContentPane().add(button2);
		button2.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 2");		
				}
			}			
		});	
		
		JLabelSZ wrist = new JLabelSZ("Wrist");
		wrist.setBounds(80+220, 600, 100, 30);
		wrist.setFont(new Font(Font.SERIF, Font.PLAIN, 24));
		frame.getContentPane().add(wrist);
		
		JButtonSZ button3 = new JButtonSZ("++++");
		button3.setBounds(10+220, 640, 100, 30);
		frame.getContentPane().add(button3);
		button3.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 3");		
				}
			}			
		});	
		
		JButtonSZ button4 = new JButtonSZ("----");
		button4.setBounds(120+220, 640, 100, 30);
		frame.getContentPane().add(button4);
		button4.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 4");		
				}
			}			
		});	
		
		JLabelSZ arm = new JLabelSZ("Arm");
		arm.setBounds(80+440, 600, 100, 30);
		arm.setFont(new Font(Font.SERIF, Font.PLAIN, 24));
		frame.getContentPane().add(arm);
		
		JButtonSZ button5 = new JButtonSZ("++++");
		button5.setBounds(10+440, 640, 100, 30);
		frame.getContentPane().add(button5);
		button5.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 5");		
				}
			}			
		});	
		
		JButtonSZ button6 = new JButtonSZ("----");
		button6.setBounds(120+440, 640, 100, 30);
		frame.getContentPane().add(button6);
		button6.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					m_network.SendMessage("B 6");		
				}
			}			
		});	
		
		message = new JLabelSZ("Message");
		message.setBounds(10, k_designHeight - 70, 1500, 24);
		message.setFont(new Font(Font.SERIF, Font.PLAIN, 16));
		frame.getContentPane().add(message);
		
		redCheck = new JCheckBoxSZ("Red");
		redCheck.addActionListener(new ActionListener()
		{ 
			public void actionPerformed(ActionEvent arg0)
			{
				synchronized(m_lock)
				{
					setAllianceColor(!m_allianceRed);			
				}
			}
			
		}); 
		redCheck.setBounds(10, (k_designHeight-100), 100, 23);
		frame.getContentPane().add(redCheck);
		
		JButtonSZ btnSave = new JButtonSZ("Clear");
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
		btnSave.setBounds(240, (m_screenHeight-100), 140, 23);
		frame.getContentPane().add(btnSave);
		
		m_panel = new CustomPanel();
		m_panel.addMouseListener(new MouseAdapter() {
			@Override
			public void mouseReleased(MouseEvent event) 
			{
				if (event.getButton() == MouseEvent.BUTTON3)
				{
					int	x	= event.getX();
					int	y	= event.getY();
					double	dx = x - m_mouseX;
					double	dy = y - m_mouseY;
					
					if (dx != 0)
					{
						m_yaw = (int) (Math.atan2(dx, dy) * 180 / Math.PI);
					}
					m_xPos = m_panel.isx(m_mouseY);
					m_yPos = m_panel.isy(m_mouseX);
					m_visible = true;
					
					m_panel.repaint();
					
				}
				else
				{
					onClick(event);
					message.setText(String.format("%d,%d", event.getX(), event.getY()));
				}
			}
			
			@Override
			public void mouseMoved(MouseEvent event)
			{
				message.setText(String.format("%d,%d", event.getX(), event.getY()));
			}
			
			
			@Override
			public void mousePressed(MouseEvent event)
			{
				m_mouseX = event.getX();
				m_mouseY = event.getY();
			}
			@Override
			public void mouseClicked(MouseEvent event) {

			}
		});
		
		//changed sizing of bounds to fit screen
		//System.out.print(screenWidth);
		
		m_panel.setBounds(0, 0, (k_fieldDisplayWidth), (int) ((k_fieldDisplayWidth) * k_fieldWidth / k_fieldLength));
		frame.getContentPane().add(m_panel);
		m_panel.computeScale();
					
		System.out.println("host = " + host);
		
		m_network.Connect(host, 5802);
	}
}

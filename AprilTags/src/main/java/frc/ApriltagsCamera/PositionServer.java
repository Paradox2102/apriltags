package frc.ApriltagsCamera;

import java.util.Timer;
import java.util.TimerTask;

import edu.wpi.first.wpilibj.interfaces.Gyro;
import frc.ApriltagsCamera.Network.NetworkReceiver;

public class PositionServer implements NetworkReceiver {
    private Network m_network = new Network();
    private Timer m_watchdogTimer = new Timer();
    private boolean m_connected = false;
    private double m_xPos = 0;
    private double m_yPos = 0;
    private boolean m_newPos = true;
    private Object m_lock = new Object();
    private Gyro m_gyro;

    public void start(Gyro gyro) {
        m_gyro = gyro;
        m_network.listen(this, 5802);

        m_watchdogTimer.scheduleAtFixedRate(new TimerTask() {

			@Override
			public void run() {
                // Logger.log("PositionServer", 1, "connected=" + m_connected);
				if (m_connected) {
					Logger.log("PositionServer",-1, "Send position");

                    double xPos;
                    double yPos;
                    boolean newPos;

                    synchronized (m_lock)
                    {
                        xPos = m_xPos;
                        yPos = m_yPos;
                        newPos = m_newPos;

                        m_newPos = false;
                    }

                    if (newPos)
                    {
					    m_network.sendMessage(String.format("+%.2f %.2f %.2f\n", m_gyro.getAngle(), xPos, yPos));
                    }
                    else
                    {
                        m_network.sendMessage("-\n");       // keep alive
                    }

					// if (m_lastMessage + k_timeout < System.currentTimeMillis()) {
					// 	Logger.log("ApriltagsCamera", 3, "Network timeout");
					// 	m_network.closeConnection();
					// }
				}
			}
		}, 200, 200);   // Send current position 5 times a second
    }

    public void setPosition(double x, double y) {
       synchronized (m_lock)
       {
           m_xPos = x;
           m_yPos = y;
           m_newPos = true;
       }
    }

    @Override
    public void processData(String command) {

    }

    @Override
    public void connected() {
        Logger.log("PositionServer", 1, "connected");

        m_connected = true;
    }

    @Override
    public void disconnected() {
        Logger.log("PositionServer", 1, "disconnected");

        m_connected = false;
    }

}

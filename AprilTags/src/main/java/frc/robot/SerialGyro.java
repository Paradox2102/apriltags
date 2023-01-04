package frc.robot;

import edu.wpi.first.wpilibj.SerialPort;
import edu.wpi.first.wpilibj.interfaces.Gyro;

public class SerialGyro implements Gyro {
    double m_yaw = 0;
    double m_zero = 0;
    String m_line = "";
    Object m_lock = new Object();
    SerialPort serial = new SerialPort(115200, SerialPort.Port.kMXP, 8, SerialPort.Parity.kNone);

    public SerialGyro() {
        System.out.println("SerialGyro");

        serial.reset();

        (new Thread() {
            public void run() {
                while (true) {
                    // System.out.println(String.format("count-%d", serial.getBytesReceived()));

                    if (serial.getBytesReceived() > 0)
                    {
                        String line = serial.readString();

                        String[] lines = line.split("\n");

                        // System.out.println("Lines");
                        for (int i = 0 ; i < lines.length ; i++)
                        {
                            // System.out.println(m_lines[i]);

                            try {
                                int value = Integer.parseInt(lines[i].trim());
    
                                synchronized (m_lock)
                                {
                                    m_yaw = -value / 100.0;
                                }
                            }
                            catch (NumberFormatException e)
                            {
                                e.printStackTrace();
                            }
                        }


                    }
                    else
                    {
                        try {
                            sleep(10);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                }
            }
        }).start();
    }

    @Override
    public void close() throws Exception {
    }

    @Override
    public void calibrate() {
    }

    @Override
    public void reset() {
        synchronized(m_lock)
        {
            m_zero = m_yaw;
        }
    }

    public void reset(double yaw)
    {
        synchronized(m_lock)
        {
            m_zero = m_yaw - yaw;
        }
    }

    @Override
    public double getAngle() {
        synchronized(m_lock)
        {
            return(m_yaw - m_zero);
        }
    }

    @Override
    public double getRate() {
        return 0;
    }

}

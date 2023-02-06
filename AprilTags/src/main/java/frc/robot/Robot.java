// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

package frc.robot;

import edu.wpi.first.wpilibj.DriverStation;
import edu.wpi.first.wpilibj.TimedRobot;
import edu.wpi.first.wpilibj.smartdashboard.SmartDashboard;
import edu.wpi.first.wpilibj2.command.Command;
import edu.wpi.first.wpilibj2.command.CommandScheduler;
import frc.ApriltagsCamera.ApriltagLocation;
import frc.ApriltagsCamera.ApriltagsCamera;
import frc.ApriltagsCamera.Logger;
import frc.ApriltagsCamera.PositionServer;
import frc.ApriltagsCamera.ApriltagsCamera.ApriltagsCameraRegion;
import frc.ApriltagsCamera.ApriltagsCamera.ApriltagsCameraRegions;
import frc.ApriltagsCamera.ApriltagsCamera.ApriltagsCameraRegions.RobotPos;

/**
 * The VM is configured to automatically run this class, and to call the functions corresponding to
 * each mode, as described in the TimedRobot documentation. If you change the name of this class or
 * the package after creating this project, you must also update the build.gradle file in the
 * project.
 */
public class Robot extends TimedRobot {
  private Command m_autonomousCommand;

  private RobotContainer m_robotContainer;
  private ApriltagsCamera m_camera = new ApriltagsCamera();
  private SerialGyro m_gyro = new SerialGyro();
  private PositionServer m_posServer = new PositionServer();

  /**
   * This function is run when the robot is first started up and should be used for any
   * initialization code.
   */
  @Override
  public void robotInit() {
    // Instantiate our RobotContainer.  This will perform all our button bindings, and put our
    // autonomous chooser on the dashboard.
    m_robotContainer = new RobotContainer();
    m_gyro.reset(0);
    m_camera.connect("10.21.2.10", 5800);
    m_posServer.start(m_gyro);
    Logger.log("Robot", 1, "Position server started");
  }

  /**
   * This function is called every 20 ms, no matter the mode. Use this for items like diagnostics
   * that you want ran during disabled, autonomous, teleoperated and test.
   *
   * <p>This runs after the mode specific periodic functions, but before LiveWindow and
   * SmartDashboard integrated updating.
   */
  int m_frameCount = 0;
  int m_missingCount = 0;
  int m_invalidCount = 0;

  ApriltagLocation tags[] = { new ApriltagLocation(12, 0, 324), new ApriltagLocation(0, 12, 324) };

  @Override
  public void robotPeriodic() {
    // Runs the Scheduler.  This is responsible for polling buttons, adding newly-scheduled
    // commands, running already-scheduled commands, removing finished or interrupted commands,
    // and running subsystem periodic() methods.  This must be called from the robot's periodic
    // block in order for anything in the Command-based framework to work.
    CommandScheduler.getInstance().run();

    SmartDashboard.putNumber("yaw", m_gyro.getAngle());
  
    ApriltagsCameraRegions regions = m_camera.getRegions();

    m_posServer.setAllianceColor(DriverStation.getAlliance() == DriverStation.Alliance.Red);
    
    if (regions != null) {
      Logger.log("Robot", -1, String.format("nRegions = %d", regions.m_regions.size()));
      SmartDashboard.putNumber("nRegions", regions.m_regions.size());
      SmartDashboard.putNumber("nFrames", ++m_frameCount);
      SmartDashboard.putNumber("Delay", System.currentTimeMillis() - regions.m_captureTime);
      SmartDashboard.putNumber("FPS", regions.m_fps);

      if (regions.m_regions.size() == 0) {
        SmartDashboard.putNumber("missing", ++m_missingCount);
      }

      // RobotPos pos = regions.ComputeRobotPosition(tags, m_gyro.getAngle() * Math.PI / 180);
      // if (pos != null)
      // {
      //   SmartDashboard.putNumber("xPos", pos.m_x);
      //   SmartDashboard.putNumber("yPos", pos.m_y);
      //   m_posServer.setPosition(pos.m_x, pos.m_y);
      // }

      int i = 1;
      for (ApriltagsCameraRegion region : regions.m_regions) {
        SmartDashboard.putNumber(String.format("Tag%d", i), region.m_tag);
        SmartDashboard.putNumber(String.format("Dist%d", i), Math.sqrt(region.m_tvec[0] * region.m_tvec[0] +
                                                                       region.m_tvec[1] * region.m_tvec[1] +
                                                                       region.m_tvec[2] * region.m_tvec[2]));
        SmartDashboard.putNumber(String.format("rx%d", i), region.m_rvec[0]);                                                                    
        SmartDashboard.putNumber(String.format("ry%d", i), region.m_rvec[1]);                                                                    
        SmartDashboard.putNumber(String.format("rz%d", i), region.m_rvec[2]);                                                                    
        if (region.m_tag < 0) {
          SmartDashboard.putNumber("invalid", ++m_invalidCount);
        }
        i++;
      }
    }
  }

  /** This function is called once each time the robot enters Disabled mode. */
  @Override
  public void disabledInit() {
  }

  @Override
  public void disabledPeriodic() {
  }

  /** This autonomous runs the autonomous command selected by your {@link RobotContainer} class. */
  @Override
  public void autonomousInit() {
    m_autonomousCommand = m_robotContainer.getAutonomousCommand();

    // schedule the autonomous command (example)
    if (m_autonomousCommand != null) {
      m_autonomousCommand.schedule();
    }
  }

  /** This function is called periodically during autonomous. */
  @Override
  public void autonomousPeriodic() {
  }

  @Override
  public void teleopInit() {
    // This makes sure that the autonomous stops running when
    // teleop starts running. If you want the autonomous to
    // continue until interrupted by another command, remove
    // this line or comment it out.
    if (m_autonomousCommand != null) {
      m_autonomousCommand.cancel();
    }
  }

  /** This function is called periodically during operator control. */
  @Override
  public void teleopPeriodic() {
  }

  @Override
  public void testInit() {
    // Cancels all running commands at the start of test mode.
    CommandScheduler.getInstance().cancelAll();
  }

  /** This function is called periodically during test mode. */
  @Override
  public void testPeriodic() {
  }

  /** This function is called once when the robot is first started up. */
  @Override
  public void simulationInit() {
  }

  /** This function is called periodically whilst in simulation. */
  @Override
  public void simulationPeriodic() {
  }
}

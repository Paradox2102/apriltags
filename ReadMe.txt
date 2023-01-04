This folder contains all of the source code for the Apriltags processing program described at:

http://programming.team2102.org/apriltags/

1/4/2023 Changes:

1) Improved tag detection for the fast implementation, particularly in low light conditions
2) Improved the accuracy of the computation of the rotation and translation vectors (rvec & tvec)
3) Fixed a problem with the camera calibration for the 1080P resolution using the standard Pi camera
4) Added some code to the FRC robot project to compute the absolute position of the robot
   based on the tvec returned from the tag and the direction of the robot as provided by a gyro
5) Added a position server within the FRC robot project that can send the robot's position
   to another device.
6) Added a Java app which can connect to the position server and display the position of the
   robot in real time.

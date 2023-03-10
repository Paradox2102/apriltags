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

/**
 * This program takes a set of corresponding 2D and 3D points and finds the transformation matrix
 * that best brings the 3D points to their corresponding 2D points.
 */
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <iostream>
#include <string>

#include "opencv.h"

std::vector<cv::Point2f> Generate2DPoints(double corners[4][2]);
std::vector<cv::Point3f> Generate3DPoints();

int solvePnP(double corners[4][2], double * rotVec, double * transVec)
{
    // Generate points
    std::vector<cv::Point2f> imagePoints = Generate2DPoints(corners);
    std::vector<cv::Point3f> objectPoints = Generate3DPoints();

#ifdef R1080
    cv::Mat cameraMatrix = (cv::Mat_<double>(3, 3) <<
        1.7039940543172036e+03, 0., 9.5950000000000000e+02,
        0, 1.7039940543172036e+03, 5.3950000000000000e+02,
        0, 0, 1);

//    1.7039940543172036e+03 0. 9.5950000000000000e+02 0.
//    1.7039940543172036e+03 5.3950000000000000e+02 0. 0. 1.

    cv::Mat distCoeffs = (cv::Mat_<double>(5, 1) <<
        1.0850362700126798e-01, -6.3809660084095887e-01, 0, 0,
        8.8167944235304774e-01
    );

//        1.0850362700126798e-01 -6.3809660084095887e-01 0. 0.
//    8.8167944235304774e-01
#else   // !R1080
    cv::Mat cameraMatrix = (cv::Mat_<double>(3, 3) <<
        1.1449297728909708e+03, 0., 6.3950000000000000e+02,
        0, 1.1449297728909708e+03, 3.9950000000000000e+02,
        0, 0, 1);

    cv::Mat distCoeffs = (cv::Mat_<double>(5, 1) <<
        -4.6114705435713005e-01, 9.8967543464882485e-02, 0, 0,
        2.3598884818691687e-01
    );
#endif // !R1080

    cv::Mat rvec(3, 1, cv::DataType<double>::type);
    cv::Mat tvec(3, 1, cv::DataType<double>::type);

    cv::solvePnP(objectPoints, imagePoints, cameraMatrix, distCoeffs, rvec, tvec);

    for (int i = 0 ; i < 3 ; i++)
    {
        rotVec[i] = rvec.at<double>(i);
        transVec[i] = tvec.at<double>(i);
    }

    return 0;
}

std::vector<cv::Point2f> Generate2DPoints(double corners[4][2])
{
    std::vector<cv::Point2f> points;

    for (int i = 0; i < 4; i++)
    {
        points.push_back(cv::Point2f(corners[i][0], corners[i][1]));
    }

    return points;
}

//#define scale  1.0
//#define scale  (92.0 / 94.5)
#define scale (102.5 / 106.0)

std::vector<cv::Point3f> Generate3DPoints()
{
    std::vector<cv::Point3f> points;

    points.push_back(cv::Point3f(-3*scale, -3*scale, 0));
    points.push_back(cv::Point3f(3*scale, -3*scale, 0));
    points.push_back(cv::Point3f(3*scale, 3*scale, 0));
    points.push_back(cv::Point3f(-3*scale, 3*scale, 0));

    return points;
}


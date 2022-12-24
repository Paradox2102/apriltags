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

#include <mutex>
#include <cstdlib>
#include "ProcessApriltags.h"

#include "../../apriltag/apriltag.h"
#include "../../apriltag/tag36h11.h"
#include "../../apriltag/tag16h5.h"

/***************
 * 
 * This finds the Apriltags in an image using the original Apriltags implementation
 * 
 */

void SaveImageToFile (unsigned char *data, int width, int height, int stride)
{
    FILE * fp = fopen("test.ppm", "wb");

    printf("SaveImageToFIle: fp=%u\n", (unsigned) fp);

    if (fp == NULL)
    {
        printf("Cannot create File\n");
        return;
    }

    fprintf(fp, "P5\n%d %d 255", width, height);


    printf("SaveImageToFile: width = %d, height = %d\n", width, height);
}

image_u8_t * ConvertToU8(unsigned char * pData, int width, int height)
{
    image_u8_t *im = image_u8_create(width, height);

    for (int y = 0 ; y < height ; y++)
    {
        memmove(im->buf + y * im->stride, pData + (y * width), width);
    }

    return(im);
}

void DumpDetections(zarray_t* detections)
{
    for (int i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        printf("detection %3d: id (%2dx%2d)-%-4d, hamming %d, margin %8.3f\n",
                i, det->family->nbits, det->family->h, det->id, det->hamming, det->decision_margin);
    }

}

apriltag_detector_t* g_td = NULL;

float g_decimate = 4;
float g_sigma = 1;
bool g_refineEdges = true;
int g_nThreads = 4;
bool g_changed = true;

void setDecimate(float decimate)
{
    g_decimate = decimate;
    g_changed = true;
}

void setNThreads(int nThreads)
{
    g_nThreads = nThreads;
    g_changed = true;
}

void setRefine(bool refine)
{
    g_refineEdges = refine;
    g_changed = true;
}

void setSigma(float sigma)
{
    g_sigma = sigma;
    g_changed = true;
}

apriltag_detector_t* getDetector()
{
    if ((g_td == NULL) || g_changed)
    {
        if (g_td != NULL)
        {
            apriltag_detector_destroy(g_td);
        }

        g_td = apriltag_detector_create();
        g_changed = false;

        g_td->quad_decimate = g_decimate;
        g_td->quad_sigma = g_sigma;
        g_td->nthreads = g_nThreads;
        // g_td->debug = getopt_get_bool(getopt, "debug");
        g_td->refine_edges = g_refineEdges;

        printf("dec=%f,sig=%f,nth=%d,refe=%d\n", g_td->quad_decimate, g_td->quad_sigma, g_td->nthreads, g_td->refine_edges);

        // apriltag_family_t* tf = tag36h11_create();
        apriltag_family_t* tf = tag16h5_create();
        apriltag_detector_add_family_bits(g_td, tf, 0);
    }

    return g_td;
}

extern std::list<ImageRegion*>* ProcessAprilTags(unsigned char * pData, int width, int height)
{
    image_u8_t* im = ConvertToU8(pData, width, height);
    std::list<ImageRegion*>* pList = new std::list<ImageRegion*>();

    apriltag_detector_t* td = getDetector();

    zarray_t* detections = apriltag_detector_detect(td, im);
    // timeprofile_display(td->tp);

    // DumpDetections(detections);
    // timeprofile_display(td->tp);

    for (int i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        double corners[4][2];
        for (int i = 0 ; i < 4 ; i++)
        {
            corners[i][0] = det->p[3-i][0];
            corners[i][1] = det->p[3-i][1];
        }
        ImageRegion * pRegion = new ImageRegion(det->id, corners);

        pList->push_back(pRegion);
    }

        // Cleanup.
    image_u8_destroy(im);

    return(pList);
}


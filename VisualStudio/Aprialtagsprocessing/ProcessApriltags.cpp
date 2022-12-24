#include "ProcessApriltags.h"

#include "../../../apriltag/apriltag.h"
#include "../../../apriltag/tag36h11.h"

void SaveImageToFile (unsigned char *data, int width, int height, int stride)
{
    FILE * fp = fopen("test.ppm", "wb");

    printf("SaveImageToFIle: fp=%u\n", fp);

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

    // printf("Convert: stride=%d,im->stride=%d\n", stride, im->stride);

    for (int y = 0 ; y < height ; y++)
    {
        // unsigned char * s = pData + (y * width);
        // unsigned char * d = im->buf + y * im->stride;

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

uint64_t time0 = 0;
apriltag_detector_t* td = NULL;

extern std::list<ImageRegion*>* ProcessAprilTags(unsigned char * pData, int width, int height)
{
    image_u8_t* im = ConvertToU8(pData, width, height);
    std::list<ImageRegion*>* pList = new std::list<ImageRegion*>();

    uint64_t time = utime_now() / 1000;

    if (time0 == 0)
    {
        time0 = time;
    }

    // printf("%d: ProcessAprilTags: width=%d, height=%d, stride=%d\n", (int) (time - time0), width, height, stride);
    // printf("Rate: %d\n", (int) (time - time0));
    time0 = time;

    // return;

    if (td == NULL)
    {
        td = apriltag_detector_create();

        td->quad_decimate = 2;
        // td->quad_sigma = getopt_get_double(getopt, "blur");
        td->nthreads = 4; // getopt_get_int(getopt, "threads");
        // td->debug = getopt_get_bool(getopt, "debug");
        // td->refine_edges = getopt_get_bool(getopt, "refine-edges");

        // printf("dec=%f,sig=%f,nth=%d,refe=%d\n", td->quad_decimate, td->quad_sigma, td->nthreads, td->refine_edges);

        apriltag_family_t* tf = tag36h11_create();
        apriltag_detector_add_family(td, tf);
    }

    zarray_t* detections = apriltag_detector_detect(td, im);
    // timeprofile_display(td->tp);

    // DumpDetections(detections);
    // timeprofile_display(td->tp);

    for (int i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        int x = 0;

        // printf("Add Region %d\n", i);
        ImageRegion * pRegion = new ImageRegion(det->id, det->p);

        pList->push_back(pRegion);
    }
        // Cleanup.
    //    tag36h11_destroy(tf);
    // apriltag_detector_destroy(td);
    image_u8_destroy(im);
    // printf("ProcessAprialTags: return\n");

    return(pList);
}

// void deleteImageRegions(std::list<ImageRegion*>* pRegions)
// {
//     if (pRegions)
//     {
//         for (ImageRegion* pRegion : *pRegions)
//         {
//             delete pRegion;
//         }

//         delete pRegions;
//     }
// }
#ifdef __cplusplus
extern "C" {
#endif

extern int initializeCamera(int shutterSpeed, int gain);
extern void grabImage();
extern int captureImage(unsigned char * out_buffer, int out_buffer_size);
extern int setShutterSpeed(int speed);

extern void setGain(int gain);
#ifndef INNOMAKER
extern void grabImage();
extern void setContrast(int contrast);
extern void setAutoExposure(bool autoExp);
extern void setBrightness(int brightness);
#endif // !INNOMAKER

#ifdef __cplusplus
}
#endif

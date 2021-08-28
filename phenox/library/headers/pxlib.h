/*
Copyright (c) 2014 Ryo Konomura

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef	__PXLIB_H___
#define	__PXLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cv.h"
#include "cxcore.h"
#include "highgui.h"

  typedef struct _px_operate{
    int mode;
    float vision_dtx;
    float vision_dty;
    float sonar_dtz;
    float dangx;
    float dangy;
    float dangz;
    int led[2];
    int buzzer;
    int fix_selfposition_query;
    float fix_selfposition_tx;
    float fix_selfposition_ty;
  } px_operate;

  typedef struct _px_private{
    int ready;
    int startup;
    int selxy;
    int dangz_rotbusy;
    int keepalive;
    int ax;
    int ay;
    int az;
    int gx;
    int gy;
    int gz;
  } px_private;

  typedef struct _px_pconfig{
    float duty_hover;
    float duty_hover_max;
    float duty_hover_min;
    float duty_up;
    float duty_down;
    float duty_bias_front;
    float duty_bias_back; 
    float duty_bias_left;
    float duty_bias_right;
    float pgain_vision_tx;
    float pgain_vision_ty;
    float dgain_vision_tx;
    float dgain_vision_ty;
    float pgain_sonar_tz;
    float dgain_sonar_tz;
    int whisleborder;
    int soundborder;
    float uptime_max;
    float downtime_max;
    float dangz_rotspeed;
    int featurecontrast_front;
    int featurecontrast_bottom;
    float pgain_degx;
    float pgain_degy;
    float pgain_degz;
    float dgain_degx;
    float dgain_degy;
    float dgain_degz;
    int pwm_or_servo;
    int propeller_monitor;
  } px_pconfig;
  
  typedef struct _px_selfstate{
    float degx;
    float degy;
    float degz;
    float vision_tx;
    float vision_ty;
    float vision_tz;
    float vision_vx;
    float vision_vy;
    float vision_vz;
    float height;
    int battery;
  } px_selfstate;

  typedef struct _px_blobmark {
    float cx;
    float cy;
    float size;    
  } px_blobmark;

  typedef struct _px_blobmark_config {

    int min_y;
    int min_u;
    int min_v;
    int max_y;
    int max_u;
    int max_v;
    int state;
    int cam;
    int num;
  } px_blobmark_config;

  typedef struct _px_imgfeature{
    float pcx;
    float pcy;
    float cx;
    float cy;
  } px_imgfeature;

  typedef enum {
    PX_HALT,
    PX_UP,
    PX_HOVER,
    PX_DOWN
  } px_flymode;

  typedef enum {
    PX_FRONT_CAM,
    PX_BOTTOM_CAM
  } px_cameraid;


  //1.basic
  void pxinit_chain();
  void pxclose_chain();
  int  pxget_cpu1ready();
  int  pxget_motorstatus();
  void pxget_pconfig(px_pconfig *param);
  void pxset_pconfig(px_pconfig *param);
  void pxget_selfstate(px_selfstate *state);
  void pxset_keepalive();
  //2.auto_control
  void pxset_operate_mode(px_flymode mode);
  px_flymode  pxget_operate_mode();
  void pxset_visioncontrol_xy(float tx,float ty);
  void pxset_rangecontrol_z(float tz);
  void pxset_dst_degx(float val);
  void pxset_dst_degy(float val);
  void pxset_dst_degz(float val);
  int pxset_visualselfposition(float tx,float ty);
  //3.image processing
  //a.raw image
  int pxget_imgfullwcheck(px_cameraid cam,IplImage **img);
  void pxset_img_seq(px_cameraid cam);
  //b.image feature points
  int pxset_imgfeature_query(px_cameraid cam);
  int pxget_imgfeature(px_imgfeature *ft,int maxnum);
  //c.image color blob
  int pxset_blobmark_query(px_cameraid cam,float min_y,float max_y,float min_u,float max_u,float min_v,float max_v);
  int pxget_blobmark(float *x,float *y, float *size);
  //4.sound processing
  int pxget_whisle_detect();
  void pxset_whisle_detect_reset();
  int pxget_sound_recordstate();
  int pxset_sound_recordquery(float recordtime);
  int pxget_sound(short *buffer,float recordtime);
  //5.logger and indicator
  void pxset_led(int led,int state);
  void pxset_buzzer(int state);
  void pxset_systemlog();
  int pxget_battery();

  void pxget_imgraw_yuv(unsigned char **copybuf);
  int pxget_imgcopy_yuv(unsigned char **copybuf, IplImage **img);

  void pxget_imu(int *ax,int *ay,int *az,int *gx,int *gy,int *gz);

#ifdef __cplusplus
}
#endif

#endif

/*
Copyright (c) 2015 Ryo Konomura

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include "pxlib.h"
#include "parameter.h"

static char timer_disable = 0;

static void timerhandler(int i);
static void setup_timer();
static int ftnum = 0;
const int ftmax = 200;
static int once = 0;

const px_cameraid cameraid = PX_BOTTOM_CAM;

//configurable parameter by user
static float dst_tx = 0;
static float dst_ty = 300;
static float speed_tx = 50;
static float speed_ty = 50;

int main(int argc, char **argv)
{
  pxinit_chain();
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

  px_imgfeature *ft =(px_imgfeature *)calloc(ftmax,sizeof(px_imgfeature));
  int ftstate = 0;

  while(1) {
    if(ftstate == 0) {
      if(pxset_imgfeature_query(cameraid) == 1)
	ftstate = 1;
    }
    else if(ftstate == 1) {
      int res = pxget_imgfeature(ft,ftmax);
      if(res >= 0) {
	ftnum = res;
	ftstate = 0;      
      }
    }
    usleep(1000);
  }
}

static void setup_timer() {
  struct sigaction action;
  struct itimerval timer;
  
  memset(&action, 0, sizeof(action));
    
  action.sa_handler = timerhandler;
  action.sa_flags = SA_RESTART;
  sigemptyset(&action.sa_mask);
  if(sigaction(SIGALRM, &action, NULL) < 0){
    perror("sigaction error");
    exit(1);
  }
  
  /* set intarval timer (10ms) */
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 10000;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 10000;
  if(setitimer(ITIMER_REAL, &timer, NULL) < 0){
    perror("setitimer error");
    exit(1);
  }
}

void timerhandler(int i) {
  char c;  

  if(timer_disable == 1) {
    return;
  }

  pxset_keepalive();
  pxset_systemlog();
  
  px_selfstate st;
  pxget_selfstate(&st);
  
  static unsigned long msec_cnt = 0;
  msec_cnt++;
  if(!(msec_cnt % 3)){
    printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
  } 

  static int time_start = 0;
  static float origin_tx = 0;
  static float origin_ty = 0;
  static float start_tx = 0;
  static float start_ty = 0;
  static int mfin_tx = 0;
  static int mfin_ty = 0;

  static int prev_operatemode = PX_HALT;
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    origin_tx = st.vision_tx;
    origin_ty = st.vision_ty;
    pxset_visioncontrol_xy(origin_tx,origin_ty);
    time_start = 1;
  }
  prev_operatemode = pxget_operate_mode();  


  static int time = 0;
  if(time == 800) {    
    start_tx = st.vision_tx;
    start_ty = st.vision_ty;
    time++;
  }
  else if(time == 801) {
    float pos_tx = st.vision_tx - start_tx;
    float pos_ty = st.vision_ty - start_ty;
    float dist_tx = dst_tx - pos_tx; 
    float dist_ty = dst_ty - pos_ty; 
    float sign_tx = (dist_tx > 0)? 1:-1;
    float sign_ty = (dist_ty > 0)? 1:-1;
    float input_tx,input_ty;
    if(mfin_tx == 1) {
      input_tx = dist_tx;
    }
    else if(fabs(dist_tx) < speed_tx) {
      mfin_tx = 1;
    }
    else {
      input_tx = speed_tx*sign_tx;      
    }

    if(mfin_ty == 1) {
      input_ty = dist_ty;
    }
    else if(fabs(dist_ty) < speed_ty) {
      mfin_ty = 1;
    }
    else {
      input_ty = speed_ty*sign_ty;      
    }

    pxset_visioncontrol_xy(origin_tx+pos_tx+input_tx,origin_ty+pos_ty+input_ty);
  }
  else if(time_start == 1) {
    time++;
  }


  if(pxget_whisle_detect() == 1) {
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN);
      printf("CPU0:whisle sound detected. Going down.\n");
    }      
    else if((pxget_operate_mode() == PX_HALT) && (once == 0)) {
      pxset_rangecontrol_z(120);
      pxset_operate_mode(PX_UP);
      once = 1; 
    }      
  }

  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }

  
  return;
}

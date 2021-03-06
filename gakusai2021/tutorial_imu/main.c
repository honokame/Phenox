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


const px_cameraid cameraid = PX_BOTTOM_CAM;

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
    int ax,ay,az,gx,gy,gz;
    pxget_imu(&ax,&ay,&az,&gx,&gy,&gz);
    printf("%d %d %d %d %d %d\n",ax,ay,az,gx,gy,gz);
    //printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
  } 

  static int prev_operatemode = PX_HALT;
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    pxset_visioncontrol_xy(st.vision_tx,st.vision_ty);
  }
  prev_operatemode = pxget_operate_mode();  

  if(pxget_whisle_detect() == 1) {
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN);
      printf("CPU0:whisle sound detected. Going down.\n");
    }      
    else if(pxget_operate_mode() == PX_HALT) {
      pxset_rangecontrol_z(120);
      pxset_operate_mode(PX_UP);		   
    }      
  }

  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }
  
  return;
}

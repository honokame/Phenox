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

const px_cameraid cameraid = PX_BOTTOM_CAM;

//configurable parameter by user
static int min_y = 0;
static int max_y = 255;
static int min_u = -127;
static int max_u = -3;
static int min_v = -127;
static int max_v = -3;
static float bias_x = 0;//from -100 to 100
static float bias_y = 0;//from -100 to 100 

int main(int argc, char **argv)
{
  pxinit_chain(); // parameter init
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

while(1);
}

static void setup_timer() {
  struct sigaction action; // signal check
  struct itimerval timer;
  
  memset(&action, 0, sizeof(action)); // actionの先頭アドレスに0を書き込む。
    
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

  pxset_keepalive(); // use aplication by linux
  pxset_systemlog(); // write log
  
  px_selfstate st;
  pxget_selfstate(&st); // get selfstate
  
  static unsigned long msec_cnt = 0;
  printf(%lu,msec_cnt);
  msec_cnt++; // 1
  printf(%lu,msec_cnt);
  if(!(msec_cnt % 3)){
    float blobx,bloby,blobsize;
    int ret = pxget_blobmark(&blobx,&bloby,&blobsize);
    if(ret == 1) {    
      pxset_blobmark_query(1,min_y,max_y,min_u,max_u,min_v,max_v);
      if(blobsize > 12) {
	printf("CPU0: green mark is found at (%.0f %.0f)\n",blobx,bloby); 
	pxset_visualselfposition(-blobx,-bloby);
      }
    }
  } 

  static int prev_operatemode = PX_HALT;
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {    
    pxset_visioncontrol_xy(bias_x,bias_y);//Note that target point is set (0,0), not (st.vision_tx,st.vision_ty), because estimated position(x,y) is reinitialized by "pxset_visualselfposition()", which is called when the colored mark is found. 
  }
  prev_operatemode = pxget_operate_mode();  

  if(pxget_whisle_detect() == 1) {
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN);
      printf("CPU0:whisle sound detected. Going down.\n");
    }      
    else if(pxget_operate_mode() == PX_HALT) {
      pxset_visualselfposition(0,0);//This tutorial expects a colored mark is put on the ground near Phenox. And this statement is needed to preventa large displacement of the target position from the current position when the Phenox enters PX_HOVER state but no colored mark is not found.
      
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

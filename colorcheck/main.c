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
static int g_min_y = 0;
static int g_max_y = 255;
static int g_min_u = -127;
static int g_max_u = -3;
static int g_min_v = -127;
static int g_max_v = -3;

static int b_min_y = 0;
static int b_max_y = 255;
static int b_min_u = 30;
static int b_max_u = 127;
static int b_min_v = -127;
static int b_max_v = -10;

static int r_min_y = 0;
static int r_max_y = 255;
static int r_min_u = -127;
static int r_max_u = -3;
static int r_min_v = 28;
static int r_max_v = 127;

int main(int argc, char **argv)
{
  pxinit_chain();
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

while(getchar() != 113);
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
  if(!(msec_cnt % 10)){
    float blobx_g,bloby_g,blobsize_g;
    int ret_g = pxget_blobmark(&blobx_g,&bloby_g,&blobsize_g);
    float blobx_b,bloby_b,blobsize_b;
    int ret_b = pxget_blobmark(&blobx_b,&bloby_b,&blobsize_b);
    float blobx_r,bloby_r,blobsize_r;
    int ret_r = pxget_blobmark(&blobx_r,&bloby_r,&blobsize_r);
  
/*
    if(ret_g == 1) {    
      pxset_blobmark_query(1,g_min_y,g_max_y,g_min_u,g_max_u,g_min_v,g_max_v);
      if(blobsize_g > 12) {
	printf("CPU0: green mark is found at (%.0f %.0f)\n",blobx_g,bloby_g); 
      }
     else
	return; 
    }*/
    if(ret_b == 1){
      pxset_blobmark_query(1,b_min_y,b_max_y,b_min_u,b_max_u,b_min_v,b_max_v);
      if(blobsize_b > 12) {
	printf("CPU0: blue mark is found at (%.0f %.0f)\n",blobx_b,bloby_b); 
      }
    else
      return;
    }/*
    if(ret_r == 1){
      pxset_blobmark_query(1,r_min_y,r_max_y,r_min_u,r_max_u,r_min_v,r_max_v);
      if(blobsize_r > 12) {
	printf("CPU0: red mark is found at (%.0f %.0f)\n",blobx_r,bloby_r); 
      }
    else
      return;
    }*/
  } 

  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }
  return;
}

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
const int ftmax = 200;
const int imgcount_max = 10;
const float recordtime = 5.0;

int main(int argc, char **argv)
{
  int i;
  //int end;
  pxinit_chain();
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

  //---------------------------------------------//
  printf("CPU0:Image processing Example.\n");
    
  IplImage *srcImage,*copyImage;
  copyImage = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);
  px_imgfeature *ft =(px_imgfeature *)calloc(ftmax,sizeof(px_imgfeature));
  int ftstate = 0;
  int ftnum = 0;

  int imgcount = 0;
  do { 
    if(ftstate == 0) {
      if(pxset_imgfeature_query(cameraid) == 1)
	ftstate = 1;
    }
    else if(ftstate == 1) {
     int res = pxget_imgfeature(ft,ftmax);
      if(res >= 0) {
	ftnum = res;
	ftstate = 2;
      }
    }

    if(pxget_imgfullwcheck(cameraid,&srcImage) == 1) {	    
      cvCopyImage(srcImage,copyImage);
      if(ftstate == 2) {
	ftstate = 0;
	for(i = 0;i < ftnum;i++) {
	  cvCircle(copyImage,cvPoint((int)ft[i].pcx,(int)ft[i].pcy),2,CV_RGB(255,255,0),1,8,0);
	  cvCircle(copyImage,cvPoint((int)ft[i].cx,(int)ft[i].cy),2,CV_RGB(0,255,0),1,8,0);
	  cvLine(copyImage,cvPoint((int)ft[i].pcx,(int)ft[i].pcy),cvPoint((int)ft[i].cx,(int)ft[i].cy),CV_RGB(0,0,255),1,8,0);
	}
	char name[100]={0};       
	sprintf(name,"../image/red/image_%d.png",imgcount++);	  
	cvSaveImage(name,copyImage,0);
	printf("CPU0:saved image %d/%d\n",imgcount,imgcount_max);      
      }
    }
  } while(imgcount < imgcount_max);
  
  //---------------------------------------------//
  printf("CPU0:Sound processing Example.\n");
  
  int buffersize = (int)(recordtime * 10000);//10kHz sampling rate
  short *soundbuffer = (short *)malloc(buffersize*sizeof(short));
  if(pxget_sound_recordstate() == 0) {
    pxset_sound_recordquery(recordtime);
  }  
  printf("CPU0:sound record for %.1f seconds\n",recordtime);
  while(pxget_sound_recordstate() != 1) {
    sleep(1);
  }
  pxget_sound(soundbuffer,recordtime);

  FILE *fp = fopen("./sound.raw","w+");
  fwrite(soundbuffer,sizeof(short),buffersize,fp);
  fclose(fp);

  printf("CPU0:waiting for whisle sound\n");
  //pxset_whisle_detect_reset();  
  //while(pxget_whisle_detect() == 0);
  //end = getchar();
  //printf("%d\n",end); 
  while(getchar() == 113);
  printf("Exit Tutorial\n");  
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
  
  /* set interval timer (10ms) */
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

  pxset_img_seq(cameraid);
  
  static unsigned long msec_cnt = 0;
  msec_cnt++;
  if(!(msec_cnt % 100)){
    //printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height);
  } 

  if(pxget_battery() == 1) {
    timer_disable = 1;    
    system("shutdown -h now\n");   
    exit(1);
  }
  
  return;
}

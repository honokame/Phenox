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
#include <string.h>

static char timer_disable = 0;

static void timerhandler(int i);
static void setup_timer();
void colormark(int *min_y, int *max_y, int *min_u, int *max_u, int *min_v, int *max_v);

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

static int fla1;/*ループ回数カウント*/
char iro[10];
int fla2;
int fla3;

int main(int argc, char **argv)
{
  fla1 = 0;
  pxinit_chain();
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

while(1);
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
    float blobx,bloby,blobsize;

	colormark(&min_y, &max_y, &min_u, &max_u, &min_v, &max_v);/***************************************/


    int ret = pxget_blobmark(&blobx,&bloby,&blobsize);
    if(ret == 1) {    
      pxset_blobmark_query(1,min_y,max_y,min_u,max_u,min_v,max_v);
      if(blobsize > 12) {
	printf("CPU0: mark is found at (%.0f %.0f)\n",blobx,bloby);
	printf("%d %d %d %d %d %d\t",min_y,max_y,min_u,max_u,min_v,max_v);
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
      
      pxset_rangecontrol_z(80);//120
      //pxset_operate_mode(PX_UP);		   
    }      
  }

  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }
  
  return;
}




void colormark(int *min_y,int *max_y,int *min_u,int *max_u,int *min_v,int *max_v)
{

/****追従する色の変更*******************************************************/
	float R1,G1,B1,R2,G2,B2;
	/*++fla1;/*グローバル変数*/

	       //緑:0
	       //赤:1
	       //青:2
	       //橙:5
	       //紫:6
	       //水:7
	//fla2=2;//blue

	fla3 = fla1/500;//500ごとに　緑→赤→青
	//fla2 = fla3%3;
	
	
	fla2=fla3%3;

	//green
	if(fla2==0){
		printf("green\n");
		*max_y = 255;
		*min_y = 0;
		*max_u = -3;//-3
		*min_u = -127;
		*max_v = -3;//-3
		*min_v = -127;
	}

	//red
	if(fla2==1){
		printf("red\n");
		*max_y =115;//121
		*min_y =75;//71
		*max_u =10;//12
		*min_u =-30;//-32
		*max_v =70;//74
		*min_v =15;//12
	}

	//blue
	if(fla2==2){
		printf("blue\n");
		*max_y =100;//69;//76
		*min_y = 50;//55;//40
		*max_u = 64;//57;//40
		*min_u = 20;//-100//51;//41
		*max_v =0;//74;//65
		*min_v =-100;//70;//73
	}

	/**********************************************************/
	if(fla2==5){//橙
		printf("orange\n");
		*max_y = 255;
		*min_y = 0;
		*max_u = -3;
		*min_u = -127;
		*max_v = 3;
		*min_v = 127;
	}

	if(fla2==6){//紫
		printf("purple\n");
		*max_y = 255;
		*min_y = 0;
		*max_u = 3;
		*min_u = 127;
		*max_v = 3;
		*min_v = 127;
	}

	if(fla2==7){//水色
		printf("light blue\n");
		*max_y = 255;
		*min_y = 0;
		*max_u = 3;
		*min_u = 127;
		*max_v = -3;
		*min_v = -127;
	}
	/**********************************************************/


	fla1++;

}


	// RGB to YUV 変換
	//	max_y =    0.299*R1 +    0.587*G1 +    0.114*B1;
	//	max_u = -0.14713*R2-   0.28886*G2 +     0.436*B1;
	//	max_v =    0.615*R1 -  0.51499*G2 -   0.10001*B2;
	//	min_y =    0.299*R2 +    0.587*G2 +    0.114*B2;
	//	min_u = -0.14713*R1 -  0.28886*G1 +    0.436*B2;
	//	min_v =    0.615*R2 -  0.51499*G1 -  0.10001*B1;

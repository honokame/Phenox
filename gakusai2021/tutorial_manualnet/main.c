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
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include "pxlib.h"
#include "parameter.h"

int timer_disable = 0;
static void timerhandler(int i);
static void setup_timer();
static void controller_process(char str);

int sock_s,sock;
struct sockaddr_in addr;
struct sockaddr_in client;
int len;

int js_triang;
int js_rect;
int js_cross;
int js_round;
int js_left_analog_x; 
int js_left_analog_y; 
int js_right_analog_y; 

int main(int argc, char **argv)
{
int i;
  pxinit_chain();
  set_parameter();
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready());
  printf("CPU0:Finished Initialization.\n");

  //minimum tcp server connects with a single client.
  sock_s = socket(AF_INET, SOCK_STREAM, 0);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(12000);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind(sock_s, (struct sockaddr *)&addr, sizeof(addr));

  listen(sock_s, 1);
  len = sizeof(client);
  sock = accept(sock_s, (struct sockaddr *)&client, &len); 

  setup_timer();
  while(1) {
    char buf[32];
    int n = read(sock, buf, sizeof(buf));  
    for(i = 0;i < n;i++) {
      controller_process(buf[i]);
    } 
    usleep(10000);
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
    //printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
  } 

  static int prev_operatemode = PX_HALT;
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    pxset_visioncontrol_xy(st.vision_tx,st.vision_ty);
  }
  prev_operatemode = pxget_operate_mode();  

  
  const float gain_angle = 8.0;
  if(js_triang == 1) {
    if(pxget_operate_mode() == PX_HALT) {
      pxset_rangecontrol_z(120);
      pxset_operate_mode(PX_UP);		   
    }      
  }
  else if(js_cross == 1) {
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN);
    }      
  }
  else if((js_left_analog_x != 127) | (js_left_analog_y != 127)) {
    float dst_angx = gain_angle * -(float)(js_left_analog_y - 127)/127.0;
    float dst_angy = gain_angle *  (float)(js_left_analog_x - 127)/127.0;
    pxset_dst_degx(dst_angx);
    pxset_dst_degy(dst_angy);
  }

  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }
  
  return;
}


static void controller_process(char str) {
  int ret = 0;
  int i,j;
  static int state = 0; 
  static char tcommand[20];  
  static char chk1 = 0;
  static char chk2 = 0;
  static char rcvchk1 = 0;
  static char rcvchk2 = 0;
  const int numcommand = 15;  
  
  if(state == 0) {     
    if(str == 'g') {
      chk1 = 0;	
      chk2 = 0;	
      state = 1;
    }
  }
  else if(state <= numcommand) {
    tcommand[state-1] = str;	
    chk1 ^= tcommand[state-1];
    chk2 += tcommand[state-1];
    state++;
  }       
  else if(state == (numcommand+1)) {
    rcvchk1 = str;
    state++;
  }
  else {
    state = 0;
    rcvchk2 = str;
    if(((chk1|0x80) == rcvchk1) && ((chk2|0x80) == rcvchk2)){	      
       
      for(i = 0;i < numcommand;i++) {	
	tcommand[i] = tcommand[i] & (~0x80);
      }      
      js_triang = tcommand[0];
      js_round = tcommand[1];
      js_cross = tcommand[2];
      js_rect = tcommand[3];
      js_left_analog_x =  (tcommand[4] << 4) & 0xf0; 
      js_left_analog_x |= tcommand[5] & 0x0f;
      js_left_analog_y =  (tcommand[6] << 4) & 0xf0; 
      js_left_analog_y |= tcommand[7] & 0x0f;
      js_right_analog_y =  (tcommand[10] << 4) & 0xf0; 
      js_right_analog_y |= tcommand[11] & 0x0f;          
      printf("%d %d %d %d %d %d %d\n",js_triang,js_round,js_cross,js_rect,js_left_analog_x,js_left_analog_y,js_right_analog_y);
    } 
  } 
}

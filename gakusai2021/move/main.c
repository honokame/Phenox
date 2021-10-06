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
  pxinit_chain(); // パラメータの初期化 
  set_parameter(); // パラメータの設定、parameter.c
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready()); // cpu1の準備ができたら1を返す
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

 // 画像特徴点を取得
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

// タイマー割り込み
void timerhandler(int i) {
  char c;  

  if(timer_disable == 1) {
    return;
  }

  pxset_keepalive(); // cpuがアプリケーション実行中であるとcpu1に伝える
  pxset_systemlog(); // systemlog.txtにログを書き込む
  
  px_selfstate st; // px_selfstateは機体の状態の一覧
  pxget_selfstate(&st); // 機体の状態の一覧を取得
  
  static unsigned long msec_cnt = 0;
  msec_cnt++;
  if(!(msec_cnt % 3)){
    // 機体の傾き、機体軸に対する移動量、高度、特徴点の個数を出力
    // 消しても良い
    printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
  } 

  static int prev_operatemode = PX_HALT; //停止状態
  
  // 前：上昇でホバー状態の時
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    pxset_visioncontrol_xy(st.vision_tx,st.vision_ty); // 初期位置を追従、元の位置で飛行
  }
  prev_operatemode = pxget_operate_mode();  

  // 笛の音を検出したら（＝１）
  //if(pxget_whisle_detect() == 1) {
  if(getchar() == 113){ // キーボードのq,a=97
    // ホバー状態の時
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN); // 下降状態に設定→停止状態に遷移
      printf("CPU0:whisle sound detected. Going down.\n");
    }      
    // 停止状態のとき
    else if(pxget_operate_mode() == PX_HALT) {
      pxset_rangecontrol_z(120); // 高さ120まで上昇
      pxset_operate_mode(PX_UP); // 上昇状態に設定→ホバー状態に遷移		   
    }      
  }

 // バッテリーの残量が少ないときに1を返す　
  if(pxget_battery() == 1) {
    timer_disable = 1;
    system("shutdown -h now\n");   
    exit(1);
  }

  printf("%d\n",prev_operatemode);
  
  return;
}

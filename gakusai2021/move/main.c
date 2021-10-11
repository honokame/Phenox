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

// static：関数が終了しても値が保存される
static char timer_disable = 0;
static void timerhandler(int i);
static void setup_timer();
static int ftnum = 0; // 特徴点の個数
const int ftmax = 200;

const px_cameraid cameraid = PX_BOTTOM_CAM;

int main(int argc, char **argv) {
  pxinit_chain(); // パラメータの初期化 
  set_parameter(); // パラメータの設定、parameter.c
   
  printf("CPU0:Start Initialization. Please do not move Phenox.\n");
  while(!pxget_cpu1ready()); // cpu1の準備ができたら1を返す
  setup_timer();
  printf("CPU0:Finished Initialization.\n");

 // 画像特徴点を取得
  px_imgfeature *ft =(px_imgfeature *)calloc(ftmax,sizeof(px_imgfeature));
  int ftstate = 0;
  
  // 特徴点取得の要求→100ms待機→特徴点の情報取得→100ms待機を繰り返す
  while(1) {
    if(ftstate == 0) {
      if(pxset_imgfeature_query(cameraid) == 1) // 画像特徴点取得の要求、成功すれば１を返す
	ftstate = 1;
    }
    else if(ftstate == 1) { // 画像特徴点取得の要求が成功したとき
      int res = pxget_imgfeature(ft,ftmax); // ftに特徴点の座標を書き込み、特徴点の個数を返す
      if(res >= 0) { 
	      ftnum = res; 
	      ftstate = 0;      
      }
    }
    usleep(1000); // 100ms
  }
}

static void setup_timer() {
  struct sigaction action;
  struct itimerval timer;
  
  // SIGINT受信時にtimerhandlerを実行
  memset(&action, 0, sizeof(action)); // actionに0を代入  
  action.sa_handler = timerhandler; // シグナルハンドラ、シグナル発生時に実行したい関数へのポインタ
  action.sa_flags = SA_RESTART; // シグナルにより中断されたシステムコールの処理は再開する
  sigemptyset(&action.sa_mask); // 禁止するシグナルのマスクはなし

  // シグナルを設定
  // (シグナル,sigactionのアドレス,古いアクション)、失敗すれば-1を返す
  if(sigaction(SIGALRM, &action, NULL) < 0){
    perror("sigaction error");
    exit(1);
  }
  
  // 周期的にシグナルを発生させる (10ms) 
  // it_vslue：1回目の時間、it_interval：それ以降のインターバル時間
  // 1ms = 0.001s
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 10000;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 10000;

  // タイマを設定
  // ITIMER_REAL：実時間で減少し、満了するとSIGALUMが生成され送られる
  if(setitimer(ITIMER_REAL, &timer, NULL) < 0){
    perror("setitimer error");
    exit(1);
  }
}

// シグナルハンドラ（タイマ割り込み）
void timerhandler(int i) {
  char c;  
  
  // タイマ割り込み終了、timerhandler関数を抜ける
  if(timer_disable == 1) {
    return;
  }

  pxset_keepalive(); // cpu0がアプリケーション実行中であるとcpu1に伝える
  pxset_systemlog(); // systemlog.txtにログを書き込む
  
  px_selfstate st; // px_selfstateは機体の状態の一覧
  pxget_selfstate(&st); // 機体の状態の一覧を取得
  
  static int prev_operatemode = PX_HALT; // 停止状態 
  static unsigned long msec_cnt = 0;
  msec_cnt++;
  
  if(!(msec_cnt % 3)){
    // 機体の傾き、現在の自己位置、高度、特徴点の個数を出力
    //printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
    printf("%d\n",prev_operatemode);
  } 

  //static int prev_operatemode = PX_HALT; //停止状態
  
  // 上昇→ホバー状態の時
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    pxset_visioncontrol_xy(st.vision_tx,st.vision_ty); // 自己位置を追従、元の位置で飛行
  }
  prev_operatemode = pxget_operate_mode();  
  
  // 笛の音を検出したら（＝１）
  if(pxget_whisle_detect() == 1) {
    // ホバー状態の時
    if(pxget_operate_mode() == PX_HOVER) {
      pxset_operate_mode(PX_DOWN); // 下降状態に設定→3秒後停止状態に遷移
      printf("CPU0:whisle sound detected. Going down.\n");
    }      
    // 停止状態のとき、初めの状態
    else if(pxget_operate_mode() == PX_HALT) {
      pxset_rangecontrol_z(120); // 高さ120を目標に設定
      pxset_operate_mode(PX_UP); // 上昇状態→1.2秒後ホバー状態に遷移		   
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

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
void colorchange(int *min_y,int *max_y,int *min_u,int *max_u,int 
*min_v,int *max_v);
static int ftnum = 0; // 特徴点の個数
const int ftmax = 200;
static int count = 0;
static int flag = 0;
static int flag_red = 0;
static int flag_blue = 0;
static int flag_green = 0;
const px_cameraid cameraid = PX_BOTTOM_CAM;

// 色のパラメータ
//red 
static int min_y = 75;
static int max_y = 110;
static int min_u = -40;
static int max_u = 10;
static int min_v = 15;
static int max_v = 70;
/*
//blue
static int min_y = 50;
static int max_y = 100;
static int min_u = 0;
static int max_u = 64;
static int min_v = -100;
static int max_v = 0;

//green
static int min_y = 0;
static int max_y = 255;
static int min_u = -127;
static int max_u = 0;
static int min_v = -127;
static int max_v = 0;
*/
static float bias_x = 0;//from -100 to 100
static float bias_y = 0;//from -100 to 100 

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
  static float x[10];
  static float y[10];
  static int j = 0;
  static int k = 0;
  static float red_x = 0;
  static float red_y = 0;
  static float blue_x = 0;
  static float blue_y = 0;
  static float green_x = 0;
  static float green_y = 0;
  msec_cnt++;
  
  // 3msに1回
  if(!(msec_cnt % 3)){
    // 機体の傾き、現在の自己位置、高度、特徴点の個数を出力
    //printf("%.2f %.2f %.2f | %.2f %.2f %.2f | %.2f | %d\n",st.degx,st.degy,st.degz,st.vision_tx,st.vision_ty,st.vision_tz,st.height,ftnum);
    float blobx,bloby,blobsize;
    int ret = pxget_blobmark(&blobx,&bloby,&blobsize); // 重心とピクセル数が書き込まれる
    if(ret == 1) {    
      pxset_blobmark_query(1,min_y,max_y,min_u,max_u,min_v,max_v); // 指定した色のピクセル数と重心を求める要求
      if(blobsize > 12) {
        printf("mark is found at (%.0f %.0f)\n",blobx,bloby); 
        pxset_visualselfposition(-blobx,-bloby);
        x[flag] = blobx;
        y[flag] = bloby; 
        flag++;
      }
    }  
  count++;    
  
  // 10回色を検知するごとに切り替える、赤→青→緑
  if(count == 10){
    if(flag > 6){
      printf("red detected\n");
      red_x = x[flag - 1];
      red_y = y[flag - 1];
      for(k = 0;k < 10;k++){
        //printf("x[%d]=%f,y[%d]=%f\n",k,x[k],k,y[k]);
        //red_x = x[k];
        //red_y = y[k];
        x[k] = 0;
        y[k] = 0; 
      }
      printf("red_x=%f,red_y=%f\n",red_x,red_y);    
      //printf("red_x_ave=%f,red_y_ave=%f\n",red_x/(flag-1),red_y/(flag -1));    
      flag_red = 1;
    }
    colorchange(&min_y,&max_y,&min_u,&max_u,&min_v,&max_v);
    flag = 0;
  }else if(count == 20) {
    if(flag > 6){
      printf("blue detected\n");
      blue_x = x[flag - 1];
      blue_y = y[flag - 1];
      for(k = 0;k < 10;k++) {
        x[k] = 0;
        y[k] = 0;
      }
      printf("blue_x=%f,blue_y=%f\n",blue_x,blue_y);    
      flag_blue = 1;
    } 
    colorchange(&min_y,&max_y,&min_u,&max_u,&min_v,&max_v);
    flag = 0;
  }else if(count == 30) {
    if(flag > 6){
      printf("green detected\n");
      green_x = x[flag - 1];
      green_y = y[flag - 1];
      for(k = 0;k < 10;k++){
        x[k] = 0;
        y[k] = 0;
      }
      printf("green_x=%f,green_y=%f\n",green_x,green_y);
      flag_green = 1;
    }
    colorchange(&min_y,&max_y,&min_u,&max_u,&min_v,&max_v);
    flag = 0;
    count = 0; // カウンターをリセット
  }
}
  //static int prev_operatemode = PX_HALT; //停止状態
  
  // 上昇→ホバー状態の時
  if((prev_operatemode == PX_UP) && (pxget_operate_mode() == PX_HOVER)) {
    pxset_visioncontrol_xy(st.vision_tx,st.vision_ty); // 自己位置を追従、元の位置で飛行
    //pxset_visioncontrol_xy(bias_x,bias_y); // マーカーの位置まで飛行
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
      //pxset_visualselfposition(0,0); // 自己位置を原点に設定
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
  return;
}

// YUV値の切り替え
void colorchange(int *min_y,int *max_y,int *min_u,int *max_u,int *min_v,int *max_v) {
  switch(count / 10) {
    case 3: // red
      printf("red\n");
      *min_y = 75;
      *max_y = 110;
      *min_u = -40;
      *max_u = 10;
      *min_v = 15;
      *max_v = 70;
      break;

    case 1: // blue
      printf("blue\n");
      *min_y = 50;
      *max_y = 100;
      *min_u = 0;
      *max_u = 74;
      *min_v = -100;
      *max_v = 0;
      break;

    case 2: // green
      printf("green\n");
      *min_y = 0;
      *max_y = 255;
      *min_u = -127;
      *max_u = 0;
      *min_v = -127;
      *max_v = 0;
      break;

    default:
      break;
  }
}

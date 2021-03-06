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
#include <byteswap.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include "pxlib.h"


#define PAGE_SIZE ((size_t)getpagesize()) 
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))

static void rm_setpixel_bga(IplImage *frameimage,int x,int y,int color,unsigned char pixel) {
  *((unsigned char *)(frameimage->imageData + x * sizeof(unsigned char) * 3 + y * frameimage->widthStep) + color) = pixel;
}

static void rm_setpixel_mono(IplImage *frameimage,int x,int y,unsigned char pixel) {
  *((unsigned char *)(frameimage->imageData + x * sizeof(unsigned char) + y * frameimage->widthStep)) = pixel;
}

static volatile uint8_t *mm_image_r,*mm_image_l,*mm_flag_r,*mm_flag_l;    
static volatile uint8_t *mm_operate;    
static volatile uint8_t *mm_sound;
static volatile uint8_t *mm_log,*mm_log_notify;
static volatile uint8_t *mm_boot;
static volatile uint8_t *mm_feature;
static volatile uint8_t *mm_blobmark;
static volatile uint8_t *mm_img_t;

static int raw_hlength = 320;
static int raw_vlength = 240;
static int full_hlength = 640;
static int full_vlength = 480;
struct termios save_settings;
struct termios settings;

//dualimage
static IplImage *imgdual_l[2];
static IplImage *imgdual_r[2];

//image
volatile uint8_t *addr_img_l;
volatile uint8_t *addr_img_r;
volatile uint32_t *addr_imgq_l; 
volatile uint32_t *addr_imgq_r;

static FILE *fp_systemlog;

volatile static px_operate *addr_operate;
volatile static px_pconfig *addr_pconfig;
volatile static px_selfstate *addr_selfstate;
volatile static px_private *addr_private;

//sound
volatile short *addr_sound;
volatile int *addr_soundnotify;
volatile int *addr_whislenotify;
volatile float *addr_soundlength;

//feature
volatile px_imgfeature *addr_feature;
volatile int *addr_feature_num;
volatile int *addr_feature_state;

//blobmark
volatile px_blobmark *addr_blobmark;
volatile px_blobmark_config *addr_blobmark_config;

//thermo
volatile unsigned char *addr_img_t;

void pxinit_chain() {
  int fd;
  
  fd = open("/dev/mem", O_RDWR|O_SYNC);
  if (fd < 0) {
    fprintf(stderr, "open(/dev/mem) failed (%d)\n", errno);
    return;
  }
  
  //image
  uint64_t base_image_l = 0x0ff00000;//0x0ff00000;    
  uint64_t base_image_r = 0x0fe00000;
  uint64_t base_flag_l =  0xfffdf000;
  uint64_t base_flag_r =  0xfffcf000;
  
  mm_image_r = mmap(NULL, PAGE_SIZE*144, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_image_r);
  if (mm_image_r == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_image_r, errno);
    exit(-1);
  }
  
  mm_image_l = mmap(NULL, PAGE_SIZE*144, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_image_l);
  if (mm_image_l == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_image_l, errno);
    exit(-1);
  }
    
  mm_flag_r = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_flag_r);
  if (mm_flag_r == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_flag_r, errno);
    exit(-1);
  }
  
  mm_flag_l = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_flag_l);
  if (mm_flag_l == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_flag_l, errno);
    exit(-1);
  }

  addr_img_l   = (volatile uint8_t *)(mm_image_l);
  addr_img_r   = (volatile uint8_t *)(mm_image_r);
  addr_imgq_l = (volatile uint32_t *)(mm_flag_l+0xffc);
  addr_imgq_r = (volatile uint32_t *)(mm_flag_r+0xffc);

  //operate
  uint64_t base_operate = 0xffff0000;  
  mm_operate = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_operate);
  if (mm_operate == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_operate, errno);
    exit(-1);
  }

  addr_operate = (volatile px_operate *)(mm_operate+0x000);
  addr_private = (volatile px_private *)(mm_operate+0x400);
  addr_pconfig = (volatile px_pconfig *)(mm_operate+0x600);
  addr_selfstate = (volatile px_selfstate *)(mm_operate+0x800);
  
  //sound
  uint64_t base_sound =  0x0fd00000;
  mm_sound = mmap(NULL, PAGE_SIZE*256, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_sound);
  if (mm_sound == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_sound, errno);
    exit(-1);
  }
  
  addr_sound = (volatile short *)(mm_sound);
  addr_soundnotify =  (volatile int *)(mm_sound+0xffff4);
  addr_whislenotify = (volatile int *)(mm_sound+0xffff8);
  addr_soundlength = (volatile float *)(mm_sound+0xffffc);
  
  //general log  
  uint64_t base_log =   0x0ff90000;
  uint64_t base_notify = 0xfffef000;       
  mm_log = mmap(NULL, PAGE_SIZE*15, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_log);
  if (mm_log == MAP_FAILED) {
    fprintf(stderr, "mmap64(0x%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_log, errno);
    exit(-1);
  }
 
  mm_log_notify = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_notify);
  if (mm_log_notify == MAP_FAILED) {
    fprintf(stderr, "mmap64(0x%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_notify, errno);
    exit(-1);
  }

  //feature
  uint64_t base_feature = 0x0fe90000;
  mm_feature = mmap(NULL, PAGE_SIZE*16, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_feature);
  if (mm_feature == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_image_l, errno);
    exit(-1);
  }
  addr_feature = (volatile px_imgfeature *)(mm_feature);
  addr_feature_num = (volatile int *)(mm_feature+0xfff8);
  addr_feature_state = (volatile int *)(mm_feature+0xfffc);

  //blobmark
  uint64_t base_blobmark = 0x0fea0000;
  mm_blobmark = mmap(NULL, PAGE_SIZE*1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_blobmark);
  if (mm_blobmark == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_blobmark, errno);
    exit(-1);
  }
  addr_blobmark = (volatile px_blobmark *)(mm_blobmark);
  addr_blobmark_config = (volatile px_blobmark_config *)(mm_blobmark+0xfa0);

  //thermo
  uint64_t base_img_t = 0x0fea1000;
  mm_img_t = mmap(NULL, PAGE_SIZE*3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_img_t);
  if (mm_img_t == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_img_t, errno);
    exit(-1);
  }
  addr_img_t = (volatile unsigned char *)(mm_img_t);

  if ((fp_systemlog = fopen("/root/systemlog.txt", "w+")) == NULL) {  
    printf("file open error -- /root/systemlog.txt\n");
    exit(1);
  }


  addr_pconfig->duty_hover = 1200;//1100;////1600;
  addr_pconfig->duty_hover_max = 1350;//1250;//1850;
  addr_pconfig->duty_hover_min = 1000;//900;////1400;
  addr_pconfig->duty_up = 1350;//1250;//1850;
  addr_pconfig->duty_down = 1000;//900;//1430;
  addr_pconfig->duty_bias_front = 0;
  addr_pconfig->duty_bias_back = 0;
  addr_pconfig->duty_bias_left = 130;
  addr_pconfig->duty_bias_right = -130;
  addr_pconfig->pgain_sonar_tz = 45.0/1000.0;//35.0/1000.0;//20.0/1000.0;
  addr_pconfig->dgain_sonar_tz = 20.0;//11.5;//4.0;
  addr_pconfig->pgain_vision_tx = 0.032;//0.045;//0.032;//0.03;want
  addr_pconfig->pgain_vision_ty = 0.032;//0.045;//0.032;//0.03;
  addr_pconfig->dgain_vision_tx = 0.80;//0.95;//0.15;
  addr_pconfig->dgain_vision_ty = 0.80;//0.95;//0.15;
  addr_pconfig->whisleborder = 280;
  addr_pconfig->soundborder = 1000;
  addr_pconfig->uptime_max = 0.8;//1.2;
  addr_pconfig->downtime_max = 3.0;
  addr_pconfig->dangz_rotspeed = 15.0;
  addr_pconfig->featurecontrast_front = 35;
  addr_pconfig->featurecontrast_bottom = 25;
  addr_pconfig->pgain_degx = 880;//300;//880;
  addr_pconfig->pgain_degy = 880;//300;//880;
  addr_pconfig->pgain_degz = 2400;
  addr_pconfig->dgain_degx = 22;//30;//22;
  addr_pconfig->dgain_degy = 22;//30;//22;
  addr_pconfig->dgain_degz = 28;
  addr_pconfig->pwm_or_servo = 0;  
  addr_pconfig->propeller_monitor = 1;
  //dualimage
  imgdual_l[0] = cvCreateImage(cvSize(raw_hlength,raw_vlength),IPL_DEPTH_8U,3);
  imgdual_r[0] = cvCreateImage(cvSize(raw_hlength,raw_vlength),IPL_DEPTH_8U,3);
  imgdual_l[1] = cvCreateImage(cvSize(raw_hlength,raw_vlength),IPL_DEPTH_8U,3);
  imgdual_r[1] = cvCreateImage(cvSize(raw_hlength,raw_vlength),IPL_DEPTH_8U,3);


  //boot   
  uint64_t base_boot =  0xfffff000;
  mm_boot = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_boot);
  if (mm_boot == MAP_FAILED) {
    fprintf(stderr, "mmap64(0lx%lx@0lx%lx) failed (%d)\n",
	    PAGE_SIZE, base_boot, errno);
    exit(-1);
  }
  *(volatile int *)(mm_boot+0xff0) = 1;  
}

void pxclose_chain() {
  fclose(fp_systemlog);
  munmap((void *)mm_image_r, PAGE_SIZE*38*4);
  munmap((void *)mm_image_l, PAGE_SIZE*38*4);
  munmap((void *)mm_flag_l, PAGE_SIZE);
  munmap((void *)mm_flag_r, PAGE_SIZE);
  munmap((void *)mm_operate, PAGE_SIZE);
  munmap((void *)mm_sound, PAGE_SIZE*14);
  munmap((void *)mm_log, PAGE_SIZE*15);
  munmap((void *)mm_log_notify, PAGE_SIZE);
  munmap((void *)mm_boot, PAGE_SIZE);
  munmap((void *)mm_feature, PAGE_SIZE*16);
  munmap((void *)mm_blobmark, PAGE_SIZE);
}

int pxget_cpu1ready() {
  return addr_private->ready;
}

int pxget_motorstatus() {
  return addr_private->startup;
}

void pxset_operate_mode(px_flymode mode) { 
  if(mode == PX_HALT) {
    addr_operate->mode = 0;
  }
  else if(mode == PX_UP) {
    addr_operate->mode = 1;
  }
  else if(mode == PX_HOVER) {
    addr_operate->mode = 2;
  }
  else if(mode == PX_DOWN) {
    addr_operate->mode = 3;
  }
}

px_flymode pxget_operate_mode() {
  if(addr_operate->mode == 0)
    return PX_HALT;
  else if(addr_operate->mode == 1)
    return PX_UP;
  else if(addr_operate->mode == 2)
    return PX_HOVER;
  else if(addr_operate->mode == 3)
    return PX_DOWN;
}

void pxget_pconfig(px_pconfig *param) {
  memcpy((px_pconfig *)param,(px_pconfig *)addr_pconfig,sizeof(px_pconfig));
}  

void pxset_pconfig(px_pconfig *param) {
  memcpy((px_pconfig *)addr_pconfig,(px_pconfig *)param,sizeof(px_pconfig));
}  

void pxset_keepalive() {
  (addr_private->keepalive)++;
} 

void pxset_visioncontrol_xy(float tx,float ty) {
  addr_private->selxy = 0;
  addr_operate->vision_dtx = tx;
  addr_operate->vision_dty = ty;
}

void pxset_rangecontrol_z(float tz) {
  addr_operate->sonar_dtz = tz;
}

void pxset_dst_degx(float val) {
  addr_private->selxy = 1;
  addr_operate->dangx = val;
}

void pxset_dst_degy(float val) {
  addr_private->selxy = 1;
  addr_operate->dangy = val;
}

void pxset_dst_degz(float val) {
  addr_operate->dangz = val;
}

int pxset_visualselfposition(float tx,float ty) {
  if(addr_operate->fix_selfposition_query == 0) {
    addr_operate->fix_selfposition_tx = tx;
    addr_operate->fix_selfposition_ty = ty;
    addr_operate->fix_selfposition_query = 1;
    return 1;
  }
  else {
    return 0;
  }
} 

//dualimage
static void incimgpos(int cam, int *val) {
  if(cam == 0) {
    if(*val == raw_vlength) {
      *val = 0;
    }
    else {
      (*val)++;
    }
  }
  else if(cam == 1) {
    if(*val == raw_vlength) {
      *val = 0;
    }
    else {
      (*val)++;
    }
  } 
}

static void setimgline(char id, IplImage *img,int div, int line) {  
  int j;
  volatile uint32_t value;
  if((id < 0) || (id > 1)) {
    return;
  }
  int hlength,vlength;
  if(id == 0) {
    hlength = raw_hlength;
    vlength = raw_vlength;
  }
  else if(id == 1) {
    hlength = raw_hlength;
    vlength = raw_vlength;
  }
 
  for(j=0;j < hlength;j+=2) {
    if(id == 0) {
      value = *(volatile uint32_t *)(addr_img_l + line*hlength*2 + j*2);
    }
    else {
      value = *(volatile uint32_t *)(addr_img_r + line*hlength*2 + j*2);
    }

    /*
    int u0 = (unsigned char)((value >> 0) & 0xff)-127;
    int y1 = (unsigned char)((value >> 8) & 0xff);
    int v0 = (unsigned char)((value >> 16) & 0xff)-127;
    int y0 = (unsigned char)((value >> 24) & 0xff);
    */
    
    int u0 = (unsigned char)((value >> 0) & 0xff)-127;
    int y0 = (unsigned char)((value >> 8) & 0xff);
    int v0 = (unsigned char)((value >> 16) & 0xff)-127;
    int y1 = (unsigned char)((value >> 24) & 0xff);
    
    int r0 = (1000*y0+1402*v0)/1000;
    int g0 = (1000*y0-344*u0-714*v0)/1000;
    int b0 = (1000*y0+1772*u0)/1000;
    int r1 = (1000*y1+1402*v0)/1000;
    int g1 = (1000*y1-344*u0-714*v0)/1000;
    int b1 = (1000*y1+1772*u0)/1000;
    if(r0 > 255) r0 = 255;
    else if(r0 < 0) r0 = 0;
    if(g0 > 255) g0 = 255;
    else if(g0 < 0) g0 = 0;
    if(b0 > 255) b0 = 255;
    else if(b0 < 0) b0 = 0;
    if(r1 > 255) r1 = 255;
    else if(r1 < 0) r1 = 0;
    if(g1 > 255) g1 = 255;
    else if(g1 < 0) g1 = 0;
    if(b1 > 255) b1 = 255;
    else if(b1 < 0) b1 = 0;
    
    if(div == 0) {
      rm_setpixel_bga(img,j,line,2,(unsigned char)r0);
      rm_setpixel_bga(img,j,line,1,(unsigned char)g0);
      rm_setpixel_bga(img,j,line,0,(unsigned char)b0);
      rm_setpixel_bga(img,j+1,line,2,(unsigned char)r1);
      rm_setpixel_bga(img,j+1,line,1,(unsigned char)g1);
      rm_setpixel_bga(img,j+1,line,0,(unsigned char)b1);		
    }
    else if(div == 1) {
      if(((j % 2) == 0) && ((line % 2) == 0)) { 
	rm_setpixel_bga(img,j/2,line/2,2,(unsigned char)r0);
	rm_setpixel_bga(img,j/2,line/2,1,(unsigned char)g0);
	rm_setpixel_bga(img,j/2,line/2,0,(unsigned char)b0);
      }
    }
    else if(div == 2) {
      if(((j % 4) == 0) && ((line % 4) == 0)) { 
	rm_setpixel_bga(img,j/4,line/4,2,(unsigned char)r0);
	rm_setpixel_bga(img,j/4,line/4,1,(unsigned char)g0);
	rm_setpixel_bga(img,j/4,line/4,0,(unsigned char)b0);
      }
    }    
  }  
}

static unsigned long s_imgcnt_l = 0;
static unsigned long s_imgcnt_r = 0;
static char imgsel_l = 0;
static char imgsel_r = 0;
  
void pxset_img_seq(px_cameraid cam) {
  int id;
  if(cam == PX_FRONT_CAM) id = 0;
  else if(cam == PX_BOTTOM_CAM) id = 1;
  else return;
 
  static int s_hcount_l = 0;
  static int s_hcount_r = 0;

  int c_hcount_l = *addr_imgq_l;
  int c_hcount_r = *addr_imgq_r;  

  int debug_r = 0;
  int debug_l = 0;
int hlength,vlength;
  if(id == 0) {
    hlength = raw_hlength;
    vlength = raw_vlength;
  }
  else if(id == 1) {
    hlength = raw_hlength;
    vlength = raw_vlength;
  }

  if(id == 0) {
    if((c_hcount_l < 0) || (c_hcount_l > hlength)) return;
    while(s_hcount_l != c_hcount_l) {    
      static int times = 0;
      if(s_hcount_l > 0) {
	setimgline(0,imgdual_l[imgsel_l],0,s_hcount_l-1);	
	times++;
      }   
      if(s_hcount_l == 0) {
	imgsel_l = (imgsel_l == 0)? 1:0;
	s_imgcnt_l++;
	//printf("img updated:%d %d\n",s_imgcnt_l,times);
	times = 0;
      }
      incimgpos(0,&s_hcount_l);     

      if(debug_l > 1000) {
	//printf("de:l %d %d\n",s_hcount_l,c_hcount_l);
      }	
      debug_l++;
    }
  }
  else {
    if((c_hcount_r < 0) || (c_hcount_r > hlength)) return;
    while(s_hcount_r != c_hcount_r) {
      if(s_hcount_r > 0) {
	setimgline(1,imgdual_r[imgsel_r],0,s_hcount_r-1);
      }
      if(s_hcount_r == 0) {
	imgsel_r = (imgsel_r == 0)? 1:0;
	s_imgcnt_r++;
      }
      incimgpos(1,&s_hcount_r);
      
      if(debug_r > 1000) {
	//printf("de:r %d %d\n",s_hcount_r,c_hcount_r);
      }		  
      debug_r++;
      
    }
  }
}

int pxget_imgfullwcheck(px_cameraid cam, IplImage **img) {
  int id;
  int ret = 0;
  static unsigned long c_imgcnt_l = 0;
  static unsigned long c_imgcnt_r = 0;

  if(cam == PX_FRONT_CAM) id = 0;
  else if(cam == PX_BOTTOM_CAM) id = 1;
  else return 0;
  
  static int curimgid_l = 0;
  static int curimgid_r = 0;
  if(id == 0) {    
    if(c_imgcnt_l != s_imgcnt_l) {
      c_imgcnt_l = s_imgcnt_l;
      if(imgsel_l == 0) {
	*img = imgdual_l[1];	
      }
      else { 
	*img = imgdual_l[0];
      }
      ret = 1;
    }
    else {
      ret = 0;
    }
  }
  else if(id == 1) {    
    if(c_imgcnt_r != s_imgcnt_r) {
      c_imgcnt_r = s_imgcnt_r;
      if(imgsel_r == 0) 
	*img = imgdual_r[1];
      else 
	*img = imgdual_r[0];
      ret = 1;
    }
    else {
      ret = 0;
    }
  }
  return ret;
}
//!dualimage
//feature

int pxset_imgfeature_query(px_cameraid cam) {
  if(*addr_feature_state != 0) return -1;
  else {
    if(cam == PX_FRONT_CAM) {
      *addr_feature_state = 1;
      return 1;
    }
    else if(cam == PX_BOTTOM_CAM) {
      *addr_feature_state = 2;
      return 1;
    }
    else return -1;
  }
}

int pxget_imgfeature(px_imgfeature *ft,int maxnum) {
  int i,j;
  if(*addr_feature_state != 0) return -1;
  if(*addr_feature_num == 0) return 0;
  
  int num;
  if(*addr_feature_num > maxnum) 
    num = maxnum;
  else 
    num = *addr_feature_num;
  memcpy(ft,(void *)addr_feature,num*sizeof(px_imgfeature));
  return num;  
}
//!feature


//blobmark
int pxset_blobmark_query(px_cameraid cam,float min_y,float max_y,float min_u,float max_u,float min_v,float max_v) {  
  if(addr_blobmark_config->state == 0) {
    addr_blobmark_config->min_y = min_y;
    addr_blobmark_config->min_u = min_u;
    addr_blobmark_config->min_v = min_v;
    addr_blobmark_config->max_y = max_y;
    addr_blobmark_config->max_u = max_u;
    addr_blobmark_config->max_v = max_v;   
    if(cam == PX_FRONT_CAM) {
      addr_blobmark_config->cam = 0;
    }
    else if(cam == PX_BOTTOM_CAM) {
      addr_blobmark_config->cam = 1;
    }
    addr_blobmark_config->num = 0;
    addr_blobmark_config->state = 1;
    return 1;
  }
  else
    return -1;
}

int pxget_blobmark(float *x,float *y, float *size) {
  if(addr_blobmark_config->state == 0) {
    *x = addr_blobmark[0].cx;
    *y = addr_blobmark[0].cy;
    *size = addr_blobmark[0].size;
    return 1;
  }
  else {
    *x = 0;
    *y = 0;
    *size = 0;
    return 0;
  }
}
//!blobmark


int pxget_whisle_detect() {    
  if(*addr_whislenotify == 1) {
    *addr_whislenotify = 0;
    return 1;
  }
  else {
    return 0;
  }
}

void pxset_whisle_detect_reset() {
  *addr_whislenotify = 0;
}

int pxget_sound_recordstate() {
  if(*addr_soundnotify == 2) {   
    return 1;
  }
  else if(*addr_soundnotify == 1){
    return -1;
  }
  else {
    return 0;
  }
}

int pxset_sound_recordquery(float recordtime) {
  if(*addr_soundnotify == 0) {    
    int size = (int)(recordtime * 10000);
    if(size > 0xffff0) {
      size = 0xffff0;
    }
    else if(size < 0) {
      size = 0;
    }
    *addr_soundlength = recordtime;
    *addr_soundnotify = 1;
    return 1;
  }
  else {
    return -1;
  }
}

int pxget_sound(short *buffer,float recordtime) {
  if(*addr_soundnotify == 2) {    
    int size = (int)(recordtime * 10000);
    if(size > 0xffff0) {
      size = 0xffff0;
    }
    else if(size < 0) {
      size = 0;
    }
    memcpy((short *)buffer,(short *)addr_sound,sizeof(short)*size);    
    *addr_soundnotify = 0;
    return 1;
  }
  else {
    return -1;
  }
}

void pxget_selfstate(px_selfstate *state) {
  memcpy((px_selfstate *)state,(px_selfstate *)addr_selfstate,sizeof(px_selfstate));
}

void pxset_led(int led,int state) {  
  if(led < 0) return;
  if(led > 1) return;
  if(state == 1) {
    addr_operate->led[led] = 1;
  }
  else if(state == 0) {
    addr_operate->led[led] = 0;
  }
}

void pxset_buzzer(int state) {  
  if(state < 0) return;
  if(state > 1) return;
  if(state == 1) {
    addr_operate->buzzer = 1;
  }
  else if(state == 0) {
    addr_operate->buzzer = 0;
  }
}

void pxset_systemlog() {
  volatile int update_loc = *(volatile uint32_t *)(mm_log_notify);
  volatile char *mm_logarr = (volatile char *)(mm_log);
  static int cur_loc = 0;
  while(cur_loc != update_loc) {
    if(cur_loc == 0xf000) {
      cur_loc = 0;
    }
    char a = mm_logarr[cur_loc];
    fprintf(fp_systemlog,"%c",a);
    //printf("%c",a);
    cur_loc++;
  }
}

int pxget_battery() {
  return addr_selfstate->battery;
} 

void pxget_imu(int *ax,int *ay,int *az,int *gx,int *gy,int *gz) {
  *ax = addr_private->ax;
  *ay = addr_private->ay;
  *az = addr_private->az;
  *gx = addr_private->gx;
  *gy = addr_private->gy;
  *gz = addr_private->gz;
}

#include "pxlib.h"
#include "parameter.h"

void set_parameter() {
  px_pconfig param;
  param.duty_hover = 1850;
  param.duty_hover_max = 2250;
  param.duty_hover_min = 1450;
  param.duty_up = 2050;
  param.duty_down = 1500;
  param.duty_bias_front = 0;
  param.duty_bias_back = 0;
  param.duty_bias_left = 0;
  param.duty_bias_right = 0;
  param.pgain_sonar_tz = 30.0/1000.0;//20.0/1000.0;
  param.dgain_sonar_tz = 60.0;//60.0;
  param.pgain_vision_tx = 0.034;
  param.pgain_vision_ty = 0.040;
  param.dgain_vision_tx = 0.10;
  param.dgain_vision_ty = 0.20;
  param.whisleborder = 180;
  param.soundborder = 1000;
  param.uptime_max = 1.2;
  param.downtime_max = 3.0;
  param.dangz_rotspeed = 15.0;  
  param.featurecontrast_front = 35;
  param.featurecontrast_bottom = 30;
  param.pgain_degx = 1580;//1800:
  param.pgain_degy = 1580;//1800;
  param.pgain_degz = 2000;
  param.dgain_degx = 22;//30;
  param.dgain_degy = 22;//30;
  param.dgain_degz = 30;
  param.pwm_or_servo = 0;  
  param.propeller_monitor = 1;
  pxset_pconfig(&param);
}

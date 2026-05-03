#include "board_config.h"

static void delay_ms(unsigned int ms) {
  unsigned int i;
  unsigned int j;

  for (i = 0; i < ms; ++i) {
    for (j = 0; j < 120; ++j) {
    }
  }
}

static void motors_stop(void) {
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_back(void) {
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 1;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

void main(void) {
  motors_stop();

  while (1) {
    motors_forward();
    delay_ms(2000);
    motors_stop();
    delay_ms(1000);
    motors_back();
    delay_ms(2000);
    motors_stop();
    delay_ms(1000);
  }
}

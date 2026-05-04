#include "board_config.h"

typedef unsigned char u8;
typedef unsigned int u16;

static volatile u8 g_motion = 0;

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
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_back(void) {
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 1;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void delay_ms(u16 ms) {
  u16 i;
  u16 j;

  for (i = 0; i < ms; ++i) {
    for (j = 0; j < 120; ++j) {
    }
  }
}

static void motors_left_turn(void) {
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_right_turn(void) {
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_RIGHT_EN = 1;
}

static void motors_left_90_then_forward(void) {
  motors_left_turn();
  delay_ms(160);
  motors_forward();
}

static void motors_right_90_then_forward(void) {
  motors_right_turn();
  delay_ms(220);
  motors_forward();
}

static void uart_init(void) {
  TMOD = 0x20;
  PCON &= 0x7F;
  SCON = 0x50;
  TH1 = UART_TIMER1_RELOAD;
  TL1 = UART_TIMER1_RELOAD;
  TI = 0;
  RI = 0;
  TR1 = 1;
  ES = 1;
  EA = 1;
}

void main(void) {
  motors_stop();
  uart_init();

  while (1) {
    if (g_motion) {
      u8 motion = g_motion;
      g_motion = 0;

      switch (motion) {
        case 'F':
          motors_forward();
          break;
        case 'B':
          motors_back();
          break;
        case 'L':
          motors_left_90_then_forward();
          break;
        case 'R':
          motors_right_90_then_forward();
          break;
        default:
          motors_stop();
          break;
      }
    }
  }
}

void uart_isr(void) __interrupt(4) {
  if (TI) {
    TI = 0;
  }

  if (RI) {
    u8 value;

    RI = 0;
    value = SBUF;

    if (value == 'F' || value == 'B' || value == 'L' || value == 'R' ||
        value == 'S') {
      g_motion = value;
    }
  }
}

#include "car_firmware.h"

#include "board_config.h"
#include "mcs51_sdcc.h"

typedef unsigned char u8;

static volatile u8 g_rx_seen = 0;
static volatile u8 g_stop_seen = 0;

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

void CarFirmware_Init(void) {
  motors_stop();

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

void CarFirmware_Task(void) {
  if (g_stop_seen) {
    g_stop_seen = 0;
    motors_stop();
  }

  if (g_rx_seen) {
    g_rx_seen = 0;
    motors_forward();
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

    if (value == 'F') {
      g_rx_seen = 1;
    } else if (value == 'S') {
      g_stop_seen = 1;
    }
  }
}

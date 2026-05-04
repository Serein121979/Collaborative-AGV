#include "board_config.h"

typedef unsigned char u8;
typedef unsigned int u16;

/* 串口接收测试标志：
 * 收到 F 后前进，收到 S 后停止。
 * 这个测试用来确认 ESP TX -> 51 RX 这条线和波特率是否正确。 */
static volatile u8 g_rx_seen = 0;
static volatile u8 g_stop_seen = 0;

static void motors_stop(void) {
  /* 停车并清方向脚。 */
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  /* 收到 F 后让车前进。
   * 这个测试文件假设左右电机同向接线。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void uart_init(void) {
  /* 9600 波特率串口初始化，参数要和 ESP 侧 Serial.begin(9600) 对齐。 */
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
  /* 上电先停车，然后等待串口字节。 */
  motors_stop();
  uart_init();

  while (1) {
    /* 收到停止命令 S。 */
    if (g_stop_seen) {
      g_stop_seen = 0;
      motors_stop();
    }

    /* 收到前进命令 F。 */
    if (g_rx_seen) {
      g_rx_seen = 0;
      motors_forward();
    }
  }
}

void uart_isr(void) __interrupt(4) {
  /* 8051 串口中断：RI 为收到数据，TI 为发送完成。 */
  if (TI) {
    TI = 0;
  }

  if (RI) {
    u8 value;

    RI = 0;
    value = SBUF;

    /* 只识别 F 和 S，其他字符忽略。 */
    if (value == 'F') {
      g_rx_seen = 1;
    } else if (value == 'S') {
      g_stop_seen = 1;
    }
  }
}

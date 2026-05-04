#include "board_config.h"

typedef unsigned char u8;

/* 串口中断收到命令后只设置标志，主循环再根据标志控制电机。
 * g_rx_seen 表示收到前进 F，g_stop_seen 表示收到停止 S。 */
static volatile u8 g_rx_seen = 0;
static volatile u8 g_stop_seen = 0;

static void motors_stop(void) {
  /* 停车：关闭使能脚，同时把方向脚清零。 */
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  /* 2 号车前进。
   * 右电机方向和左电机相反，所以右侧使用 IN1=0、IN2=1。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void uart_init(void) {
  /* 串口初始化为 9600 8N1：
   * Timer1 自动重装产生波特率，SCON=0x50 打开串口接收。 */
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
  /* 上电默认停车，然后开始等待 ESP 通过串口发送 F 或 S。 */
  motors_stop();
  uart_init();

  while (1) {
    /* 收到 S 后停车。 */
    if (g_stop_seen) {
      g_stop_seen = 0;
      motors_stop();
    }

    /* 收到 F 后前进。当前 2 号车程序只实现了前进和停止。 */
    if (g_rx_seen) {
      g_rx_seen = 0;
      motors_forward();
    }
  }
}

void uart_isr(void) __interrupt(4) {
  /* 串口中断号 4：接收和发送共用一个中断入口。 */
  if (TI) {
    /* 本程序不依赖发送完成事件，只清标志。 */
    TI = 0;
  }

  if (RI) {
    u8 value;

    RI = 0;
    value = SBUF;

    /* ESP 到 51 的简化协议：F=前进，S=停止。 */
    if (value == 'F') {
      g_rx_seen = 1;
    } else if (value == 'S') {
      g_stop_seen = 1;
    }
  }
}

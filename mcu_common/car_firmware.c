#include "car_firmware.h"

#include "board_config.h"
#include "mcs51_sdcc.h"

typedef unsigned char u8;

/* volatile 表示这些变量会在中断函数里被修改。
 * 主循环读取它们时，编译器不能擅自优化掉读取动作。 */
static volatile u8 g_rx_seen = 0;
static volatile u8 g_stop_seen = 0;

static void motors_stop(void) {
  /* 停车时先关使能，再把方向脚全部拉低，避免电机驱动保持某个方向输出。 */
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  /* 这个公共版本认为左右电机同向接线：两边 IN1=1、IN2=0 就是前进。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

void CarFirmware_Init(void) {
  motors_stop();

  /* 串口初始化：
   * TMOD=0x20：Timer1 工作在方式 2，也就是 8 位自动重装。
   * PCON&=0x7F：关闭 SMOD 倍频。
   * SCON=0x50：串口方式 1，允许接收。
   * TH1/TL1=0xFD：11.0592MHz 晶振下约为 9600 波特率。
   * ES/EA=1：打开串口中断和总中断。 */
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
  /* 中断里只记录“收到了什么”，真正控制电机放在主循环执行。
   * 这样中断函数足够短，系统更稳定。 */
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
  /* 串口中断号 4，同时包含发送完成 TI 和接收完成 RI 两种情况。 */
  if (TI) {
    TI = 0;
  }

  if (RI) {
    u8 value;

    RI = 0;
    value = SBUF;

    /* ESP 发来单字符命令：
     * F 表示前进，S 表示停止。
     * 这里只置标志，不直接操作电机。 */
    if (value == 'F') {
      g_rx_seen = 1;
    } else if (value == 'S') {
      g_stop_seen = 1;
    }
  }
}

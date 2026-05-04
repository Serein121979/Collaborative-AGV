#include "board_config.h"

typedef unsigned char u8;
typedef unsigned int u16;

/* 串口中断收到的运动命令。
 * 取值为 'F' 前进、'B' 后退、'L' 左转后继续前进、'R' 右转后继续前进、'S' 停止。
 * volatile 的原因是：这个变量在中断函数里写，在 main 主循环里读。 */
static volatile u8 g_motion = 0;

static void motors_stop(void) {
  /* 关闭两路电机使能，并把方向脚全部置 0，确保车不动。 */
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  /* 1 号车的“前进”电平组合。
   * 注意右电机这里是 IN1=0、IN2=1，说明右电机安装方向和左电机相反。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_back(void) {
  /* 后退就是把前进时的左右电机方向全部反过来。 */
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

  /* 简单的软件延时。
   * 这不是精确定时，只适合测试或粗略控制转弯时长。 */
  for (i = 0; i < ms; ++i) {
    for (j = 0; j < 120; ++j) {
    }
  }
}

static void motors_left_turn(void) {
  /* 原地/近似原地左转：左轮后退，右轮前进。 */
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_right_turn(void) {
  /* 原地/近似原地右转：左轮前进，右轮后退。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_RIGHT_EN = 1;
}

static void motors_left_90_then_forward(void) {
  /* 左转一小段时间后恢复前进。
   * 160ms 是经验值，真实 90 度需要按电池电压、地面摩擦和车速微调。 */
  motors_left_turn();
  delay_ms(160);
  motors_forward();
}

static void motors_right_90_then_forward(void) {
  /* 右转一小段时间后恢复前进。
   * 这里用 220ms，说明右转和左转的实际响应不完全对称。 */
  motors_right_turn();
  delay_ms(220);
  motors_forward();
}

static void uart_init(void) {
  /* 串口初始化：
   * Timer1 方式 2 产生波特率，SCON=0x50 表示 8 位 UART 并允许接收。
   * UART_TIMER1_RELOAD 在 board_config.h 里定义，当前对应 9600 波特率。 */
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
  /* 上电先停车，再打开串口，避免复位瞬间电机误动作。 */
  motors_stop();
  uart_init();

  while (1) {
    /* 主循环轮询中断留下来的命令。
     * 中断负责“收命令”，主循环负责“执行命令”。 */
    if (g_motion) {
      u8 motion = g_motion;
      g_motion = 0;

      switch (motion) {
        case 'F':
          /* ESP 发来 F：前进。 */
          motors_forward();
          break;
        case 'B':
          /* ESP 发来 B：后退。 */
          motors_back();
          break;
        case 'L':
          /* ESP 发来 L：左转一段时间，然后继续前进。 */
          motors_left_90_then_forward();
          break;
        case 'R':
          /* ESP 发来 R：右转一段时间，然后继续前进。 */
          motors_right_90_then_forward();
          break;
        default:
          /* 包括 S 在内的其他合法命令都按停车处理。 */
          motors_stop();
          break;
      }
    }
  }
}

void uart_isr(void) __interrupt(4) {
  /* 串口中断服务函数，8051 的串口中断号是 4。 */
  if (TI) {
    /* TI 是发送完成标志。本程序主要接收命令，不主动发送数据，清掉即可。 */
    TI = 0;
  }

  if (RI) {
    u8 value;

    RI = 0;
    value = SBUF;

    /* 只接受这五个单字符命令，其他字节直接忽略，避免误动作。 */
    if (value == 'F' || value == 'B' || value == 'L' || value == 'R' ||
        value == 'S') {
      g_motion = value;
    }
  }
}

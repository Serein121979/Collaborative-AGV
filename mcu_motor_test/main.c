#include "board_config.h"

static void delay_ms(unsigned int ms) {
  unsigned int i;
  unsigned int j;

  /* 粗略软件延时，用来让电机保持某个动作一段时间。
   * 这个延时会阻塞 CPU，测试程序可以这样写，正式程序一般要少用。 */
  for (i = 0; i < ms; ++i) {
    for (j = 0; j < 120; ++j) {
    }
  }
}

static void motors_stop(void) {
  /* 停止电机，防止切换方向时直接反打。 */
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
}

static void motors_forward(void) {
  /* 测试前进。这个测试文件假设左右电机同向接线。 */
  MOTOR_LEFT_IN1 = 1;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 1;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

static void motors_back(void) {
  /* 测试后退：方向脚与前进相反。 */
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 1;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 1;
  MOTOR_LEFT_EN = 1;
  MOTOR_RIGHT_EN = 1;
}

void main(void) {
  /* 电机测试循环：
   * 前进 2 秒 -> 停 1 秒 -> 后退 2 秒 -> 停 1 秒，然后不断重复。
   * 如果方向不对，优先检查 board_config.h 的引脚和电机接线。 */
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

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "../mcu_common/mcs51_sdcc.h"

/* 1 号车 51 单片机的硬件引脚映射。
 * 如果你实际接线和这里不同，只需要改这些宏，主程序不用动。 */

/* 左电机方向控制：IN1/IN2 一高一低时决定正转或反转。 */
#define MOTOR_LEFT_IN1 P1_2
#define MOTOR_LEFT_IN2 P1_3

/* 右电机方向控制。不同小车左右电机安装方向可能相反，
 * 所以前进时左右电机的高低电平不一定完全一样。 */
#define MOTOR_RIGHT_IN1 P1_6
#define MOTOR_RIGHT_IN2 P1_7

/* 电机使能脚：置 1 允许电机转动，置 0 停止输出。 */
#define MOTOR_LEFT_EN P1_4
#define MOTOR_RIGHT_EN P1_5

/* 11.0592MHz 晶振下，Timer1 自动重装值 0xFD 通常对应串口 9600 波特率。 */
#define UART_TIMER1_RELOAD 0xFD

/* Timer0 的 1ms 定时重装值，给后续超时/软件 PWM 逻辑预留。 */
#define TIMER0_RELOAD_HIGH 0xFC
#define TIMER0_RELOAD_LOW 0x66

/* 超过这个时间没有收到有效命令时应停车，单位 ms。 */
#define COMMAND_TIMEOUT_MS 1000

/* 转弯时内侧轮速度百分比，给更完整的差速转弯逻辑预留。 */
#define TURN_INNER_SPEED_PERCENT 40

#endif

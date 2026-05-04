#ifndef MCS51_SDCC_H
#define MCS51_SDCC_H

/* SDCC 没有 Keil 的 reg52.h，这里手动声明 STC89C52/8051 常用寄存器。
 * __sfr 表示特殊功能寄存器，__sbit 表示某个寄存器里的单个位。
 * 例如 P1_2 就是 P1 口第 2 位，对应硬件引脚 P1.2。 */

/* 电源控制寄存器。这里主要用 PCON.7(SMOD) 控制串口波特率倍频。 */
__sfr __at(0x87) PCON;

/* 定时器相关寄存器：串口波特率通常由 Timer1 产生。 */
__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8A) TL0;
__sfr __at(0x8B) TL1;
__sfr __at(0x8C) TH0;
__sfr __at(0x8D) TH1;

/* P1 口整组寄存器，电机驱动引脚都接在 P1 口上。 */
__sfr __at(0x90) P1;

/* 串口控制寄存器和串口数据缓冲寄存器。 */
__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;

/* 中断允许寄存器。 */
__sfr __at(0xA8) IE;

/* 定时器运行控制位。TR1=1 表示启动 Timer1。 */
__sbit __at(0x8C) TR0;
__sbit __at(0x8E) TR1;

/* 串口中断标志位：RI 收到数据，TI 发送完成。 */
__sbit __at(0x98) RI;
__sbit __at(0x99) TI;

/* 中断开关：ET0 是 Timer0，ES 是串口，EA 是总中断。 */
__sbit __at(0xA9) ET0;
__sbit __at(0xAC) ES;
__sbit __at(0xAF) EA;

/* P1 口每一位的名字，方便 board_config.h 把它们映射成电机引脚。 */
__sbit __at(0x90) P1_0;
__sbit __at(0x91) P1_1;
__sbit __at(0x92) P1_2;
__sbit __at(0x93) P1_3;
__sbit __at(0x94) P1_4;
__sbit __at(0x95) P1_5;
__sbit __at(0x96) P1_6;
__sbit __at(0x97) P1_7;

#endif

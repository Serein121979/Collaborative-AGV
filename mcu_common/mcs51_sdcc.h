#ifndef MCS51_SDCC_H
#define MCS51_SDCC_H

__sfr __at(0x87) PCON;

__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8A) TL0;
__sfr __at(0x8B) TL1;
__sfr __at(0x8C) TH0;
__sfr __at(0x8D) TH1;

__sfr __at(0x90) P1;

__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;

__sfr __at(0xA8) IE;

__sbit __at(0x8C) TR0;
__sbit __at(0x8E) TR1;

__sbit __at(0x98) RI;
__sbit __at(0x99) TI;

__sbit __at(0xA9) ET0;
__sbit __at(0xAC) ES;
__sbit __at(0xAF) EA;

__sbit __at(0x90) P1_0;
__sbit __at(0x91) P1_1;
__sbit __at(0x92) P1_2;
__sbit __at(0x93) P1_3;
__sbit __at(0x94) P1_4;
__sbit __at(0x95) P1_5;
__sbit __at(0x96) P1_6;
__sbit __at(0x97) P1_7;

#endif

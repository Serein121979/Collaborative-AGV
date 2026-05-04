#ifndef CAR_FIRMWARE_H
#define CAR_FIRMWARE_H

/* 公共 51 小车固件接口。
 * 这个文件只声明初始化函数和循环任务函数，真正实现放在 car_firmware.c。
 * 当前 mcu_car_a / mcu_car_b 的 main.c 里已经各自写了控制逻辑，
 * 所以这里更像是后续抽公共代码时可以复用的接口。 */

/* 初始化电机状态、串口和中断。 */
void CarFirmware_Init(void);

/* 主循环里反复调用，用来处理串口中断留下来的运动标志。 */
void CarFirmware_Task(void);

#endif

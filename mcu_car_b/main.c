#include "board_config.h"
#include "../mcu_common/car_firmware.h"

void main(void) {
  CarFirmware_Init();

  while (1) {
    CarFirmware_Task();
  }
}

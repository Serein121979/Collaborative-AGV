# 双车协同 AGV v1

这个仓库已经调整为一套“只用 VSCode 就能走完整流程”的工程：

- `ESP8266` 使用 `PlatformIO` 编译和烧录
- `51 单片机` 使用 `PlatformIO + SDCC` 编译
- `STC89C52RC` 这类 51 芯片通过 `stcgal` 在 VSCode 里辅助烧录

也就是说，你不需要中途切到 `Keil` 或 Arduino IDE。

## 一、VSCode 里需要安装什么

请先在 VSCode 插件商店安装这 3 个扩展：

1. `PlatformIO IDE`
2. `C/C++`
3. `Python`

说明：

- 真正必须的是 `PlatformIO IDE`
- `C/C++` 主要负责代码跳转、语法提示
- `Python` 建议安装，因为仓库里的 51 烧录辅助脚本是 Python 写的

仓库里已经提供了推荐插件文件：

- [.vscode/extensions.json](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/.vscode/extensions.json:1)

## 二、电脑上第一次需要准备什么

### 1. 安装 PlatformIO IDE

装好 `PlatformIO IDE` 扩展后，重启一次 VSCode，等它把工具链自动下载完成。

PlatformIO 官方文档说明：如果你是在 VSCode 里使用它的扩展，一般不需要单独全局安装 PlatformIO Core：

- https://docs.platformio.org/en/latest/core/installation/index.html

### 2. 安装 `stcgal`

如果你的 51 小车板子是常见的 `STC89C52RC`，并且是通过串口 Bootloader 烧录，那么还需要安装：

```bash
pip3 install stcgal
```

`stcgal` 是常用的 STC 芯片开源烧录工具：

- https://github.com/grigorig/stcgal

## 三、仓库结构说明

- [platformio.ini](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/platformio.ini:1)：PlatformIO 多环境配置
- [esp_master/main.cpp](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/esp_master/main.cpp:1)：1 号车 ESP8266 主站程序
- [esp_slave/main.cpp](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/esp_slave/main.cpp:1)：2 号车 ESP8266 从站程序
- [mcu_common/](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/mcu_common/)：两台 51 共用的底层逻辑
- [mcu_car_a/](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/mcu_car_a/)：1 号车 51 工程入口和引脚配置
- [mcu_car_b/](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/mcu_car_b/)：2 号车 51 工程入口和引脚配置
- [.vscode/tasks.json](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/.vscode/tasks.json:1)：VSCode 上传辅助任务
- [tools/stc_upload.py](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/tools/stc_upload.py:1)：调用 `stcgal` 的上传脚本
- [docs/protocol.md](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/docs/protocol.md:1)：TCP 和串口协议文档

## 四、PlatformIO 环境说明

这个仓库里一共定义了 4 个环境：

- `esp_master`
- `esp_slave`
- `mcu_car_a`
- `mcu_car_b`

当前默认的 51 配置是：

- 芯片：`STC89C52RC`
- 晶振：`11.0592 MHz`

这套配置对应的是 PlatformIO 官方支持的 `intel_mcs51` 平台：

- https://docs.platformio.org/en/latest/platforms/intel_mcs51.html
- https://docs.platformio.org/en/latest/boards/intel_mcs51/STC89C52RC.html

如果你后面确认自己的板子不是这个型号，我们再改 `platformio.ini` 就行。

## 五、怎么在 VSCode 里编译和烧录

### ESP8266 部分

在 VSCode 的 PlatformIO 侧边栏里操作：

1. 打开 PlatformIO 侧边栏
2. 找到 `esp_master` 或 `esp_slave`
3. 点击 `Build`
4. 接上对应的 ESP8266 开发板
5. 点击 `Upload`

当前默认板型是：

- `NodeMCU 1.0 (ESP-12E Module)`

也就是 `platformio.ini` 里的：

```ini
board = nodemcuv2
```

如果你的 ESP8266 不是这个板型，改 [platformio.ini](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/platformio.ini:1) 里的 `board` 即可。

### 51 单片机部分

先编译：

1. 打开 PlatformIO 侧边栏
2. 找到 `mcu_car_a` 或 `mcu_car_b`
3. 点击 `Build`

再烧录：

1. 按 `Cmd+Shift+P`
2. 输入并执行 `Tasks: Run Task`
3. 选择：
   - `Upload MCU Car A (stcgal)`
   - 或 `Upload MCU Car B (stcgal)`
4. 输入串口号
5. 当 `stcgal` 提示等待单片机时，给 51 板子重新上电或按复位键

上传任务会自动去 `.pio/build/<env>/` 下面找编译好的固件。

## 六、接线说明

### ESP8266 和 51 串口连接

- `ESP TX` 接 `51 RX`
- `51 TX` 接 `ESP RX`
- `GND` 共地

注意：

- `ESP8266` 是 `3.3V` 逻辑
- 很多 `51` 小车板是 `5V` 逻辑
- `51 TX -> ESP RX` 这一路不能直接硬连，建议加分压或电平转换

### 电机驱动默认引脚

根据你提供的 `ZY-1` 安装指导书，当前默认已经改成手册里的电机驱动接线：

- `P1.2` -> `LEFT_IN1`
- `P1.3` -> `LEFT_IN2`
- `P1.4` -> `LEFT_EN`
- `P1.6` -> `RIGHT_IN1`
- `P1.7` -> `RIGHT_IN2`
- `P1.5` -> `RIGHT_EN`

如果你的电机驱动接线不同，只改这两个文件就行：

- [mcu_car_a/board_config.h](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/mcu_car_a/board_config.h:1)
- [mcu_car_b/board_config.h](/Users/Zhuanz/Documents/协同小车/Collaborative-AGV/mcu_car_b/board_config.h:1)

## 七、手机控制流程

1. 给 1 号车上电
2. 手机连接 WiFi：`AGV_CTRL`
3. 打开手机上的网络调试精灵
4. 选择 `TCP Client`
5. 连接 `192.168.4.1:8080`
6. 先发送：

```text
HELLO,PHONE
```

7. 收到返回：

```text
OK,PHONE
```

8. 然后发送控制命令，例如：

```text
CMD,1,FWD,180
CMD,2,LEFT,160
CMD,3,STOP
```

## 八、ESP8266 和 51 之间的串口协议

```text
#STOP
#RUN,FWD,180
#RUN,BACK,180
#RUN,LEFT,160
#RUN,RIGHT,160
#RUN,SPINL,180
#RUN,SPINR,180
```

51 固件内置了保护逻辑：

- 如果 `1000 ms` 内没有收到新的有效串口命令，就会自动停车

## 九、几个你现在最关心的结论

- `ESP8266` 和 `51` 不是同一个编译目标，所以本来就不能拿同一个编译器一起编
- 原先会报错的 `reg52.h` 已经从当前有效构建链路里移除了
- 现在 51 这部分走的是 `PlatformIO + SDCC`
- 所以你可以全程留在 `VSCode` 里开发
- `ESP8266` 的 `ESP8266WiFi.h` 由 PlatformIO 自动提供，不需要你手动到处拷头文件

## 十、如果 VSCode 里还在报红怎么办

一般按这个顺序排查：

1. 确认 `PlatformIO IDE` 已经安装完成
2. 重启一次 VSCode
3. 等 PlatformIO 自己完成索引
4. 执行 `PlatformIO: Rebuild IntelliSense Index`
5. 确认你打开的是整个项目根目录，而不是只打开某一个子文件夹

## 十一、你下一步最该做什么

建议你先做下面两件事：

1. 安装 `PlatformIO IDE`
2. 对照你真实硬件，把两台车的 `board_config.h` 引脚改成实际接线

改完这两个点，我们就可以继续做第一次编译和联调。

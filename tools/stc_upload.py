#!/usr/bin/env python3
"""把 PlatformIO 编译出的 51 固件交给 stcgal 烧录。

这个脚本本身不编译代码，只负责：
1. 到 .pio/build/<env>/ 里找到 firmware.hex / firmware.ihx / firmware.bin；
2. 调用 python -m stcgal；
3. 把串口号、协议和固件路径传给 stcgal。
"""

import argparse
import pathlib
import subprocess
import sys


def find_firmware(env_name: str) -> pathlib.Path:
    """根据 PlatformIO 环境名查找已经编译好的固件文件。"""
    build_dir = pathlib.Path(".pio") / "build" / env_name

    # 不同 51 工具链可能生成不同扩展名，这里按常见顺序依次尝试。
    candidates = (
        build_dir / "firmware.hex",
        build_dir / "firmware.ihx",
        build_dir / "firmware.bin",
    )

    for candidate in candidates:
        if candidate.exists():
            # 找到第一个存在的固件文件就返回。
            return candidate

    # 如果一个都没有，通常是用户还没有先执行 PlatformIO Build。
    raise FileNotFoundError(
        f"Firmware artifact not found for env '{env_name}'. "
        f"Build the environment first in PlatformIO. Looked in: "
        + ", ".join(str(path) for path in candidates)
    )


def main() -> int:
    # 解析 VSCode task 或命令行传入的参数。
    parser = argparse.ArgumentParser(
        description="Upload STC 8051 firmware built by PlatformIO via stcgal."
    )
    parser.add_argument("--env", required=True, help="PlatformIO environment name")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/cu.usbserial-1410")
    parser.add_argument(
        "--protocol",
        default="auto",
        help="stcgal protocol, default: auto",
    )
    args = parser.parse_args()

    # 先定位固件，再拼出 stcgal 命令。
    firmware = find_firmware(args.env)
    command = [
        sys.executable,
        "-m",
        "stcgal",
        "-P",
        args.protocol,
        "-p",
        args.port,
        str(firmware),
    ]

    # 打印真实执行命令，方便排查端口、协议或固件路径问题。
    print("Running:", " ".join(command))
    print("Tip: when stcgal waits for the MCU, power-cycle or reset the 51 board.")

    # subprocess.call 会把 stcgal 的退出码原样返回给 VSCode/终端。
    return subprocess.call(command)


if __name__ == "__main__":
    # main 返回 0 表示成功，非 0 表示失败。
    raise SystemExit(main())

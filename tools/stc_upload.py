#!/usr/bin/env python3
import argparse
import pathlib
import subprocess
import sys


def find_firmware(env_name: str) -> pathlib.Path:
    build_dir = pathlib.Path(".pio") / "build" / env_name
    candidates = (
        build_dir / "firmware.hex",
        build_dir / "firmware.ihx",
        build_dir / "firmware.bin",
    )

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError(
        f"Firmware artifact not found for env '{env_name}'. "
        f"Build the environment first in PlatformIO. Looked in: "
        + ", ".join(str(path) for path in candidates)
    )


def main() -> int:
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

    print("Running:", " ".join(command))
    print("Tip: when stcgal waits for the MCU, power-cycle or reset the 51 board.")
    return subprocess.call(command)


if __name__ == "__main__":
    raise SystemExit(main())

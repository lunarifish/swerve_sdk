#!/usr/bin/env python3
"""
数据采集脚本：从 USB CDC 虚拟串口读取关节数据，手动按 Enter 键采集一组数据，保存为 CSV 文件。
用于重力参数 b1, b2 的辨识回归。

下位机发送格式（每行）：
  stamp,q1,q2,q3,dq1,dq2,dq3,tau1,tau2,tau3\r\n

使用方法:
  python3 capture_data_serial.py [--port COM3] [--baud 115200] [--output data.csv]

操作流程:
  1. 将机械臂置于某个静止姿态
  2. 按 Enter 采集当前关节角度和力矩
  3. 重复以上步骤，采集足够多的数据点
  4. 按 q + Enter 退出并保存
"""

import argparse
import csv
import os
import sys
import threading
from datetime import datetime

import serial


FIELDNAMES = ["stamp", "q1", "q2", "q3", "dq1", "dq2", "dq3", "tau1", "tau2", "tau3"]


class SerialReader:
    """后台线程持续读取串口，保留最新一帧有效数据。"""

    def __init__(self, port: str, baudrate: int):
        self._ser = serial.Serial(port, baudrate, timeout=1)
        self._latest: dict | None = None
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        print(f"[INFO] 已连接串口 {port}，波特率 {baudrate}")

    def _read_loop(self):
        while not self._stop_event.is_set():
            try:
                raw = self._ser.readline()
                if not raw:
                    continue
                line = raw.decode("ascii", errors="ignore").strip()
                if not line:
                    continue
                parts = line.split(",")
                if len(parts) != 10:
                    continue
                sample = {k: float(v) for k, v in zip(FIELDNAMES, parts)}
                with self._lock:
                    self._latest = sample
            except (serial.SerialException, UnicodeDecodeError, ValueError):
                continue

    def get_latest(self) -> dict | None:
        with self._lock:
            return dict(self._latest) if self._latest else None

    def close(self):
        self._stop_event.set()
        self._ser.close()


def main():
    parser = argparse.ArgumentParser(description="从串口手动采集关节数据")
    parser.add_argument("-p", "--port", default="COM8", help="串口号，例如 COM3 或 /dev/ttyACM0")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="波特率（默认 115200）")
    default_output = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        f"data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
    )
    parser.add_argument("-o", "--output", default=default_output, help="输出 CSV 文件路径")
    args = parser.parse_args()

    try:
        reader = SerialReader(args.port, args.baud)
    except serial.SerialException as e:
        print(f"[ERROR] 无法打开串口 {args.port}: {e}", file=sys.stderr)
        sys.exit(1)

    samples: list[dict] = []

    print("按 Enter 采集一组数据，输入 q 退出并保存。")

    try:
        while True:
            try:
                line = input()
            except EOFError:
                break

            if line.strip().lower() == "q":
                break

            sample = reader.get_latest()
            if sample is None:
                print("[WARN] 尚未收到有效数据，请确认串口连接和下位机是否正常发送。")
                continue

            samples.append(sample)
            n = len(samples)
            print(
                f"[#{n}] q=({sample['q1']:.4f}, {sample['q2']:.4f}, {sample['q3']:.4f})  "
                f"tau=({sample['tau1']:.4f}, {sample['tau2']:.4f}, {sample['tau3']:.4f})"
            )
    except KeyboardInterrupt:
        pass
    finally:
        reader.close()

    if not samples:
        print("[WARN] 没有采集到数据，不保存文件。")
        return

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(samples)

    print(f"[INFO] 已保存 {len(samples)} 组数据到 {args.output}")


if __name__ == "__main__":
    main()

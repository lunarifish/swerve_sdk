"""
后台守护线程，通过SocketCAN持续接收下位机状态帧，对外提供线程安全的 API 获取最新数据。

下位机通过 CAN FD 总线持续反馈三类帧（帧ID = base_frame_id + offset）:

  offset=1: SystemStatus   (电机状态、FSM状态、总线电压)
  offset=2: JointState     (5关节位置/速度/力矩)
  offset=3: PressureSensorData (压力传感器)
"""

import struct
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Optional

import can


# ---------------------------------------------------------------------------
# FSM 状态枚举（与下位机 StateId 对齐）
# ---------------------------------------------------------------------------
class FsmState:
    kInit: int = 0        # 开机初始化中
    kNoForce: int = 1     # 强制无力
    kManual: int = 2      # 遥控器接入，手动模式
    kIdle: int = 3        # 空闲，可执行轨迹
    kHostControl: int = 4  # 正在被上位机控制
    kSequencing: int = 5  # 正在执行动作序列
    kFault: int = 6       # 故障状态


# ---------------------------------------------------------------------------
# 数据类
# ---------------------------------------------------------------------------
@dataclass
class SystemStatus:
    """下位机系统状态帧 (offset=1)."""
    j1_motor_status: int
    j2_motor_status: int
    j3_motor_status: int
    j4_motor_status: int
    j5_motor_status: int
    is_pressure_sensor_online: bool
    fsm_state: int
    vbus: float


@dataclass
class JointState:
    """关节状态帧 (offset=2), 单位: rad, rad/s, N·m."""
    position: tuple[float, float, float, float, float]
    velocity: tuple[float, float, float, float, float]
    effort: tuple[float, float, float, float, float]


@dataclass
class PressureSensorData:
    """压力传感器帧 (offset=3), 单位: N."""
    pressure: float


@dataclass
class SensorMisc1:
    """传感器杂项数据帧 (offset=7).

    Attributes:
        motor_coil_temp: 5 个电机的线圈温度 (°C), uint8.
        motor_mos_temp:  5 个电机的 MOS 管温度 (°C), uint8.
    """
    motor_coil_temp: tuple[int, int, int, int, int]
    motor_mos_temp: tuple[int, int, int, int, int]


# ---------------------------------------------------------------------------
# 帧 ID 偏移量（与下位机 kFrameIdOffset 对齐）
# ---------------------------------------------------------------------------
_OFFSET_SYSTEM_STATUS = 1
_OFFSET_JOINT_STATE = 2
_OFFSET_PRESSURE = 3
_OFFSET_SENSOR_MISC1 = 7


class RxDaemon:
    """SocketCAN 后台读取守护线程。

    用法::

        def on_status_change(online: bool) -> None:
            print(f"Arm {'online' if online else 'offline'}")

        rx = RxDaemon(channel="can0", base_frame_id=0x233)
        rx.register_online_callback(on_status_change)
        # ... 机械臂运行中 ...
        if rx.is_online():
            joints = rx.get_joint_state()
        rx.close()
    """

    def __init__(
        self,
        channel: str = "can0",
        base_frame_id: int = 0x233,
        watchdog_timeout: float = 1.0,
    ):
        """
        Args:
            channel:          SocketCAN 接口名
            base_frame_id:    基帧ID，下位机实际帧ID = base_frame_id + offset
            watchdog_timeout: 看门狗超时时间 (秒)，超过此时间未收到任何有效帧则认为离线.
        """
        self._channel = channel
        self._base_frame_id = base_frame_id
        self._watchdog_timeout = watchdog_timeout

        self._system_status: Optional[SystemStatus] = None
        self._joint_state: Optional[JointState] = None
        self._pressure: Optional[PressureSensorData] = None
        self._sensor_misc1: Optional[SensorMisc1] = None
        self._lock = threading.Lock()

        self._is_online: bool = False
        self._last_msg_time: float = 0.0

        # 在线状态变更回调
        self._online_callbacks: list[Callable[[bool], None]] = []

        # 线程控制
        self._stop_event = threading.Event()

        # CANFD总线对象
        self._bus = can.interface.Bus(
            bustype="socketcan", channel=channel, fd=True
        )

        # 启动守护线程
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    @staticmethod
    def _parse_system_status(data: bytes) -> SystemStatus:
        """解析 SystemStatus 帧 (11 字节 packed)."""
        (j1, j2, j3, j4, j5, ps_online, fsm,
         vbus) = struct.unpack("<7Bf", data[:11])
        return SystemStatus(
            j1_motor_status=j1,
            j2_motor_status=j2,
            j3_motor_status=j3,
            j4_motor_status=j4,
            j5_motor_status=j5,
            is_pressure_sensor_online=bool(ps_online),
            fsm_state=fsm,
            vbus=vbus,
        )

    @staticmethod
    def _parse_joint_state(data: bytes) -> JointState:
        """解析 JointState 帧 (60 字节 packed)."""
        values = struct.unpack("<15f", data[:60])
        return JointState(
            position=values[0:5],   # type: ignore[arg-type]
            velocity=values[5:10],  # type: ignore[arg-type]
            effort=values[10:15],   # type: ignore[arg-type]
        )

    @staticmethod
    def _parse_pressure(data: bytes) -> PressureSensorData:
        """解析 PressureSensorData 帧 (4 字节 packed)."""
        (p,) = struct.unpack("<f", data[:4])
        return PressureSensorData(pressure=p)

    @staticmethod
    def _parse_sensor_misc1(data: bytes) -> SensorMisc1:
        """解析 SensorDataMisc1 帧 (64 字节 packed)."""
        values = struct.unpack("<10B", data[:10])
        return SensorMisc1(
            motor_coil_temp=values[0:5],   # type: ignore[arg-type]
            motor_mos_temp=values[5:10],   # type: ignore[arg-type]
        )

    def _fire_online_callbacks(self, online: bool) -> None:
        """在锁外触发所有在线状态变更回调。"""
        callbacks: list[Callable[[bool], None]]
        with self._lock:
            callbacks = list(self._online_callbacks)
        for cb in callbacks:
            try:
                cb(online)
            except Exception:
                pass

    def _check_watchdog(self) -> None:
        """检查看门狗，若超时则标记离线并触发回调。"""
        elapsed = time.monotonic() - self._last_msg_time
        if elapsed > self._watchdog_timeout:
            with self._lock:
                was_online = self._is_online
                self._is_online = False
            if was_online:
                self._fire_online_callbacks(False)

    def _read_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                msg = self._bus.recv(timeout=0.5)
                if msg is None:
                    self._check_watchdog()
                    continue

                offset = msg.arbitration_id - self._base_frame_id
                data = msg.data
                now = time.monotonic()

                if offset == _OFFSET_SYSTEM_STATUS:
                    parsed = self._parse_system_status(data)
                    with self._lock:
                        self._system_status = parsed
                        self._last_msg_time = now
                        was_online = self._is_online
                        self._is_online = True
                    if not was_online:
                        self._fire_online_callbacks(True)

                elif offset == _OFFSET_JOINT_STATE:
                    parsed = self._parse_joint_state(data)
                    with self._lock:
                        self._joint_state = parsed
                        self._last_msg_time = now
                        was_online = self._is_online
                        self._is_online = True
                    if not was_online:
                        self._fire_online_callbacks(True)

                elif offset == _OFFSET_PRESSURE:
                    parsed = self._parse_pressure(data)
                    with self._lock:
                        self._pressure = parsed
                        self._last_msg_time = now
                        was_online = self._is_online
                        self._is_online = True
                    if not was_online:
                        self._fire_online_callbacks(True)

                elif offset == _OFFSET_SENSOR_MISC1:
                    parsed = self._parse_sensor_misc1(data)
                    with self._lock:
                        self._sensor_misc1 = parsed
                        self._last_msg_time = now
                        was_online = self._is_online
                        self._is_online = True
                    if not was_online:
                        self._fire_online_callbacks(True)

            except (can.CanError, OSError, struct.error):
                continue

    # public apis
    def get_system_status(self) -> Optional[SystemStatus]:
        """获取最新系统状态，无数据时返回None."""
        with self._lock:
            return self._system_status

    def get_joint_state(self) -> Optional[JointState]:
        """获取最新关节状态，无数据时返回None."""
        with self._lock:
            return self._joint_state

    def get_pressure(self) -> Optional[PressureSensorData]:
        """获取最新压力数据，无数据时返回None."""
        with self._lock:
            return self._pressure

    def get_sensor_misc1(self) -> Optional[SensorMisc1]:
        """获取最新传感器杂项数据（电机线圈/MOS温度），无数据时返回None."""
        with self._lock:
            return self._sensor_misc1

    def is_online(self) -> bool:
        """返回机械臂是否在线（在watchdog_timeout内收到过有效帧）."""
        with self._lock:
            return self._is_online

    def register_online_callback(self, cb: Callable[[bool], None]) -> None:
        """注册在线状态变更回调。

        当机械臂从在线变为离线，或从离线恢复在线时触发。

        Args:
            cb: (online: bool) -> None
        """
        with self._lock:
            self._online_callbacks.append(cb)

    def unregister_online_callback(self, cb: Callable[[bool], None]) -> None:
        """取消已注册的在线状态变更回调。"""
        with self._lock:
            try:
                self._online_callbacks.remove(cb)
            except ValueError:
                pass

    def close(self) -> None:
        """停止守护线程并关闭总线."""
        self._stop_event.set()
        self._bus.shutdown()

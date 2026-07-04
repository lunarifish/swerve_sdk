"""
SwerveArm main class entry
"""

import struct
import threading
from collections.abc import Callable
from importlib import resources
from typing import Optional

import can

from .calibrator import MotorCalibrator
from .ik_solver import IKSolver
from .rx_daemon import JointState, PressureSensorData, RxDaemon, SensorMisc1, SystemStatus


class ControlMode:
    """Host → Arm命令，ModeCtrlCommand."""
    kFreeMove: int = 0  # 机械臂不使力，外力可拖动（用于拖动示教）
    kNormal: int = 1    # 正常关节位控模式


# 帧 ID 偏移量
_OFFSET_ENABLE = 4
_OFFSET_MODE_CTRL = 5
_OFFSET_JOINT_CTRL = 6
_OFFSET_MIT_CTRL = 8


class EndEffectorPose:
    """末端执行器笛卡尔位姿.

        x, y, z:  位置 (m).
        qx, qy, qz, qw: 四元数姿态 (xyzw).
    """

    __slots__ = ("x", "y", "z", "qx", "qy", "qz", "qw")

    def __init__(
        self,
        x: float = 0.0,
        y: float = 0.0,
        z: float = 0.0,
        qx: float = 0.0,
        qy: float = 0.0,
        qz: float = 0.0,
        qw: float = 1.0,
    ):
        self.x = x
        self.y = y
        self.z = z
        self.qx = qx
        self.qy = qy
        self.qz = qz
        self.qw = qw

    @classmethod
    def from_list(cls, values: list[float]) -> "EndEffectorPose":
        """从 [x, y, z, qx, qy, qz, qw] 构造."""
        return cls(*values)

    def to_list(self) -> list[float]:
        """转为 [x, y, z, qx, qy, qz, qw]."""
        return [self.x, self.y, self.z, self.qx, self.qy, self.qz, self.qw]

    def __repr__(self) -> str:
        return (
            f"EndEffectorPose(x={self.x:.4f}, y={self.y:.4f}, z={self.z:.4f}, "
            f"qx={self.qx:.4f}, qy={self.qy:.4f}, qz={self.qz:.4f}, qw={self.qw:.4f})"
        )


class SwerveArm:
    """机械臂控制类。

    通过 SocketCAN 与下位机通信，提供机械臂控制与状态读取接口。
    示例用法::
        arm = SwerveArm(channel="can0")
        arm.enable_arm(True)
        arm.mode_ctrl(ControlMode.kNormal)

        # 关节控制
        arm.joint_ctrl([0.0, -0.5, 0.0, 1.0, 0.0])
        js = arm.get_joint_state()

        # 末端笛卡尔控制
        pose = arm.get_end_eff_pose()
        target = EndEffectorPose(0.3, 0.0, 0.2, 0, 0, 0, 1)
        arm.end_pose_ctrl(target)

        arm.close()
    """

    _URDF_RESOURCE = "resources/swerve.urdf"

    def __init__(
        self,
        channel: str = "can0",
        base_frame_id: int = 0x233,
        watchdog_timeout: float = 1.0,
    ):
        """
        Args:
            channel:          SocketCAN 接口名.
            base_frame_id:    基础帧 ID.
            watchdog_timeout: 看门狗超时时间 (秒).
        """
        self._channel = channel
        self._base_frame_id = base_frame_id

        # CAN发送总线
        self._send_bus = can.interface.Bus(
            bustype="socketcan", channel=channel, fd=True
        )
        self._send_lock = threading.Lock()

        # 接收守护线程
        self._rx = RxDaemon(
            channel=channel,
            base_frame_id=base_frame_id,
            watchdog_timeout=watchdog_timeout,
        )

        # 坐标标定器 —— 维护用户零点偏移，使 SDK 侧坐标可重新归零
        self.calibrator = MotorCalibrator()

        # 加载URDF模型
        self._pin_model: object
        self._pin_data: object
        self._end_eff_frame_id: int = -2
        self._ik_solver: IKSolver
        self._load_pinocchio_model()

    def _load_pinocchio_model(self) -> None:
        """从内置资源加载 URDF 模型到 Pinocchio."""
        try:
            import pinocchio as pin  # type: ignore[import-untyped]
        except ImportError:
            raise ImportError(
                "pinocchio 未安装，无法使用末端控制功能。"
                "请执行: pip install pin"
            )

        ref = resources.files("swerve_arm") / self._URDF_RESOURCE
        with resources.as_file(ref) as urdf_path:
            self._pin_model = pin.buildModelFromUrdf(str(urdf_path))

        self._pin_data = self._pin_model.createData()
        self._ik_solver = IKSolver(self._pin_model, self._pin_data)

        if self._pin_model.existFrame("END_EFF"):
            self._end_eff_frame_id = self._pin_model.getFrameId("J5")
        else:
            self._end_eff_frame_id = self._pin_model.njoints - 2

    # ------------------------------------------------------------------
    ### 用户零点管理
    ################
    def set_zero(self) -> None:
        """将当前关节位置设为零点。

        注意：纯软件实现，偏移量仅在内存中，SDK 重启后丢失。

        调用后 `get_joint_state().position` 全部为 (0, 0, 0, 0, 0)，
        后续所有控制指令的坐标均相对于新的零点。
        """
        js = self._rx.get_joint_state()
        if js is None:
            raise RuntimeError("无关节数据，无法设零")
        self.calibrator.set_zero(list(js.position))

    def clear_zero(self) -> None:
        """清除用户零点偏移，恢复为固件原始坐标。

        注意：纯软件实现，SDK 重启后零点丢失。
        """
        self.calibrator.clear()

    def set_zero_offset(self, offsets: list[float]) -> None:
        """直接设置零点偏移量（纯软件，不持久化保存）。

        Args:
            offsets: 5 个偏移值 [o1..o5]，单位 rad。
        """
        self.calibrator.set_offset(offsets)

    def get_zero_offset(self) -> list[float]:
        """获取当前零点偏移量。"""
        return self.calibrator.get_offset()

    # ------------------------------------------------------------------
    ### CAN通信
    ################
    def _send_frame(self, offset: int, payload: bytes) -> None:
        """发送一帧 CAN FD 数据."""
        frame_id = self._base_frame_id + offset
        msg = can.Message(
            arbitration_id=frame_id,
            data=payload,
            is_fd=True,
            is_extended_id=False,
        )
        with self._send_lock:
            try:
                self._send_bus.send(msg)
            except can.CanError as e:
                raise RuntimeError(
                    f"CAN 发送失败 (frame_id=0x{frame_id:X}): {e}") from e

    def enable_arm(self, enable: bool) -> None:
        """使能/失能机械臂。

        Args:
            enable: True 使能，False 失能.
        """
        payload = struct.pack("<B", 1 if enable else 0)
        self._send_frame(_OFFSET_ENABLE, payload)

    def mode_ctrl(self, mode: int) -> None:
        """切换控制模式。

        Args:
            mode: ControlMode.kFreeMove (可自由移动) 或 ControlMode.kNormal (正常位控).
        """
        payload = struct.pack("<B", mode)
        self._send_frame(_OFFSET_MODE_CTRL, payload)

    def joint_ctrl(self, positions: list[float]) -> None:
        """下发 5 轴关节目标角度。

        关节角和MIT两种指令可以同时使用，
        切换时位置路径保持连续，速度和力矩命令会瞬时切换到新指令指定的值。

        ``joint_ctrl()`` 只下发目标位置，速度、力矩由机械臂内置的动力学算法自动计算，
        用于常规场景。

        ``mit_ctrl()`` 允许上位机同时指定位置、速度、力矩三个维度的
        目标值，用于上位机运行自己的轨迹规划器、阻抗/导纳控制等需要精细力控的场景。

        Args:
            positions: 5 个关节目标角度 [j1..j5]，单位 rad（用户坐标）。
        """
        if len(positions) != 5:
            raise ValueError(f"需要 5 个关节角度，收到 {len(positions)} 个")
        payload = struct.pack("<5f", *self.calibrator.user_to_logical(positions))
        self._send_frame(_OFFSET_JOINT_CTRL, payload)

    def mit_ctrl(
        self,
        positions: list[float],
        velocities: list[float],
        torques: list[float],
    ) -> None:
        """类 MIT 模式控制，下发 5 轴关节目标角度、速度和前馈力矩。

        关节角和MIT两种指令可以同时使用，
        切换时位置路径保持连续，速度和力矩命令会瞬时切换到新指令指定的值。

        ``joint_ctrl()`` 只下发目标位置，速度、力矩由机械臂内置的动力学算法自动计算，
        用于常规场景。

        ``mit_ctrl()`` 允许上位机同时指定位置、速度、力矩三个维度的
        目标值，用于上位机运行自己的轨迹规划器、阻抗/导纳控制等需要精细力控的场景。


        Args:
            positions:  5 个关节目标角度 [j1..j5]，单位 rad.
            velocities: 5 个关节速度前馈 [j1..j5]，单位 rad/s.
            torques:    5 个关节力矩前馈 [j1..j5]，单位 N·m.
        """
        for name, vals in [("positions", positions), ("velocities", velocities), ("torques", torques)]:
            if len(vals) != 5:
                raise ValueError(f"{name} 需要 5 个值，收到 {len(vals)} 个")
        # 仅 position 需要坐标转换，velocity/torque 不受零偏影响
        payload = struct.pack("<15f", *self.calibrator.user_to_logical(positions), *velocities, *torques)
        self._send_frame(_OFFSET_MIT_CTRL, payload)

    # ------------------------------------------------------------------
    # 状态读取
    def get_joint_state(self) -> Optional[JointState]:
        """读取最新关节状态（角度/速度/力矩），返回用户坐标。"""
        js = self._rx.get_joint_state()
        if js is None:
            return None
        return JointState(
            position=self.calibrator.logical_to_user(list(js.position)),
            velocity=js.velocity,
            effort=js.effort,
        )

    def get_system_status(self) -> Optional[SystemStatus]:
        """读取最新系统状态."""
        return self._rx.get_system_status()

    def get_pressure(self) -> Optional[PressureSensorData]:
        """读取最新压力传感器数据."""
        return self._rx.get_pressure()

    def get_sensor_misc1(self) -> Optional[SensorMisc1]:
        """读取最新传感器杂项数据（包含：电机线圈/MOS温度）."""
        return self._rx.get_sensor_misc1()

    # ------------------------------------------------------------------
    # 在线监测
    def is_online(self) -> bool:
        """返回机械臂是否在线."""
        return self._rx.is_online()

    def register_online_callback(self, cb: Callable[[bool], None]) -> None:
        """注册在线状态变更回调."""
        self._rx.register_online_callback(cb)

    def unregister_online_callback(self, cb: Callable[[bool], None]) -> None:
        """取消在线状态变更回调."""
        self._rx.unregister_online_callback(cb)

    def get_end_eff_pose(self) -> EndEffectorPose:
        """读取末端执行器当前位姿（通过正运动学估算）。

        Returns:
            EndEffectorPose: 末端位姿 (x, y, z, qx, qy, qz, qw).
        """
        import pinocchio as pin  # type: ignore[import-untyped]

        js = self._rx.get_joint_state()
        if js is None:
            raise RuntimeError("无关节数据，无法计算末端位姿")

        q = pin.neutral(self._pin_model)
        # FK 使用逻辑坐标（原始固件坐标）
        q[:5] = js.position

        pin.forwardKinematics(self._pin_model, self._pin_data, q)
        pin.updateFramePlacements(self._pin_model, self._pin_data)

        placement = self._pin_data.oMf[self._end_eff_frame_id]
        pos = placement.translation
        quat = pin.Quaternion(placement.rotation)

        return EndEffectorPose(
            x=float(pos[0]),
            y=float(pos[1]),
            z=float(pos[2]),
            qx=float(quat.x),
            qy=float(quat.y),
            qz=float(quat.z),
            qw=float(quat.w),
        )

    def end_pose_ctrl(self, target: EndEffectorPose) -> None:
        """末端笛卡尔位姿控制（IK + 关节控制）。

        使用解析逆运动学求解器，传入当前关节状态用于就近原则优化。

        Args:
            target: 目标末端位姿.
        """
        js = self._rx.get_joint_state()
        q_init = list(js.position) if js is not None else None
        q = self._ik_solver.solve(target, q_init=q_init)
        if q is None:
            raise RuntimeError(f"IK 无解，目标不可达: {target}")
        self.joint_ctrl(q[:5])

    def close(self) -> None:
        """失能机械臂，关闭 CAN 通信，并停止守护线程."""
        self.enable_arm(False)
        self._rx.close()
        self._send_bus.shutdown()

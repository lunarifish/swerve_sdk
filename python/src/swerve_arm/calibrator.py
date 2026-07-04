class MotorCalibrator:
    """关节坐标标定器。

    实现用户坐标与逻辑坐标的双向转换。

        logical = user + offset
        user    = logical - offset

    """

    def __init__(self, n_joints: int = 5):
        self._offset: list[float] = [0.0] * n_joints

    def user_to_logical(self, positions: list[float]) -> list[float]:
        """用户坐标 -> 逻辑坐标。"""
        return [p + o for p, o in zip(positions, self._offset)]

    def logical_to_user(self, positions: list[float]) -> tuple[float, ...]:
        """逻辑坐标 -> 用户坐标。"""
        return tuple(p - o for p, o in zip(positions, self._offset))

    def set_zero(self, current_logical: list[float]) -> None:
        """将给定的逻辑坐标位置设为零点（可在当前偏移上叠加）。"""
        self._offset = [o + p for o, p in zip(self._offset, current_logical)]

    def clear(self) -> None:
        """清除所有零点偏移。"""
        for i in range(len(self._offset)):
            self._offset[i] = 0.0

    def set_offset(self, offsets: list[float]) -> None:
        """直接设置零点偏移量。"""
        if len(offsets) != len(self._offset):
            raise ValueError(
                f"需要 {len(self._offset)} 个偏移值，收到 {len(offsets)} 个"
            )
        self._offset = list(offsets)

    def get_offset(self) -> list[float]:
        """获取当前零点偏移量。"""
        return list(self._offset)


# TESTS
if __name__ == "__main__":
    import math

    def assert_close(a, b, msg=""):
        if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)):
            for i, (x, y) in enumerate(zip(a, b)):
                if not math.isclose(x, y):
                    raise AssertionError(f"{msg}[{i}]: {a!r} != {b!r}")
        else:
            raise AssertionError(f"{msg}: {a!r} != {b!r}")

    # 1. 默认偏移为零
    c = MotorCalibrator()
    assert_close(c.get_offset(), [0.0, 0.0, 0.0, 0.0, 0.0], "init offset")
    assert_close(c.user_to_logical([1, 2, 3, 4, 5]), [
                 1, 2, 3, 4, 5], "identity user->logical")
    assert_close(c.logical_to_user([1, 2, 3, 4, 5]),
                 (1, 2, 3, 4, 5), "identity logical->user")

    # 2. set_offset/get_offset
    c.set_offset([0.1, -0.2, 0.3, -0.4, 0.5])
    assert_close(c.get_offset(), [0.1, -0.2, 0.3, -0.4, 0.5], "set_offset")
    assert_close(c.user_to_logical([1, 1, 1, 1, 1]),
                 [1.1, 0.8, 1.3, 0.6, 1.5], "user->logical with offset")

    # 3. 往返一致性
    user_in = [1.5, -2.0, 3.14, 0.0, -1.57]
    logical = c.user_to_logical(user_in)
    user_out = c.logical_to_user(logical)
    assert_close(list(user_out), user_in, "round-trip")

    # 4. set_zero（当前偏移叠加）
    c.set_zero([0.5, 0.5, 0.5, 0.5, 0.5])
    assert_close(c.get_offset(), [0.6, 0.3, 0.8,
                 0.1, 1.0], "set_zero accumulate")

    # 5. clear
    c.clear()
    assert_close(c.get_offset(), [0.0, 0.0, 0.0, 0.0, 0.0], "clear")

    # 6. 非5轴
    c3 = MotorCalibrator(n_joints=3)
    assert_close(c3.get_offset(), [0.0, 0.0, 0.0])
    c3.set_offset([1, 2, 3])
    assert_close(c3.user_to_logical([10, 20, 30]), [11, 22, 33])

    try:
        c3.set_offset([1, 2])
        raise AssertionError("should have raised")
    except ValueError:
        pass

    print("All tests passed.")

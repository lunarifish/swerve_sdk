# Swerve机械臂SDK

> 注意`git clone`时要依次执行以下命令，获取额外的子模块和LFS资源：
>
> ```bash
> git clone <url>
> cd swerve_sdk
> git submodule update --init --recursive
> git lfs pull    # 如果没有安装 Git LFS，请先安装 Git LFS：https://git-lfs.com/
> ```

本仓库为Swerve机械臂项目的monorepo，包含调参上位机、下位机固件、Python SDK、ROS2&moveit驱动、离线参数辨识工具以及一些实用脚本：

- [`configurator/`](configurator/): USBCDC上位机源码

- [`stm32/`](stm32/): STM32 MCU固件

- [`python/`](python/): Python SDK

- [`ros2/`](ros2/): ROS2驱动和MoveIt配置

- [`utils/`](utils/)
  - [`dynamics/`](utils/dynamics/): 动力学工具
  - [`scripts/`](utils/scripts/): 脚本

各组件的文档参见对应目录下的`README.md`。

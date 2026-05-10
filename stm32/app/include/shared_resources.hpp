#pragma once

#include <librm.hpp>

#include "main.h"
#include "fdcan.h"
#include "usart.h"
#include "spi.h"

#include "bsp/buzzer.hpp"
#include "bsp/led.hpp"
#include "bsp/vbus_adc.hpp"
#include "et16s.hpp"
#include "configurator/schema.hpp"
#include "host/host.hpp"

/**
 * STM32H7的DTCM区域只能被CPU访问，不能被DMA访问，所以需要开一块在DTCM之外的全局数据区域，专门放一些需要被DMA访问的对象
 * 注意linker script里要相应的开一块放在RAM_D1里的区域sram4：
 *   .sram4 (NOLOAD) :
 * {
 *   . = ALIGN(32);
 *   *(.sram4)
 *   . = ALIGN(32);
 * } >RAM_D1
 */
struct SharedResourcesNoDtcm {
  rm::hal::Serial<50> sbus_serial{huart5, false, true};         ///< 接收遥控器的串口
  rm::hal::Serial<100> sensor_485_serial{huart3, false, true};  ///< 压力传感器485通信串口
  VbusAdc vbus_adc{hadc1};
};
extern __attribute__((section(".sram4"))) SharedResourcesNoDtcm shared_no_dtcm;

/**
 * @brief   板级全局共享资源
 */
struct SharedResources {
  SharedResourcesNoDtcm *no_dtcm{&shared_no_dtcm};  ///< DTCMRAM外的资源指针

  rm::hal::ThrottledCan<200> left_can{5500, hfdcan1}, right_can{5500, hfdcan2};
  rm::hal::ThrottledCan<100> comm_can{2500, hfdcan3};

  Buzzer buzzer{htim12, TIM_CHANNEL_2};  ///< 蜂鸣器
  rm::modules::BuzzerController<
      rm::modules::buzzer_melody::Silent, rm::modules::buzzer_melody::Beeps<1>, rm::modules::buzzer_melody::Beeps<2>,
      rm::modules::buzzer_melody::Startup, rm::modules::buzzer_melody::Error, rm::modules::buzzer_melody::Success,
      rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kC6>,
      rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kD6>,
      rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kE6>,
      rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kF6>, buzzer_melody::EnterManualMode,
      buzzer_melody::HiBeeps<1>, buzzer_melody::HiBeeps<2>, buzzer_melody::HiBeeps<3>, buzzer_melody::HiBeeps<4>,
      buzzer_melody::HiBeeps<5>, buzzer_melody::HiBeeps<6>>
      buzzer_controller;
  WS2812Led led{hspi6};                                                 ///< WS2812 LED灯
  rm::modules::RgbLedController<rm::modules::led_pattern::Off,          //
                                rm::modules::led_pattern::GreenBreath,  //
                                rm::modules::led_pattern::RedFlash,     //
                                led_pattern::YellowFlash,               //
                                rm::modules::led_pattern::RgbFlow>
      led_controller;  ///< LED控制器

  rm::device::BMI088 onboard_imu{hspi2, ACC_CS_GPIO_Port, ACC_CS_Pin, GYRO_CS_GPIO_Port, GYRO_CS_Pin};
  rm::modules::MahonyAhrs mahony_ahrs{1000.f};  ///< AHRS算法

  WflyET16s rc{shared_no_dtcm.sbus_serial};     ///< 遥控器
  rm::device::DeviceManager<5> device_manager;  ///< 设备管理器，用于监测各个设备的在线状态
  Host host{comm_can, 0x233};

  rm::device::Znsv6T1 press_sensor{no_dtcm->sensor_485_serial, 254};

  etl::string<512> system_status;  ///< 系统状态摘要字符串

  float vbus;

  /**
   * @brief 获取单例实例
   */
  static SharedResources &GetInstance() {
    static SharedResources *shared_resources_instance{nullptr};
    if (shared_resources_instance == nullptr) {
      shared_resources_instance = new SharedResources;
      shared_resources_instance->Init();
    }
    return *shared_resources_instance;
  }

  /**
   * @brief 初始化所有全局对象
   */
  void Init() {
    rc.SetName("remote");
    device_manager << &rc;

    device_manager.OnDeviceStatusChange([&](rm::device::Device *dev) {
      // if (dev->name() == "remote") {
      //   if (dev->online_status() == rm::device::Device::kOk) {
      //     buzzer_controller.Play<rm::modules::buzzer_melody::Success>();
      //   } else if (dev->online_status() != rm::device::Device::kOk) {
      //     buzzer_controller.Play<rm::modules::buzzer_melody::Error>();
      //   }
      // }
    });

    buzzer.Begin();
    left_can.SetFilter(0, 0);
    right_can.SetFilter(0, 0);
    comm_can.SetFilter(0, 0);
    left_can.Begin();
    right_can.Begin();
    comm_can.Begin();
    rc.Begin();
    // no_dtcm->sensor_485_serial.Start();
    no_dtcm->vbus_adc.Begin();
  }
};

/**
 * 参数类型定义 —— 可扩展的参数 Schema
 *
 * 新增参数只需添加条目到 JOINT_SCHEMAS，组件自动渲染。
 * 新增参数类型只需扩展 ParamType 并在 JointCard 中加对应分支。
 *
 * 参数定义对齐 MCU 固件 PersistableConfigV1 (config_schema.hpp):
 *   - pd_kp[5] / pd_kd[5] / max_joint_vel[5] / max_joint_accel[5]
 */

// ---- 基础类型 ----

/** 参数控件类型 */
export type ParamType = 'spinbox' | 'toggle';

/** 单个参数的描述 */
export interface ParamDef {
  /** 协议中使用的 key，与 MCU PersistableConfigV1 字段对应 */
  key: string;
  /** UI 显示名称 */
  label: string;
  /** 控件类型 */
  type: ParamType;
  /** 最小值 */
  min?: number;
  /** 最大值 */
  max?: number;
  /** 步进 */
  step?: number;
  /** 单位（可选），如 "rad/s"、"rad/s²" */
  unit?: string;
  /** 默认值 */
  defaultValue: number;
}

/** 一个关节的完整参数 Schema */
export interface JointSchema {
  jointName: string;
  params: ParamDef[];
}

/**
 * 5 关节机械臂参数定义
 *
 * 关节名来自 URDF: J1, J2, J3, J4, J5
 *
 * 默认值严格对应 MCU PersistableConfigV1 初始化值：
 *   pd_kp  = {25.f, 40.f, 40.f, 13.5f, 9.f}
 *   pd_kd  = {5.f, 10.f, 10.f, 3.f, 0.5f}
 *   max_joint_vel  / max_joint_accel = {} (零初始化)
 */
export const JOINT_SCHEMAS: JointSchema[] = [
  {
    jointName: 'J1',
    params: [
      { key: 'pd_kp', label: 'Kp', type: 'spinbox', min: 0, max: 100, step: 0.1, defaultValue: 0 },
      { key: 'pd_kd', label: 'Kd', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0 },
      { key: 'max_joint_vel', label: '最大速度', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0, unit: 'rad/s' },
      { key: 'max_joint_accel', label: '最大加速度', type: 'spinbox', min: 0, max: 100, step: 0.01, defaultValue: 0, unit: 'rad/s²' },
    ],
  },
  {
    jointName: 'J2',
    params: [
      { key: 'pd_kp', label: 'Kp', type: 'spinbox', min: 0, max: 100, step: 0.1, defaultValue: 0 },
      { key: 'pd_kd', label: 'Kd', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0 },
      { key: 'max_joint_vel', label: '最大速度', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0, unit: 'rad/s' },
      { key: 'max_joint_accel', label: '最大加速度', type: 'spinbox', min: 0, max: 100, step: 0.01, defaultValue: 0, unit: 'rad/s²' },
    ],
  },
  {
    jointName: 'J3',
    params: [
      { key: 'pd_kp', label: 'Kp', type: 'spinbox', min: 0, max: 100, step: 0.1, defaultValue: 0 },
      { key: 'pd_kd', label: 'Kd', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0 },
      { key: 'max_joint_vel', label: '最大速度', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0, unit: 'rad/s' },
      { key: 'max_joint_accel', label: '最大加速度', type: 'spinbox', min: 0, max: 100, step: 0.01, defaultValue: 0, unit: 'rad/s²' },
    ],
  },
  {
    jointName: 'J4',
    params: [
      { key: 'pd_kp', label: 'Kp', type: 'spinbox', min: 0, max: 100, step: 0.1, defaultValue: 0 },
      { key: 'pd_kd', label: 'Kd', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0 },
      { key: 'max_joint_vel', label: '最大速度', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0, unit: 'rad/s' },
      { key: 'max_joint_accel', label: '最大加速度', type: 'spinbox', min: 0, max: 100, step: 0.01, defaultValue: 0, unit: 'rad/s²' },
    ],
  },
  {
    jointName: 'J5',
    params: [
      { key: 'pd_kp', label: 'Kp', type: 'spinbox', min: 0, max: 100, step: 0.1, defaultValue: 0 },
      { key: 'pd_kd', label: 'Kd', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0 },
      { key: 'max_joint_vel', label: '最大速度', type: 'spinbox', min: 0, max: 30, step: 0.01, defaultValue: 0, unit: 'rad/s' },
      { key: 'max_joint_accel', label: '最大加速度', type: 'spinbox', min: 0, max: 100, step: 0.01, defaultValue: 0, unit: 'rad/s²' },
    ],
  },
];

/** 根据 Schema 初始化所有关节参数的默认值 Map */
export function buildDefaultValues(schemas: JointSchema[]): Map<string, Map<string, number>> {
  const result = new Map<string, Map<string, number>>();
  for (const joint of schemas) {
    const paramMap = new Map<string, number>();
    for (const param of joint.params) {
      paramMap.set(param.key, param.defaultValue);
    }
    result.set(joint.jointName, paramMap);
  }
  return result;
}

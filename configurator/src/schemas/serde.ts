/**
 * 序列化/反序列化工具
 *
 * 依赖: protocol.ts (类型定义) + crc.ts (CRC 计算)
 * 使用者: useSerial.ts
 */

import { crc8, crc16, CRC8_INIT, CRC16_INIT } from './crc';
import {
  type PersistableConfigV1,
  PROTOCOL_VERSION,
  CONFIG_SERIALIZED_LEN,
  HEADER_LEN,
  CMD_ID_LEN,
  CRC16_LEN,
  SOF,
} from './protocol';

// ═══════════════════════════════════════════════════════════════
// PersistableConfigV1 序列化 / 反序列化
// ═══════════════════════════════════════════════════════════════

/**
 * 将 PersistableConfigV1 序列化为 84 字节 Uint8Array (LE)
 *
 * 内存布局 (packed):
 *   [0..3]   schema_version (uint32 LE)
 *   [4..23]  pd_kp[5]  (5 × float32 LE)
 *   [24..43] pd_kd[5]  (5 × float32 LE)
 *   [44..63] max_joint_vel[5]  (5 × float32 LE)
 *   [64..83] max_joint_accel[5] (5 × float32 LE)
 */
export function serializeConfig(config: PersistableConfigV1): Uint8Array {
  const buf = new ArrayBuffer(CONFIG_SERIALIZED_LEN);
  const dv = new DataView(buf);
  let offset = 0;

  dv.setUint32(offset, config.schemaVersion, true);
  offset += 4;

  const arrays: Float32Array[] = [config.pdKp, config.pdKd, config.maxJointVel, config.maxJointAccel];
  for (const arr of arrays) {
    for (let i = 0; i < 5; i++) {
      dv.setFloat32(offset, arr[i] ?? 0, true);
      offset += 4;
    }
  }

  return new Uint8Array(buf);
}

/**
 * 从 84 字节 payload 解析 PersistableConfigV1
 */
export function parseConfig(data: Uint8Array): PersistableConfigV1 {
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  let offset = 0;

  const schemaVersion = dv.getUint32(offset, true);
  offset += 4;

  const readFloat32x5 = (): Float32Array => {
    const arr = new Float32Array(5);
    for (let i = 0; i < 5; i++) {
      arr[i] = dv.getFloat32(offset, true);
      offset += 4;
    }
    return arr;
  };

  return {
    schemaVersion,
    pdKp: readFloat32x5(),
    pdKd: readFloat32x5(),
    maxJointVel: readFloat32x5(),
    maxJointAccel: readFloat32x5(),
  };
}

// ═══════════════════════════════════════════════════════════════
// 帧构建
// ═══════════════════════════════════════════════════════════════

/**
 * 按 configurator_schema::CmdFrame 格式构建完整帧
 *
 * 帧布局:
 *   [0]       SOF (0xA5)
 *   [1..2]    payload_len (uint16 LE)
 *   [3]       seq
 *   [4]       header_crc8 (校验 [0..3])
 *   [5..6]    cmd_id (uint16 LE)
 *   [7..N]    payload[]
 *   [N+1..N+2] crc16 (校验 [0..N-1])
 *
 * @param cmdId   命令码 (0x100 / 0x102 / 0x200)
 * @param payload 载荷数据
 * @param seq     包序号
 */
export function buildCmdFrame(cmdId: number, payload: Uint8Array, seq: number): Uint8Array {
  const totalLen = HEADER_LEN + CMD_ID_LEN + payload.length + CRC16_LEN;
  const frame = new Uint8Array(totalLen);

  // [0] SOF
  frame[0] = SOF;

  // [1..2] payload_len (LE)
  frame[1] = payload.length & 0xff;
  frame[2] = (payload.length >> 8) & 0xff;

  // [3] seq
  frame[3] = seq & 0xff;

  // [4] header_crc8 — 校验前 4 字节
  frame[4] = crc8(frame.subarray(0, 4), CRC8_INIT);

  // [5..6] cmd_id (LE)
  frame[5] = cmdId & 0xff;
  frame[6] = (cmdId >> 8) & 0xff;

  // [7..] payload
  frame.set(payload, HEADER_LEN + CMD_ID_LEN);

  // crc16 — 校验从 SOF 到 payload 末尾
  const crcVal = crc16(frame.subarray(0, totalLen - CRC16_LEN), CRC16_INIT);
  frame[totalLen - 2] = crcVal & 0xff;
  frame[totalLen - 1] = (crcVal >> 8) & 0xff;

  return frame;
}

// ═══════════════════════════════════════════════════════════════
// UI Map ↔ Config 互转
// ═══════════════════════════════════════════════════════════════

/** 默认关节顺序 */
export const JOINT_ORDER = ['J1', 'J2', 'J3', 'J4', 'J5'] as const;

/**
 * 从 UI 的 allValues Map 构建 PersistableConfigV1 对象
 */
export function buildConfigFromMap(
  allValues: Map<string, Map<string, number>>,
  jointOrder: readonly string[] = JOINT_ORDER,
): PersistableConfigV1 {
  const pdKp = new Float32Array(5);
  const pdKd = new Float32Array(5);
  const maxJointVel = new Float32Array(5);
  const maxJointAccel = new Float32Array(5);

  for (let i = 0; i < jointOrder.length; i++) {
    const params = allValues.get(jointOrder[i]);
    if (params) {
      pdKp[i] = params.get('pd_kp') ?? 0;
      pdKd[i] = params.get('pd_kd') ?? 0;
      maxJointVel[i] = params.get('max_joint_vel') ?? 0;
      maxJointAccel[i] = params.get('max_joint_accel') ?? 0;
    }
  }

  return {
    schemaVersion: PROTOCOL_VERSION,
    pdKp,
    pdKd,
    maxJointVel,
    maxJointAccel,
  };
}

/**
 * 将解析出的 PersistableConfigV1 转为 UI 使用的 allValues Map
 */
export function configToMap(
  config: PersistableConfigV1,
  jointOrder: readonly string[] = JOINT_ORDER,
): Map<string, Map<string, number>> {
  const result = new Map<string, Map<string, number>>();

  for (let i = 0; i < jointOrder.length; i++) {
    const m = new Map<string, number>();
    m.set('pd_kp', config.pdKp[i]);
    m.set('pd_kd', config.pdKd[i]);
    m.set('max_joint_vel', config.maxJointVel[i]);
    m.set('max_joint_accel', config.maxJointAccel[i]);
    result.set(jointOrder[i], m);
  }

  return result;
}

/**
 * ConfiguratorParser — 上位机配置协议字节流分包器 (TypeScript 实现)
 *
 * 与 C++ ConfiguratorParser::operator<< 的 FSM 完全一致。
 *
 * 帧格式:
 *   [SOF:1] [payload_len:2 LE] [seq:1] [header_crc8:1] [cmd_id:2 LE] [payload:N] [crc16:2 LE]
 *
 * Usage:
 * @code
 *   const parser = new ConfiguratorParser();
 *   parser.onFrame((cmdId, payload, seq) => {
 *     if (cmdId === CmdId.ALL_CONFIG) { ... }
 *   });
 *   // 逐字节喂入
 *   while (true) {
 *     const byte = await readByte();
 *     parser.feed(byte);
 *   }
 * @endcode
 */

import { crc8, crc16, CRC8_INIT, CRC16_INIT } from './crc';
import { SOF, HEADER_LEN, CMD_ID_LEN, CRC16_LEN, MAX_PAYLOAD_LEN, MAX_FRAME_LEN } from './protocol';

// ============ FSM 状态常量 (与 C++ State 一一对应) ============

const State = {
  /** 等待 0xA5 */
  SOF: 0,
  /** payload_len 低字节 */
  LEN_LSB: 1,
  /** payload_len 高字节 + 长度校验 */
  LEN_MSB: 2,
  /** 包序号 */
  SEQ: 3,
  /** header CRC8 校验 */
  CRC8: 4,
  /** cmd_id 低字节 */
  CMD_ID_LSB: 5,
  /** cmd_id 高字节 */
  CMD_ID_MSB: 6,
  /** 吸收 payload + crc16 → 校验 → 分发 */
  PAYLOAD: 7,
} as const;

/** 帧回调类型 */
export type FrameCallback = (cmdId: number, payload: Uint8Array, seq: number) => void;

/**
 * 串口字节流 FSM 解析器
 */
export class ConfiguratorParser {
  /** 注册帧接收回调 */
  onFrame(cb: FrameCallback): void {
    this.callbacks_.push(cb);
  }

  /** 移除指定回调 */
  offFrame(cb: FrameCallback): void {
    const idx = this.callbacks_.indexOf(cb);
    if (idx !== -1) {
      this.callbacks_.splice(idx, 1);
    }
  }

  /** 移除所有回调 */
  clearCallbacks(): void {
    this.callbacks_.length = 0;
  }

  /** CRC 校验错误累计次数 */
  get errorCount(): number {
    return this.errorCount_;
  }

  /** 重置错误计数 */
  resetErrorCount(): void {
    this.errorCount_ = 0;
  }

  /**
   * 逐字节喂入数据，内部 FSM 自动完成分包与校验
   */
  feed(byte: number): void {
    const data = byte & 0xff;

    switch (this.state_) {
      case State.SOF: {
        if (data === SOF) {
          this.buf_[0] = data;
          this.bufIdx_ = 1;
          this.state_ = State.LEN_LSB;
        }
        break;
      }

      case State.LEN_LSB: {
        this.buf_[this.bufIdx_++] = data;
        this.state_ = State.LEN_MSB;
        break;
      }

      case State.LEN_MSB: {
        this.buf_[this.bufIdx_++] = data;
        this.payloadLen_ = this.buf_[1] | (this.buf_[2] << 8);
        if (this.payloadLen_ <= MAX_PAYLOAD_LEN) {
          this.state_ = State.SEQ;
        } else {
          this.errorCount_++;
          this.reset();
        }
        break;
      }

      case State.SEQ: {
        this.buf_[this.bufIdx_++] = data;
        this.state_ = State.CRC8;
        break;
      }

      case State.CRC8: {
        this.buf_[this.bufIdx_++] = data;
        if (
          crc8(new Uint8Array(this.buf_.subarray(0, HEADER_LEN - 1)), CRC8_INIT) === data
        ) {
          this.state_ = State.CMD_ID_LSB;
        } else {
          this.errorCount_++;
          this.reset();
        }
        break;
      }

      case State.CMD_ID_LSB: {
        this.buf_[this.bufIdx_++] = data;
        this.state_ = State.CMD_ID_MSB;
        break;
      }

      case State.CMD_ID_MSB: {
        this.buf_[this.bufIdx_++] = data;
        this.state_ = State.PAYLOAD;
        // fallthrough to PAYLOAD — 当前字节已在 buf_ 中
        this.feedPayload();
        break;
      }

      case State.PAYLOAD: {
        this.buf_[this.bufIdx_++] = data;
        this.feedPayload();
        break;
      }
    }
  }

  private feedPayload(): void {
    const totalLen = HEADER_LEN + CMD_ID_LEN + this.payloadLen_ + CRC16_LEN;
    if (this.bufIdx_ >= totalLen) {
      const receivedCrc =
        this.buf_[totalLen - 2] | (this.buf_[totalLen - 1] << 8);
      const computedCrc = crc16(
        new Uint8Array(this.buf_.subarray(0, totalLen - CRC16_LEN)),
        CRC16_INIT,
      );

      if (receivedCrc === computedCrc) {
        const cmdId = this.buf_[5] | (this.buf_[6] << 8);
        const seq = this.buf_[3];
        const payload = new Uint8Array(
          this.buf_.subarray(HEADER_LEN + CMD_ID_LEN, HEADER_LEN + CMD_ID_LEN + this.payloadLen_),
        );

        for (const cb of this.callbacks_) {
          cb(cmdId, payload, seq);
        }
      } else {
        this.errorCount_++;
      }
      this.reset();
    }
  }

  /** 原子重置 FSM */
  private reset(): void {
    this.state_ = State.SOF;
    this.bufIdx_ = 0;
  }

  private state_: number = State.SOF;
  private buf_: Uint8Array = new Uint8Array(MAX_FRAME_LEN);
  private bufIdx_ = 0;
  private payloadLen_ = 0;
  private callbacks_: FrameCallback[] = [];
  private errorCount_ = 0;
}

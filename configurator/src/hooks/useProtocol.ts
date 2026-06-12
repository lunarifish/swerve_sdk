import { useState, useCallback, useEffect, useRef } from 'react';
import { useSerial, LogLevel, type LogEntry } from './useSerial';
import { CmdId, ReadRequestType } from '../schemas/protocol';
import {
  buildCmdFrame,
  serializeConfig,
  parseConfig,
  buildConfigFromMap,
  configToMap,
} from '../schemas/serde';

/**
 * 协议层 Hook
 *
 * 职责: 帧收发、LOG_STRING 解析、mcuLogs/hostLogs、commit 校验、读写参数。
 * 依赖 useSerial 提供的纯传输层 (writeRaw / parser / status)。
 */
export function useProtocol(serial: ReturnType<typeof useSerial>) {
  const seqRef = useRef<number>(0);
  const [parserErrors, setParserErrors] = useState(0);

  // ---- 文本日志 (MCU LogString + Host) ----
  const [mcuLogs, setMcuLogs] = useState<LogEntry[]>([]);
  const [hostLogs, setHostLogs] = useState<LogEntry[]>([]);

  /** 添加宿主侧日志 */
  const addHostLog = useCallback((level: LogLevel, text: string) => {
    setHostLogs((prev) => [...prev.slice(-255), { ts: Date.now(), level, source: 'host', text }]);
  }, []);

  // ---- 注册 MCU 日志回调 (连接后持续生效) ----
  useEffect(() => {
    if (serial.status !== 'connected') return;
    const parser = serial.parser.current;

    const onLogString = (cmdId: number, payload: Uint8Array, _seq: number) => {
      if (cmdId !== CmdId.LOG_STRING) return;
      let level: LogLevel = LogLevel.Info;
      if (payload.length > 0 && payload[0] <= 3) {
        level = payload[0] as LogLevel;
      }
      const text = new TextDecoder()
        .decode(payload.subarray(payload[0] <= 3 ? 1 : 0))
        .replace(/\0.*$/, '');
      if (text.length > 0) {
        setMcuLogs((prev) => [...prev.slice(-255), { ts: Date.now(), level, source: 'mcu', text }]);
      }
    };
    parser.onFrame(onLogString);

    return () => {
      parser.offFrame(onLogString);
      setParserErrors(0);
    };
  }, [serial.status]);

  // ---- 连接 (含 commit 校验) ----
  const connect = useCallback(async (): Promise<boolean> => {
    const ok = await serial.connect();
    if (!ok) return false;

    // 校验 MCU 固件 commit ID
    const parser = serial.parser.current;
    const mcuCommit = await new Promise<string | null>((resolve) => {
      const timeout = setTimeout(() => resolve(null), 2000);
      const onCommit = (_cmdId: number, payload: Uint8Array, _s: number) => {
        clearTimeout(timeout);
        parser.offFrame(onCommit);
        if (payload.length >= 7) {
          const hash = new TextDecoder().decode(payload.subarray(0, 7)).replace(/\0.*$/, '');
          resolve(hash || null);
        } else {
          resolve(null);
        }
      };
      parser.onFrame(onCommit);

      const seq = seqRef.current++;
      const frame = buildCmdFrame(CmdId.READ_REQUEST, new Uint8Array([ReadRequestType.FirmwareCommit]), seq);
      serial.writeRaw(frame).catch(() => {
        clearTimeout(timeout);
        parser.offFrame(onCommit);
        resolve(null);
      });
    });

    if (mcuCommit === null) {
      addHostLog(LogLevel.Error, '无法获取 MCU 固件版本 (超时)，拒绝连接');
      await serial.disconnect();
      return false;
    }
    if (mcuCommit !== __COMMIT_ID__) {
      addHostLog(LogLevel.Error, `固件版本不匹配: MCU=${mcuCommit} App=${__COMMIT_ID__}，拒绝连接`);
      await serial.disconnect();
      return false;
    }
    addHostLog(LogLevel.Info, `固件版本一致: ${mcuCommit}`);
    return true;
  }, [serial]);

  // ---- 同步 parser 错误计数 ----
  useEffect(() => {
    const interval = setInterval(() => {
      setParserErrors(serial.parser.current.errorCount);
    }, 500);
    return () => clearInterval(interval);
  }, [serial.parser]);

  /** 发送组好的帧 */
  const sendFrame = useCallback(
    async (cmdId: number, payload: Uint8Array): Promise<void> => {
      const seq = seqRef.current++;
      const frame = buildCmdFrame(cmdId, payload, seq);
      await serial.writeRaw(frame);
    },
    [serial.writeRaw],
  );

  /** 批量发送全部关节参数并等待 MCU ACK */
  const sendAllParams = useCallback(
    async (allValues: Map<string, Map<string, number>>): Promise<boolean> => {
      const parser = serial.parser.current;

      return new Promise<boolean>((resolve) => {
        const timeout = setTimeout(() => {
          parser.offFrame(handler);
          resolve(false);
        }, 2000);

        const handler = (cmdId: number, payload: Uint8Array, _seq: number) => {
          if (cmdId !== CmdId.CONFIG_WRITE_ACK) return;
          clearTimeout(timeout);
          parser.offFrame(handler);
          const ok = payload.length > 0 && payload[0] !== 0;
          if (ok) {
            resolve(true);
          } else {
            const reason = new TextDecoder()
              .decode(payload.subarray(1))
              .replace(/\0.*$/, '')
              || 'MCU rejected';
            addHostLog(LogLevel.Error, `参数写入失败: ${reason}`);
            resolve(false);
          }
        };

        parser.onFrame(handler);

        const config = buildConfigFromMap(allValues);
        const payload = serializeConfig(config);
        sendFrame(CmdId.ALL_CONFIG, payload).catch(() => {
          clearTimeout(timeout);
          parser.offFrame(handler);
          resolve(false);
        });
      });
    },
    [serial.parser, sendFrame, addHostLog],
  );

  /** 向 MCU 发送读指令并等待响应 */
  const readAllParams = useCallback(async (): Promise<Map<string, Map<string, number>> | null> => {
    const parser = serial.parser.current;

    return new Promise((resolve) => {
      const timeout = setTimeout(() => {
        parser.offFrame(handler);
        resolve(null);
      }, 2000);

      const handler = (cmdId: number, payload: Uint8Array, _seq: number) => {
        if (cmdId !== CmdId.ALL_CONFIG) return;
        clearTimeout(timeout);
        parser.offFrame(handler);
        try {
          const config = parseConfig(payload);
          resolve(configToMap(config));
        } catch (e) {
          console.error('解析配置失败:', e);
          resolve(null);
        }
      };

      parser.onFrame(handler);
      sendFrame(CmdId.READ_REQUEST, new Uint8Array([ReadRequestType.AllConfig])).catch(() => {
        clearTimeout(timeout);
        parser.offFrame(handler);
        resolve(null);
      });
    });
  }, [serial.parser, sendFrame]);

  /** 断开时清空日志 */
  const disconnect = useCallback(async () => {
    await serial.disconnect();
    seqRef.current = 0;
    setParserErrors(0);
  }, [serial.disconnect]);

  /** 发送重启 MCU 命令 (空 payload) */
  const reboot = useCallback(async () => {
    await sendFrame(CmdId.REBOOT_REQUEST, new Uint8Array(0));
  }, [sendFrame]);

  return {
    status: serial.status,
    connect,
    disconnect,
    parserErrors,
    mcuLogs,
    hostLogs,
    addHostLog,
    sendAllParams,
    readAllParams,
    reboot,
  };
}

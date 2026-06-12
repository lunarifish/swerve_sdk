import { useState, useCallback, useRef } from 'react';
import { ConfiguratorParser } from '../schemas/parser';

/** 串口连接状态 */
export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected';

/** 一条字节流记录 */
export interface ByteLogEntry {
  ts: number;
  data: Uint8Array;
}

/** 日志级别 (对应 MCU LogString::LogLevel) */
export const LogLevel = {
  Debug: 0,
  Info: 1,
  Warning: 2,
  Error: 3,
} as const;
export type LogLevel = (typeof LogLevel)[keyof typeof LogLevel];

/** 一条文本日志 */
export interface LogEntry {
  ts: number;
  level: LogLevel;
  source: 'mcu' | 'host';
  text: string;
}

/** 串口固定参数 */
const SERIAL_OPTIONS: SerialOptions = {
  baudRate: 921600,
  dataBits: 8,
  stopBits: 1,
  parity: 'none',
  flowControl: 'none',
};

/**
 * Web Serial API 纯串口传输 hook
 *
 * 职责: 打开/关闭串口、TX/RX 字节流日志、将接收数据喂入协议解析器。
 * 不包含任何业务协议逻辑 (帧构建/解析/commit 校验等)。
 */
export function useSerial() {
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const portRef = useRef<SerialPort | null>(null);
  const writerRef = useRef<WritableStreamDefaultWriter<Uint8Array> | null>(null);
  const readerRef = useRef<ReadableStreamDefaultReader<Uint8Array> | null>(null);
  const readLoopAbortRef = useRef<AbortController | null>(null);

  /** 协议解析器实例 — 外部通过 parser 属性注册回调、获取错误计数 */
  const parserRef = useRef<ConfiguratorParser>(new ConfiguratorParser());

  // ---- TX/RX 字节流日志 ----
  const [txLog, setTxLog] = useState<ByteLogEntry[]>([]);
  const [rxLog, setRxLog] = useState<ByteLogEntry[]>([]);
  const [txBytes, setTxBytes] = useState(0);
  const [rxBytes, setRxBytes] = useState(0);

  /** 写入原始数据并记录 TX 日志 */
  const writeRaw = useCallback(async (data: Uint8Array) => {
    if (!writerRef.current) {
      console.error('串口未连接');
      return;
    }
    await writerRef.current.write(data);
    setTxLog((prev) => [...prev.slice(-255), { ts: Date.now(), data }]);
    setTxBytes((n) => n + data.length);
  }, []);

  /** 背景读取循环 — 将每个接收字节喂入协议解析器 */
  const startReadLoop = useCallback((port: SerialPort) => {
    const abort = new AbortController();
    readLoopAbortRef.current = abort;
    const parser = parserRef.current;

    (async () => {
      while (port.readable && !abort.signal.aborted) {
        try {
          const reader = port.readable.getReader();
          readerRef.current = reader;
          while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            if (value && value.length > 0) {
              setRxLog((prev) => [...prev.slice(-255), { ts: Date.now(), data: value }]);
              setRxBytes((n) => n + value.length);
              for (let i = 0; i < value.length; i++) {
                parser.feed(value[i]);
              }
            }
          }
          reader.releaseLock();
        } catch {
          break;
        }
      }
    })();
  }, []);

  /** 连接串口: 弹出浏览器串口选择器，选中后直接连接 */
  const connect = useCallback(async (): Promise<boolean> => {
    let port: SerialPort;
    try {
      port = await navigator.serial.requestPort();
    } catch {
      return false;
    }
    portRef.current = port;
    setStatus('connecting');
    try {
      await port.open(SERIAL_OPTIONS);
      const writer = port.writable!.getWriter();
      writerRef.current = writer;

      startReadLoop(port);

      port.addEventListener('disconnect', () => {
        readLoopAbortRef.current?.abort();
        readLoopAbortRef.current = null;
        setStatus('disconnected');
        portRef.current = null;
        writerRef.current = null;
        readerRef.current = null;
      });

      setStatus('connected');
      return true;
    } catch (e) {
      console.error('连接失败:', e);
      setStatus('disconnected');
      return false;
    }
  }, [startReadLoop]);

  /** 断开串口并清空日志 */
  const disconnect = useCallback(async () => {
    try {
      readLoopAbortRef.current?.abort();
      readLoopAbortRef.current = null;
      if (readerRef.current) {
        try { readerRef.current.releaseLock(); } catch {}
        readerRef.current = null;
      }
      if (writerRef.current) {
        await writerRef.current.close();
        writerRef.current = null;
      }
      if (portRef.current) {
        await portRef.current.close();
        portRef.current = null;
      }
    } catch (e) {
      console.error('断开失败:', e);
    }
    setStatus('disconnected');
    setTxBytes(0);
    setRxBytes(0);
  }, []);

  return {
    status,
    connect,
    disconnect,
    /** 协议解析器 (供协议层注册 onFrame / offFrame 回调) */
    parser: parserRef,
    /** 写入原始字节 (供协议层发送组好的帧) */
    writeRaw,
    txLog,
    rxLog,
    txBytes,
    rxBytes,
  };
}

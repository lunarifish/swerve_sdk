# Swerve Configurator

Web-based parameter configurator for a 5-joint robotic arm, communicating with STM32 MCU via serial port.

## Tech Stack

- **React 19** + **TypeScript 6** + **Chakra UI v3** (custom theme)
- **Vite 8** for dev/build
- **Web Serial API** — Chromium-based browsers only (Chrome/Edge/Opera)

## Quick Start

```bash
cd configurator
npm install
npm run dev      # → http://localhost:5173
npm run build    # production build → dist/
```

> **WSL2 用户**：`vite.config.ts` 已开启 `server.watch.usePolling: true`，否则文件变更不会被热更新检测到。

## Project Structure

```
src/
├── main.tsx                   # Entry: wraps App in ChakraProvider with custom theme
├── App.tsx                    # Root: Splitter layout (params ↑ / bottom panel ↓), header buttons
├── theme.ts                   # Chakra v3 system theme — brand blue palette
├── vite-env.d.ts              # declare __COMMIT_ID__ (build-time git hash injection)
├── components/
│   ├── BottomPanel.tsx         # Tabs(Serial Monitor | Log Console) in splitter bottom pane
│   ├── ConnectionBar.tsx       # Connect/Disconnect + status badge (no dropdown)
│   ├── JointCard.tsx           # One card per joint, colored accent bar per joint
│   ├── LogDisplay.tsx          # Text log viewer: merged MCU LOG_STRING + host events
│   ├── ParamSpinbox.tsx        # NumberInput spinbox (no slider) for one parameter
│   ├── SerialTerminal.tsx      # Hex dump viewer: TX (blue) / RX (green) columns + error count
│   ├── VersionInfo.tsx         # App commit id + schema version (grey subtitle)
│   └── ui/
│       └── toaster.tsx         # Toast notification system
├── hooks/
│   ├── useSerial.ts            # ★ Pure transport: Web Serial open/close/read-loop/TX-RX logging
│   └── useProtocol.ts          # ★ Protocol layer: frame send/recv, commit-verify, ACK, reboot
└── schemas/
    ├── protocol.ts             # ★ Protocol definition — single source of truth (CmdId, payloads)
    ├── serde.ts                # Serialization: buildCmdFrame, serializeConfig, parseConfig, Map↔Config
    ├── crc.ts                  # CRC8/CRC16 lookup tables (256 entries each) matching librm
    ├── parser.ts               # Byte-stream FSM parser (ConfiguratorParser: SOF→LEN→SEQ→CRC8→CMD→PAYLOAD)
    └── params.ts               # Joint/param UI schema (J1–J5, 4 params each), buildDefaultValues()
```

## Architecture: Data Flow

```
App.tsx  (owner of allValues: Map<jointName, Map<paramKey, value>>)
  │
  ├── useProtocol(serial) ──────► frame send/recv, commit verify, ACK handling
  │     └── useSerial() ────────► Web Serial API (921600 8N1), read loop, TX/RX byte logs
  │
  ├── Header
  │   ├── VersionInfo ──────────► __COMMIT_ID__ + PROTOCOL_VERSION
  │   ├── ConnectionBar ────────► protocol.connect() / protocol.disconnect()
  │   └── Buttons ──────────────► readAllParams / sendAllParams / reboot (Popover confirm)
  │
  ├── SimpleGrid of JointCard[]
  │     └── ParamSpinbox[] ───── onChange → handleParamChange (NumberInput, no slider)
  │
  └── Splitter (vertical 75/25)
        └── BottomPanel
              ├── Tab: Serial Monitor → <SerialTerminal txLog/rxLog/parserErrors>
              └── Tab: Log Console   → <LogDisplay mcuLogs+hostLogs>
```

**State is schema-driven**: `JOINT_SCHEMAS` in `schemas/params.ts` defines joints and their params. The UI renders automatically from it. Adding a param = adding one entry to the schema array.

## Key Types

```typescript
type ConnectionStatus = 'disconnected' | 'connecting' | 'connected'

// ---- 参数定义 (params.ts) ----
interface ParamDef {
  key: string; label: string; type: 'number';
  min?: number; max?: number; step?: number; unit?: string;
  defaultValue: number;
}
interface JointSchema {
  jointName: string;     // "J1"–"J5"
  params: ParamDef[];    // each joint has 4 params: pd_kp, pd_kd, max_joint_vel, max_joint_accel
}

// ---- 传输层日志 (useSerial.ts) ----
interface ByteLogEntry { ts: number; data: Uint8Array; }

// ---- 文本日志 (useSerial.ts / useProtocol.ts) ----
enum LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 }
interface LogEntry {
  ts: number; level: LogLevel; source: 'mcu' | 'host'; text: string;
}
```

## Serial Protocol — Full Implementation

### 帧格式 (对应 `configurator_schema::CmdFrame<PayloadType>`)

```
┌──────┬─────────────┬──────┬───────────┬──────────┬───────────┬──────────┐
│ SOF  │ payload_len │ seq  │ hdr_crc8  │ cmd_id   │ payload   │ crc16    │
│ 0xA5 │ 2B LE       │ 1B   │ 1B        │ 2B LE    │ N B       │ 2B LE   │
└──────┴─────────────┴──────┴───────────┴──────────┴───────────┴──────────┘
```

- **SOF**: `0xA5`
- **payload_len**: 小端序 2 字节，不含帧头/帧尾
- **seq**: 自增序列号，匹配请求-响应
- **hdr_crc8**: 多项式 `G(x)=x⁸+x⁵+x⁴+1`，覆盖 SOF+payload_len+seq
- **crc16**: 覆盖 cmd_id + payload
- **最大帧长**: 137 字节 (MAX_FRAME_LEN)

### 命令码 (`schemas/protocol.ts`)

| CmdId             | 值    | 方向       | Payload               |
|-------------------|-------|-----------|------------------------|
| LOG_STRING        | 0x100 | MCU→Host  | LogStringPayload       |
| ALL_CONFIG        | 0x102 | 双向       | PersistableConfigV1    |
| COMMIT_ID         | 0x103 | MCU→Host  | CommitIdPayload (7B)   |
| CONFIG_WRITE_ACK  | 0x104 | MCU→Host  | ConfigWriteAckPayload  |
| READ_REQUEST      | 0x200 | Host→MCU  | ReadRequestPayload     |
| REBOOT_REQUEST    | 0x201 | Host→MCU  | (empty)                |

### 连接流程

1. `useSerial.connect()` — `requestPort()` → `port.open(921600 8N1)` → 启动后台 read loop
2. `useProtocol.connect()` — 额外发送 `READ_REQUEST(FirmwareCommit)`，等待 `COMMIT_ID` 回复 (2s 超时)
3. 比对 MCU commit hash 与 `__COMMIT_ID__`（构建时 `git rev-parse --short HEAD` 注入）
4. 不匹配则自动断开并报错；匹配则连接成功

### 参数读写

- **sendAllParams(allValues)** — 序列化配置 → 发送 `ALL_CONFIG` → 等待 `CONFIG_WRITE_ACK` (2s)
  - ACK.ok=1 → resolve(true)；ACK.ok=0 → 提取 reason 文本 → resolve(false)
- **readAllParams()** — 发送 `READ_REQUEST(AllConfig)` → 等待 `ALL_CONFIG` → parse → 返回 `Map<string, Map<string, number>>`
- **reboot()** — 发送 `REBOOT_REQUEST` (空 payload)，弹窗确认后执行

### 日志系统

- **MCU 日志**: 后台监听 `LOG_STRING` 帧，level 取自 payload[0]，text 为剩余字节
- **Host 日志**: `useProtocol.addHostLog(level, text)` 记录连接/写入/校验等事件
- **持久化**: 断开连接时不清空日志 (TX/RX/mcu/host 全部保留)

## Hook 架构

### `useSerial()` — 纯传输层
- 打开/关闭串口、读写 stream
- TX/RX 字节流日志 (`txLog`, `rxLog`, `txBytes`, `rxBytes`)
- 将接收字节喂入 `ConfiguratorParser` (parserRef)
- 暴露 `writeRaw(data: Uint8Array)` 供上层使用
- **不包含任何协议逻辑**

### `useProtocol(serial)` — 协议层
- 帧收发 (`sendFrame` 内部使用 `buildCmdFrame` + `serial.writeRaw`)
- 注册 LOG_STRING 回调 → 产出 `mcuLogs`
- 连接时 commit 校验
- `sendAllParams` / `readAllParams` / `reboot`
- host 侧日志 (`addHostLog`)
- 解析器错误计数同步 (`parserErrors`)

## How to Extend

**添加新参数** → `schemas/params.ts`: 在对应 joint 的 `params[]` 中添加 `ParamDef`。UI 自动渲染。

**添加新关节 (e.g. J6)** → `schemas/params.ts`: 添加 `JointSchema`；`JointCard.tsx`: 在 `JOINT_COLORS` 中添加颜色。

**添加新命令** → `schemas/protocol.ts`: 在 `CmdId` 添加条目、定义 payload 类型、在 `PAYLOAD_REGISTRY` 注册映射。然后在 `useProtocol.ts` 添加对应的收发逻辑。

**协议版本升级 (v2/v3)** → 定义新的 `PersistableConfigV2` 结构体，schema_version 递增，新增 CmdId 条目。

## UI Conventions

- **Chakra v3** 自定义 `system` theme (`theme.ts`, brand blue `600` 为主色)
- 浅色主题 (`bg="white"`, `bg="gray.50"`)
- 关节卡片使用彩色 accent bar：蓝/青/紫/橙/粉 (按关节编号索引)
- 参数控件使用 **NumberInput 数字输入框** (无 Slider)，带步进按钮
- 未连接时所有交互控件禁用
- Toast 通知异步操作结果 (success/error/info)
- 响应式网格：移动端 1 列，宽屏最多 5 列
- 主内容区和底部面板使用 **Splitter.Root** 垂直分割 (默认 75/25)，支持拖拽调整
- Reboot 按钮使用 **Popover** 弹窗二次确认
- **Vite 构建时注入** `__COMMIT_ID__` 用于固件版本校验；非 git 环境 fallback 为 `'dev'`

## Build Config (`vite.config.ts`)

- `define.__COMMIT_ID__`: `git rev-parse --short HEAD` (构建时注入)
- `server.watch.usePolling: true` + `interval: 1000`: WSL2 文件系统兼容

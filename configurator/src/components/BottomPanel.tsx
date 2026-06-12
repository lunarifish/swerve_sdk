import { Tabs, Box } from '@chakra-ui/react'
import { SerialTerminal } from './SerialTerminal'
import { LogDisplay } from './LogDisplay'
import type { ByteLogEntry, LogEntry } from '../hooks/useSerial'

interface BottomPanelProps {
  txLog: ByteLogEntry[]
  rxLog: ByteLogEntry[]
  txBytes: number
  rxBytes: number
  parserErrors?: number
  mcuLogs: LogEntry[]
  hostLogs: LogEntry[]
}

/**
 * 底部面板：Tabs 切换 Serial Monitor ↔ Log Console
 */
export function BottomPanel({ txLog, rxLog, txBytes, rxBytes, parserErrors, mcuLogs, hostLogs }: BottomPanelProps) {
  return (
    <Box h="full" display="flex" flexDirection="column" bg="white">
      <Tabs.Root defaultValue="logs" variant="subtle" size="sm" display="flex" flexDirection="column" flex={1} minH={0}>
        <Tabs.List bg="white" px={4} pt={1} flexShrink={0}>
          <Tabs.Trigger value="serial">
            Serial Monitor
          </Tabs.Trigger>
          <Tabs.Trigger value="logs">
            Logs
          </Tabs.Trigger>
        </Tabs.List>

        <Tabs.Content value="serial" p={0} flex={1} minH={0} display="flex" flexDirection="column">
          <SerialTerminal
            txLog={txLog}
            rxLog={rxLog}
            txBytes={txBytes}
            rxBytes={rxBytes}
            parserErrors={parserErrors}
          />
        </Tabs.Content>

        <Tabs.Content value="logs" p={0} flex={1} minH={0} display="flex" flexDirection="column">
          <LogDisplay mcuLogs={mcuLogs} hostLogs={hostLogs} />
        </Tabs.Content>
      </Tabs.Root>
    </Box>
  )
}

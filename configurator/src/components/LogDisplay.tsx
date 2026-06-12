import { useRef, useEffect } from 'react'
import { Box, Flex, Text, HStack, Badge } from '@chakra-ui/react'
import { LogLevel, type LogEntry } from '../hooks/useSerial'
import { formatTime } from './SerialTerminal'

interface LogDisplayProps {
  mcuLogs: LogEntry[]
  hostLogs: LogEntry[]
}

/** 日志级别 → 颜色映射 */
const LEVEL_CONFIG: Record<LogLevel, { label: string; color: string }> = {
  [LogLevel.Debug]: { label: 'DEBUG', color: 'gray.400' },
  [LogLevel.Info]: { label: 'INFO', color: 'blue.400' },
  [LogLevel.Warning]: { label: 'WARN', color: 'yellow.400' },
  [LogLevel.Error]: { label: 'ERROR', color: 'red.400' },
}

/** 来源 → 标签 */
const SOURCE_LABEL: Record<string, string> = {
  mcu: 'MCU',
  host: 'HOST',
}

export function LogDisplay({ mcuLogs, hostLogs }: LogDisplayProps) {
  const ref = useRef<HTMLDivElement>(null)

  // 合并并按时间排序
  const allLogs = [...mcuLogs, ...hostLogs].sort((a, b) => a.ts - b.ts)

  useEffect(() => {
    if (ref.current) {
      ref.current.scrollTop = ref.current.scrollHeight
    }
  }, [allLogs])

  return (
    <Box display="flex" flexDirection="column" flex={1} minH={0} px={5} py={3}>
      <Flex justify="space-between" align="center" mb={2} flexShrink={0}>
        {/* <Text fontSize="sm" fontWeight="semibold" color="gray.700">
          Log Console
        </Text> */}
        <HStack gap={3}>
          <HStack gap={1.5}>
            <Badge variant="subtle" colorPalette="blue" size="xs">MCU</Badge>
            <Text fontSize="xs" color="gray.500" fontFamily="mono">{mcuLogs.length}</Text>
          </HStack>
          <HStack gap={1.5}>
            <Badge variant="subtle" colorPalette="purple" size="xs">HOST</Badge>
            <Text fontSize="xs" color="gray.500" fontFamily="mono">{hostLogs.length}</Text>
          </HStack>
        </HStack>
      </Flex>

      <Box
        ref={ref}
        flex={1}
        minH={0}
        overflowY="auto"
        bg="#1e1e2e"
        borderWidth="1px"
        borderColor="gray.700"
        borderRadius="md"
        p={1.5}
        fontFamily="mono"
        fontSize="2xs"
        lineHeight="1.5"
      >
        {allLogs.length === 0 ? (
          <Text color="gray.500" fontStyle="italic" fontSize="2xs" p={1}>
            暂无日志...
          </Text>
        ) : (
          allLogs.map((entry, i) => {
            const levelCfg = LEVEL_CONFIG[entry.level]
            return (
              <HStack key={i} gap={0} whiteSpace="nowrap">
                <Text as="span" color="gray.500" flexShrink={0} mr={1}>
                  {formatTime(entry.ts)}
                </Text>
                <Badge variant="solid" colorPalette={entry.source === 'mcu' ? 'blue' : 'purple'} size="xs" mr={1} fontSize="8px" px={1}>
                  {SOURCE_LABEL[entry.source]}
                </Badge>
                <Text as="span" color={levelCfg.color} flexShrink={0} mr={1.5} fontWeight="bold" fontSize="2xs">
                  {levelCfg.label}
                </Text>
                <Text as="span" color="gray.200">
                  {entry.text}
                </Text>
              </HStack>
            )
          })
        )}
      </Box>
    </Box>
  )
}

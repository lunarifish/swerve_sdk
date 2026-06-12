import { useRef, useEffect } from 'react'
import { Box, Flex, Text, HStack } from '@chakra-ui/react'
import type { ByteLogEntry } from '../hooks/useSerial'

interface SerialTerminalProps {
  txLog: ByteLogEntry[]
  rxLog: ByteLogEntry[]
  txBytes: number
  rxBytes: number
  /** 协议解析 CRC 错误计数 */
  parserErrors?: number
}

function formatHex(data: Uint8Array): string {
  return Array.from(data)
    .map((b) => b.toString(16).toUpperCase().padStart(2, '0'))
    .join(' ')
}

export function formatTime(ts: number): string {
  const d = new Date(ts)
  return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}.${d.getMilliseconds().toString().padStart(3, '0')}`
}

function LogColumn({
  title,
  color,
  log,
}: {
  title: string
  color: string
  log: ByteLogEntry[]
}) {
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (ref.current) {
      ref.current.scrollTop = ref.current.scrollHeight
    }
  }, [log])

  return (
    <Box flex={1} minW={0} display="flex" flexDirection="column">
      <Text fontSize="xs" fontWeight="semibold" color="gray.500" mb={1.5} flexShrink={0}>
        {title}
      </Text>
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
        {log.length === 0 ? (
          <Text color="gray.500" fontStyle="italic" fontSize="2xs" p={1}>
            等待数据...
          </Text>
        ) : (
          log.map((entry, i) => (
            <HStack key={i} gap={0} whiteSpace="nowrap">
              <Text as="span" color="gray.500" flexShrink={0} mr={1.5}>
                {formatTime(entry.ts)}
              </Text>
              <Text as="span" color={color} fontFamily="mono">
                {formatHex(entry.data)}
              </Text>
            </HStack>
          ))
        )}
      </Box>
    </Box>
  )
}

export function SerialTerminal({ txLog, rxLog, txBytes, rxBytes, parserErrors }: SerialTerminalProps) {
  return (
    <Box display="flex" flexDirection="column" flex={1} minH={0} px={5} py={3}>
      <Flex justify="space-between" align="center" mb={3} flexShrink={0}>
        {/* <Text fontSize="sm" fontWeight="semibold" color="gray.700">
          Serial Monitor
        </Text> */}
        <HStack gap={5}>
          <HStack gap={1.5}>
            <Text fontSize="xs" color="gray.400" fontWeight="medium">
              TX
            </Text>
            <Text fontSize="xs" color="blue.500" fontFamily="mono" fontWeight="semibold">
              {txBytes.toLocaleString()} B
            </Text>
          </HStack>
          <HStack gap={1.5}>
            <Text fontSize="xs" color="gray.400" fontWeight="medium">
              RX
            </Text>
            <Text fontSize="xs" color="green.500" fontFamily="mono" fontWeight="semibold">
              {rxBytes.toLocaleString()} B
            </Text>
          </HStack>
          {parserErrors !== undefined && parserErrors > 0 && (
            <HStack gap={1.5}>
              <Text fontSize="xs" color="gray.400" fontWeight="medium">
                CRC Err
              </Text>
              <Text fontSize="xs" color="red.500" fontFamily="mono" fontWeight="semibold">
                {parserErrors}
              </Text>
            </HStack>
          )}
        </HStack>
      </Flex>
      <Flex gap={4} flex={1} minH={0}>
        <LogColumn title="TX" color="#60a5fa" log={txLog} />
        <LogColumn title="RX" color="#4ade80" log={rxLog} />
      </Flex>
    </Box>
  )
}

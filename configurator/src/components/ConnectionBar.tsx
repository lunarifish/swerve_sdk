import { HStack, Button, Badge, Box } from '@chakra-ui/react'
import type { ConnectionStatus } from '../hooks/useSerial'

interface ConnectionBarProps {
  status: ConnectionStatus
  onConnect: () => void
  onDisconnect: () => void
}

const STATUS_CONFIG: Record<ConnectionStatus, { label: string; colorPalette: string }> = {
  disconnected: { label: '未连接', colorPalette: 'gray' },
  connecting: { label: '连接中...', colorPalette: 'yellow' },
  connected: { label: '已连接', colorPalette: 'green' },
}

export function ConnectionBar({
  status,
  onConnect,
  onDisconnect,
}: ConnectionBarProps) {
  const cfg = STATUS_CONFIG[status]
  const isConnected = status === 'connected'
  const isConnecting = status === 'connecting'

  return (
    <HStack gap={2.5} bg="gray.50" px={3} py={1.5} borderRadius="md" borderWidth="1px" borderColor="gray.200">
      <Badge
        variant="subtle"
        colorPalette={cfg.colorPalette}
        size="sm"
        borderRadius="full"
        px={2.5}
        py={0.5}
      >
        <HStack gap={1.5}>
          <Box
            w="6px"
            h="6px"
            borderRadius="full"
            bg={`${cfg.colorPalette}.500`}
            animation={isConnecting ? 'pulse 1.5s infinite' : undefined}
          />
          <Box as="span">{cfg.label}</Box>
        </HStack>
      </Badge>

      {isConnected ? (
        <Button size="sm" colorPalette="red" variant="solid" onClick={onDisconnect}>
          断开
        </Button>
      ) : (
        <Button
          size="sm"
          colorPalette="brand"
          variant="solid"
          onClick={onConnect}
          disabled={isConnecting}
        >
          {isConnecting ? '连接中...' : '连接'}
        </Button>
      )}
    </HStack>
  )
}

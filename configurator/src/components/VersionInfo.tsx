import { VStack, Text } from '@chakra-ui/react'
import { PROTOCOL_VERSION } from '../schemas/protocol'

/**
 * 版本信息组件 — 左上角两行灰色小字
 *
 * 显示当前 App commit ID 和 Schema 协议版本。
 * commit ID 由 vite.config.ts 构建时通过 git rev-parse 注入。
 */
export function VersionInfo() {
  return (
    <VStack gap={1} flexShrink={0} align="right">
      <Text fontSize="10px" color="gray.400" fontFamily="mono" userSelect="all" whiteSpace="nowrap">
        App commit id: {__COMMIT_ID__}
      </Text>
      <Text fontSize="10px" color="gray.400" fontFamily="mono" whiteSpace="nowrap">
        Schema version: v{PROTOCOL_VERSION}
      </Text>
    </VStack>
  )
}

import { HStack, Text, NumberInput, Box, Separator } from '@chakra-ui/react'
import type { ParamDef } from '../schemas/params'

interface ParamSpinboxProps {
  param: ParamDef
  value: number
  onChange: (key: string, value: number) => void
  disabled?: boolean
}

/**
 * 纯数字输入框组件（无滑块）。
 * 对齐 MCU PersistableConfigV1 参数，通过 ↑↓ 箭头步进调节。
 */
export function ParamSpinbox({ param, value, onChange, disabled }: ParamSpinboxProps) {
  // 在 Kd 参数前加分隔线，视觉上区分 Kp/Kd 与 vel/accel
  const isKd = param.key === 'pd_kd'

  return (
    <Box>
      {isKd && <Separator mb={3} opacity={0.5} />}
      <HStack gap={2.5} w="full">
        <Text
          fontSize="xs"
          fontWeight="medium"
          color="fg"
          minW="52px"
          textAlign="right"
          flexShrink={0}
        >
          {param.label}
        </Text>

        <NumberInput.Root
          min={param.min}
          max={param.max}
          step={param.step}
          value={String(value)}
          disabled={disabled}
          onValueChange={(d) => {
            const v = parseFloat(d.value)
            if (!isNaN(v)) onChange(param.key, v)
          }}
          size="xs"
          flex={1}
        >
          <NumberInput.Input
            textAlign="right"
            fontFamily="mono"
            fontSize="xs"
            borderRadius="md"
            borderColor="gray.200"
            _focus={{ borderColor: 'brand.500', boxShadow: '0 0 0 1px {colors.brand.500}' }}
          />
        </NumberInput.Root>

        <Text fontSize="xs" color="fg.muted" minW="44px" flexShrink={0}>
          {param.unit || ''}
        </Text>
      </HStack>
    </Box>
  )
}

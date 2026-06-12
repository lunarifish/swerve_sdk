import { Card, Stack, Heading, Box } from '@chakra-ui/react'
import type { JointSchema } from '../schemas/params'
import { ParamSpinbox } from './ParamSpinbox'

interface JointCardProps {
  schema: JointSchema
  values: Map<string, number>
  onChange: (key: string, value: number) => void
  disabled: boolean
}

const JOINT_COLORS = ['blue', 'teal', 'purple', 'orange', 'pink'] as const

export function JointCard({ schema, values, onChange, disabled }: JointCardProps) {
  const jointIndex = parseInt(schema.jointName.replace('J', ''), 10) - 1
  const accentColor = JOINT_COLORS[jointIndex] ?? 'blue'

  return (
    <Card.Root
      variant="elevated"
      size="sm"
      borderRadius="lg"
      bg="white"
      borderWidth="1px"
      borderColor="gray.200"
      boxShadow="xs"
      _hover={{ boxShadow: 'sm' }}
      transition="box-shadow 0.2s"
      overflow="hidden"
    >
      <Box
        h="3px"
        bg={`${accentColor}.500`}
        flexShrink={0}
      />
      <Card.Header
        py={3}
        px={4}
      >
        <Heading size="sm" fontWeight="semibold" color={`${accentColor}.600`} letterSpacing="tight">
          {schema.jointName}
        </Heading>
      </Card.Header>

      <Card.Body pt={1} pb={4} px={4}>
        <Stack gap={3}>
          {schema.params.map((param) => (
            <ParamSpinbox
              key={param.key}
              param={param}
              value={values.get(param.key) ?? param.defaultValue}
              onChange={onChange}
              disabled={disabled}
            />
          ))}
        </Stack>
      </Card.Body>
    </Card.Root>
  )
}

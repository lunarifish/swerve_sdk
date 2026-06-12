import { useState, useCallback } from "react";
import {
  Box,
  Flex,
  SimpleGrid,
  Button,
  HStack,
  Separator,
  Splitter,
  Popover,
  Text,
} from "@chakra-ui/react";
import { AppToaster, toaster } from "./components/ui/toaster";
import { JOINT_SCHEMAS, buildDefaultValues } from "./schemas/params";
import { VersionInfo } from "./components/VersionInfo";
import { ConnectionBar } from "./components/ConnectionBar";
import { JointCard } from "./components/JointCard";
import { BottomPanel } from "./components/BottomPanel";
import { useSerial, LogLevel } from "./hooks/useSerial";
import { useProtocol } from "./hooks/useProtocol";

function App() {
  const serial = useSerial();
  const protocol = useProtocol(serial);

  const [allValues, setAllValues] = useState(() =>
    buildDefaultValues(JOINT_SCHEMAS),
  );

  const isConnected = serial.status === "connected";

  const handleParamChange = useCallback(
    (jointName: string, key: string, value: number) => {
      setAllValues((prev) => {
        const next = new Map(prev);
        const jointValues = new Map(next.get(jointName));
        jointValues.set(key, value);
        next.set(jointName, jointValues);
        return next;
      });
    },
    [],
  );

  /** 连接串口 */
  const handleConnect = useCallback(async () => {
    const ok = await protocol.connect();
    if (ok) {
      protocol.addHostLog(LogLevel.Info, '串口已连接');
      toaster.create({
        title: "串口已连接",
        // description: "921600 8N1",
        type: "success",
      });
    } else {
      protocol.addHostLog(LogLevel.Error, '串口连接失败');
      toaster.create({
        title: "连接失败",
        description: "无法打开串口，请检查设备",
        type: "error",
      });
    }
  }, [protocol]);

  /** 断开串口 */
  const handleDisconnect = useCallback(async () => {
    await protocol.disconnect();
    protocol.addHostLog(LogLevel.Info, '串口已断开');
    toaster.create({ title: "串口已断开", type: "info" });
  }, [protocol]);

  /** 一键发送全部关节的所有参数 */
  const handleSendAll = useCallback(async () => {
    protocol.addHostLog(LogLevel.Info, '发送全部参数到MCU');
    const ok = await protocol.sendAllParams(allValues);
    if (ok) {
      protocol.addHostLog(LogLevel.Info, '参数写入完成');
      toaster.create({
        title: "发送成功",
        description: "所有参数已写入成功，重启后生效",
        type: "success",
      });
    } else {
      protocol.addHostLog(LogLevel.Error, '参数发送失败');
      toaster.create({
        title: "发送失败",
        description: "协议未实现，参数未发送",
        type: "warning",
      });
    }
  }, [allValues, protocol]);

  /** 从 MCU 读取所有参数并更新 UI */
  const handleReadAll = useCallback(async () => {
    protocol.addHostLog(LogLevel.Info, '请求读取 MCU 参数...');
    const result = await protocol.readAllParams();
    if (result) {
      setAllValues(result);
      protocol.addHostLog(LogLevel.Info, 'MCU 参数已同步');
      toaster.create({
        title: "读取成功",
        description: "MCU 参数已同步",
        type: "success",
      });
    } else {
      protocol.addHostLog(LogLevel.Warning, '读取 MCU 参数超时/失败');
      toaster.create({
        title: "读取失败",
        description: "协议未实现或串口无响应",
        type: "warning",
      });
    }
  }, [protocol]);

  return (
    <Box
      minH="100vh"
      h="100vh"
      display="flex"
      flexDirection="column"
      bg="gray.50"
    >
      <AppToaster />
      <Flex
        as="header"
        px={6}
        py={3}
        bg="white"
        borderBottomWidth="1px"
        borderColor="gray.200"
        align="center"
        justify="space-between"
        flexShrink={0}
        gap={4}
        flexWrap="wrap"
        boxShadow="sm"
      >
        <HStack gap={4}>
          <ConnectionBar
            status={serial.status}
            onConnect={handleConnect}
            onDisconnect={handleDisconnect}
          />
          <Separator orientation="vertical" height="8" />
          <Button
            size="sm"
            colorPalette="green"
            variant="solid"
            disabled={!isConnected}
            onClick={handleReadAll}
          >
            读参数
          </Button>
          <Button
            size="sm"
            colorPalette="brand"
            variant="solid"
            disabled={!isConnected}
            onClick={handleSendAll}
          >
            写参数
          </Button>
          <Separator orientation="vertical" height="8" />
          <Popover.Root positioning={{ placement: "bottom-start" }}>
            <Popover.Trigger>
              <Button
                size="sm"
                colorPalette="red"
                variant="outline"
                disabled={!isConnected}
              >
                重启MCU
              </Button>
            </Popover.Trigger>
            <Popover.Positioner>
              <Popover.Content>
                <Popover.Body p={4}>
                  <Text fontSize="sm" mb={3}>确定要重启 MCU 吗？重启后机械臂将失能，且上位机需要重新连接。</Text>
                  <HStack gap={2} justify="end">
                    <Popover.CloseTrigger asChild>
                      <Button size="sm" variant="outline" colorPalette="gray">
                        取消
                      </Button>
                    </Popover.CloseTrigger>
                    <Button
                      size="sm"
                      colorPalette="red"
                      variant="solid"
                      onClick={async () => {
                        protocol.addHostLog(LogLevel.Info, '发送重启 MCU 指令');
                        await protocol.reboot();
                        protocol.addHostLog(LogLevel.Info, 'MCU 已重启，请重新连接');
                        toaster.create({ title: "MCU 已重启", type: "info" });
                      }}
                    >
                      确认重启
                    </Button>
                  </HStack>
                </Popover.Body>
              </Popover.Content>
            </Popover.Positioner>
          </Popover.Root>
        </HStack>

        <HStack gap={2} flexShrink={0}>
          <VersionInfo />
        </HStack>
      </Flex>

      <Splitter.Root
        panels={[{ id: "main" }, { id: "bottom" }]}
        defaultSize={[75, 25]}
        orientation="vertical"
        flex={1}
        minH={0}
      >
        <Splitter.Panel id="main">
          <Box h="full" overflowY="auto" p={5}>
            <SimpleGrid columns={{ base: 1, sm: 2, md: 3, lg: 4, xl: 5 }} gap={4}>
              {JOINT_SCHEMAS.map((schema) => (
                <JointCard
                  key={schema.jointName}
                  schema={schema}
                  values={allValues.get(schema.jointName) ?? new Map()}
                  onChange={(key, value) =>
                    handleParamChange(schema.jointName, key, value)
                  }
                  disabled={!isConnected}
                />
              ))}
            </SimpleGrid>
          </Box>
        </Splitter.Panel>

        <Splitter.ResizeTrigger id="main:bottom" />

        <Splitter.Panel id="bottom">
          <BottomPanel
            txLog={serial.txLog}
            rxLog={serial.rxLog}
            txBytes={serial.txBytes}
            rxBytes={serial.rxBytes}
            parserErrors={protocol.parserErrors}
            mcuLogs={protocol.mcuLogs}
            hostLogs={protocol.hostLogs}
          />
        </Splitter.Panel>
      </Splitter.Root>
    </Box>
  );
}

export default App;

#include "bsp/w25q64.h"

extern OSPI_HandleTypeDef hospi2;

/*************************************************************************************************
*	�� �� ��: OSPI_W25Qxx_Init
*	��ڲ���: ��
*	�� �� ֵ: OSPI_W25Qxx_OK - ��ʼ���ɹ���W25Qxx_ERROR_INIT - ��ʼ������
*	��������: ��ʼ�� OSPI ���ã���ȡW25Q64ID
*	˵    ��: ��	
*************************************************************************************************/

int8_t OSPI_W25Qxx_Init(void)
{
    uint32_t    Device_ID;  // ����ID
    
    
    
    Device_ID = OSPI_W25Qxx_ReadID();       // ��ȡ����ID
    
    if( Device_ID == W25Qxx_FLASH_ID )      // ����ƥ��
    {
//        printf ("W25Q64 OK,flash ID:%X\r\n",Device_ID);		// ��ʼ���ɹ�
        return OSPI_W25Qxx_OK;              // ���سɹ���־		
    }
    else
    {
//        printf ("W25Q64 ERROR!!!!!  ID:%X\r\n",Device_ID);	// ��ʼ��ʧ��	
        return W25Qxx_ERROR_INIT;           // ���ش����־
    }   
}

/*************************************************************************************************
*	�� �� ��: OSPI_W25Qxx_AutoPollingMemReady
*	��ڲ���: ��
*	�� �� ֵ: OSPI_W25Qxx_OK - ͨ������������W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*	��������: ʹ���Զ���ѯ��־��ѯ���ȴ�ͨ�Ž���
*	˵    ��: ÿһ��ͨ�Ŷ�Ӧ�õ��ô˺������ȴ�ͨ�Ž������������Ĳ���	
******************************************************************************************FANKE*****/

int8_t OSPI_W25Qxx_AutoPollingMemReady(void)
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    OSPI_AutoPollingTypeDef sConfig;			// ��ѯ�Ƚ�������ò���
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.Address            = 0x0;                                // ��ַ0
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // �޵�ַģʽ
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // ��ַ����24λ
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // ��ֹ��ַDTRģʽ
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.NbData             = 1;                                  // ͨ�����ݳ���
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
    
    sCommand.Instruction        = W25Qxx_CMD_ReadStatus_REG1;         // ��״̬��Ϣ�Ĵ���
    
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }

// ��ͣ�Ĳ�ѯ W25Qxx_CMD_ReadStatus_REG1 �Ĵ���������ȡ����״̬�ֽ��е� W25Qxx_Status_REG1_BUSY ��ͣ����0���Ƚ�
// ��״̬�Ĵ���1�ĵ�0λ��ֻ������Busy��־λ�������ڲ���/д������/д����ʱ�ᱻ��1�����л�ͨ�Ž���Ϊ0
// FANKE	
    sConfig.Match         = 0;                                      //	ƥ��ֵ	
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;                //	������
    sConfig.Interval      = 0x10;                                   //	��ѯ���
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;         // �Զ�ֹͣģʽ
    sConfig.Mask          = W25Qxx_Status_REG1_BUSY;                // ������ѯģʽ�½��յ�״̬�ֽڽ������Σ�ֻ�Ƚ���Ҫ�õ���λ
    
    // ������ѯ�ȴ�����
    if (HAL_OSPI_AutoPolling(&hospi2, &sConfig,HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // ͨ����������	
}

/*************************************************************************************************
*	�� �� ��: OSPI_W25Qxx_ReadID
*	��ڲ���: ��
*	�� �� ֵ: W25Qxx_ID - ��ȡ��������ID��W25Qxx_ERROR_INIT - ͨ�š���ʼ������
*	��������: ��ʼ�� OSPI ���ã���ȡ����ID
*	˵    ��: ��	
**************************************************************************************************/

uint32_t OSPI_W25Qxx_ReadID(void)	
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    
    uint8_t	OSPI_ReceiveBuff[3];		      // �洢OSPI����������
    uint32_t	W25Qxx_ID;					      // ������ID
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // �޵�ַģʽ
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // ��ַ����24λ   
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.NbData             = 3;                                  // �������ݵĳ���
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��   
    
    sCommand.Instruction        = W25Qxx_CMD_JedecID;                 // ִ�ж�����ID����
    
    
    HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);	// ����ָ��
    
    HAL_OSPI_Receive (&hospi2, OSPI_ReceiveBuff, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);	// ��������
    
    W25Qxx_ID = (OSPI_ReceiveBuff[0] << 16) | (OSPI_ReceiveBuff[1] << 8 ) | OSPI_ReceiveBuff[2];	// ���õ���������ϳ�ID
    
    return W25Qxx_ID; // ����ID
}


/*************************************************************************************************
*	�� �� ��: OSPI_W25Qxx_MemoryMappedMode
*	��ڲ���: ��
*	�� �� ֵ: OSPI_W25Qxx_OK - дʹ�ܳɹ���W25Qxx_ERROR_WriteEnable - дʹ��ʧ��
*	��������: ��OSPI����Ϊ�ڴ�ӳ��ģʽ
*	˵    ��: ��
**************************************************************************************************/

int8_t OSPI_W25Qxx_MemoryMappedMode(void)
{
   OSPI_RegularCmdTypeDef     sCommand;         // QSPI��������
   OSPI_MemoryMappedTypeDef   sMemMappedCfg;    // �ڴ�ӳ����ʲ���

   sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // ͨ������
   sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID

   sCommand.Instruction             = W25Qxx_CMD_FastReadQuad_IO;             // 1-4-4ģʽ��(1��ָ��4�ߵ�ַ4������)�����ٶ�ȡָ��
   sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1��ָ��ģʽ
   sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // ָ���8λ
   sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // ��ָֹ��DTRģʽ

   sCommand.AddressMode             = HAL_OSPI_ADDRESS_4_LINES;               // 4�ߵ�ַģʽ
   sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // ��ַ����24λ
   sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // ��ֹ��ַDTRģʽ

   sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // �޽����ֽ�    
   sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // ��ֹ���ֽ�DTRģʽ 

   sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4������ģʽ
   sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // ��ֹ����DTRģʽ 

   sCommand.DummyCycles             = 6;                                      // �����ڸ���
   sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // ��ʹ��DQS 
   sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // ÿ�δ������ݶ�����ָ��   

    // д����
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// �������ݴ���
    }   
    
    sMemMappedCfg.TimeOutActivation  = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;          // ���ó�ʱ������, nCS ���ּ���״̬
    sMemMappedCfg.TimeOutPeriod      = 0;                                         // ��ʱ�ж�����
    // �����ڴ�ӳ��ģʽ
    if (HAL_OSPI_MemoryMapped(&hospi2,  &sMemMappedCfg) != HAL_OK)	// ��������
    {
        return W25Qxx_ERROR_MemoryMapped; 	// �����ڴ�ӳ��ģʽ����
    }
    return OSPI_W25Qxx_OK; // ���óɹ�
}

/*************************************************************************************************
*   �� �� ��: OSPI_W25Qxx_WriteEnable
*   ��ڲ���: ��
*   �� �� ֵ: OSPI_W25Qxx_OK - дʹ�ܳɹ���W25Qxx_ERROR_WriteEnable - дʹ��ʧ��
*   ��������: ����дʹ������
*   ˵    ��: ��	
**************************************************************************************************/

int8_t OSPI_W25Qxx_WriteEnable(void)
{
   OSPI_RegularCmdTypeDef  sCommand;// OSPI��������
   OSPI_AutoPollingTypeDef sConfig;// ��ѯ�Ƚ�������ò���

   sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // ͨ������
   sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID                       
   sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1��ָ��ģʽ
   sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // ָ���8λ
   sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // ��ָֹ��DTRģʽ
   sCommand.Address                 = 0;                                      // ��ַ0
   sCommand.AddressMode             = HAL_OSPI_ADDRESS_NONE;                  // �޵�ַģʽ   
   sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // ��ַ����24λ
   sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // ��ֹ��ַDTRģʽ
   sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   //	��ֹ���ֽ�DTRģʽ
   sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          //	�޽����ֽ�
   sCommand.DataMode                = HAL_OSPI_DATA_NONE;                     // ������ģʽ
   sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // ��ֹ����DTRģʽ
   sCommand.DummyCycles             = 0;                                      // �����ڸ���
   sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // ��ʹ��DQS
   sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // ÿ�δ������ݶ�����ָ��

   sCommand.Instruction             = W25Qxx_CMD_WriteEnable;                 // дʹ������

   // ����дʹ������
   if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
   {
      return W25Qxx_ERROR_WriteEnable;	
   }
   // ���Ͳ�ѯ״̬�Ĵ������� 
   sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
   sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID 
   sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
   sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
   sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
   sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // �޵�ַģʽ  
   sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�   
   sCommand.DummyCycles        = 0;                                  // �����ڸ���
   sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS   
   sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
   sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;               // 1������ģʽ
   sCommand.NbData             = 1;                                  // ͨ�����ݳ���

   sCommand.Instruction        = W25Qxx_CMD_ReadStatus_REG1;         // ��ѯ״̬�Ĵ�������

   if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
   {
      return W25Qxx_ERROR_WriteEnable;	
   }

// ��ͣ�Ĳ�ѯ W25Qxx_CMD_ReadStatus_REG1 �Ĵ���������ȡ����״̬�ֽ��е� W25Qxx_Status_REG1_WEL ��ͣ���� 0x02 ���Ƚ�
// ��״̬�Ĵ���1�ĵ�1λ��ֻ������WELдʹ�ܱ�־λ���ñ�־λΪ1ʱ��������Խ���д����
// FANKE	7B0	
    sConfig.Match         = 0x02;										//	ƥ��ֵ	
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;			//	������
    sConfig.Interval      = 0x10;										//	��ѯ���
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;	// �Զ�ֹͣģʽ
    sConfig.Mask          = W25Qxx_Status_REG1_WEL; 			// ������ѯģʽ�½��յ�״̬�ֽڽ������Σ�ֻ�Ƚ���Ҫ�õ���λ
    
    if (HAL_OSPI_AutoPolling(&hospi2, &sConfig,HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK;  // ͨ����������
}

/*************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_SectorErase
*
*	��ڲ���: SectorAddress - Ҫ�����ĵ�ַ
*
*	�� �� ֵ: OSPI_W25Qxx_OK - �����ɹ�
*			    W25Qxx_ERROR_Erase - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ������������������ÿ�β���4K�ֽ�
*
*	˵    ��: 1.���� W25Q64JV �����ֲ�����Ĳ����ο�ʱ�䣬����ֵΪ 45ms�����ֵΪ400ms
*				 2.ʵ�ʵĲ����ٶȿ��ܴ���45ms��Ҳ����С��45ms
*				 3.flashʹ�õ�ʱ��Խ������������ʱ��Ҳ��Խ��
*
**************************************************************************************************/

int8_t OSPI_W25Qxx_SectorErase(uint32_t SectorAddress)	
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.Address            = SectorAddress;                      // ��ַ
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1�ߵ�ַģʽ
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // ��ַ����24λ
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // ��ֹ��ַDTRģʽ
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // ������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
    
    sCommand.Instruction        = W25Qxx_CMD_SectorErase;             // ��������ָ�ÿ�β���4K�ֽ�
    
    // ����дʹ��
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// дʹ��ʧ��
    }
    // ���Ͳ���ָ��
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }   
    // ʹ���Զ���ѯ��־λ���ȴ������Ľ��� 
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // �����ɹ�
}

/*************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_BlockErase_32K
*
*	��ڲ���: SectorAddress - Ҫ�����ĵ�ַ
*
*	�� �� ֵ: OSPI_W25Qxx_OK - �����ɹ�
*			    W25Qxx_ERROR_Erase - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ���п����������ÿ�β���32K�ֽ�
*
*	˵    ��: 1.���� W25Q64JV �����ֲ�����Ĳ����ο�ʱ�䣬����ֵΪ 120ms�����ֵΪ1600ms
*				 2.ʵ�ʵĲ����ٶȿ��ܴ���120ms��Ҳ����С��120ms
*				 3.flashʹ�õ�ʱ��Խ������������ʱ��Ҳ��Խ��
*
*************************************************************************************************/

int8_t OSPI_W25Qxx_BlockErase_32K (uint32_t SectorAddress)	
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.Address            = SectorAddress;                      // ��ַ
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1�ߵ�ַģʽ
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // ��ַ����24λ
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // ��ֹ��ַDTRģʽ
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // ������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
    
    sCommand.Instruction        = W25Qxx_CMD_BlockErase_32K;          // �����ָ�ÿ�β���32K�ֽ�
    
    // ����дʹ��
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// дʹ��ʧ��
    }
    // ���Ͳ���ָ��
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }   
    // ʹ���Զ���ѯ��־λ���ȴ������Ľ��� 
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // �����ɹ�	
}

/*************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_BlockErase_64K
*
*	��ڲ���: SectorAddress - Ҫ�����ĵ�ַ
*
*	�� �� ֵ: OSPI_W25Qxx_OK - �����ɹ�
*			    W25Qxx_ERROR_Erase - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ���п����������ÿ�β���64K�ֽ�
*
*	˵    ��: 1.���� W25Q64JV �����ֲ�����Ĳ����ο�ʱ�䣬����ֵΪ 150ms�����ֵΪ2000ms
*				 2.ʵ�ʵĲ����ٶȿ��ܴ���150ms��Ҳ����С��150ms
*				 3.flashʹ�õ�ʱ��Խ������������ʱ��Ҳ��Խ��
*				 4.ʵ��ʹ�ý���ʹ��64K������������ʱ�����
*
**************************************************************************************************/
int8_t OSPI_W25Qxx_BlockErase_64K (uint32_t SectorAddress)	
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.Address            = SectorAddress;                      // ��ַ
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;            // 1�ߵ�ַģʽ
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;           // ��ַ����24λ
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // ��ֹ��ַDTRģʽ
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // ������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
    
    sCommand.Instruction        = W25Qxx_CMD_BlockErase_64K;          // ��������ָ�ÿ�β���64K�ֽ�
    
    // ����дʹ��
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// дʹ��ʧ��
    }
    // ���Ͳ���ָ��
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }   
    // ʹ���Զ���ѯ��־λ���ȴ������Ľ��� 
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // �����ɹ�			
}

/*************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_ChipErase
*
*	��ڲ���: ��
*
*	�� �� ֵ: OSPI_W25Qxx_OK - �����ɹ�
*			    W25Qxx_ERROR_Erase - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ������Ƭ��������
*
*	˵    ��: 1.���� W25Q64JV �����ֲ�����Ĳ����ο�ʱ�䣬����ֵΪ 20s�����ֵΪ100s
*				 2.ʵ�ʵĲ����ٶȿ��ܴ���20s��Ҳ����С��20s
*				 3.flashʹ�õ�ʱ��Խ������������ʱ��Ҳ��Խ��
*
*************************************************************************************************/
int8_t OSPI_W25Qxx_ChipErase (void)	
{
    OSPI_RegularCmdTypeDef  sCommand;		// OSPI��������
    OSPI_AutoPollingTypeDef sConfig;       // ��ѯ�Ƚ�������ò���
    
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;         // ͨ������
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;                // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;        // 1��ָ��ģʽ
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;        // ָ���8λ
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;   // ��ָֹ��DTRģʽ
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;              // �޵�ַģʽ
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;       // ��ֹ��ַDTRģʽ   
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;      //	�޽����ֽ�
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;                 // ������ģʽ
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;          // ��ֹ����DTRģʽ
    sCommand.DummyCycles        = 0;                                  // �����ڸ���
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;               // ��ʹ��DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;       // ÿ�δ������ݶ�����ָ��
    
    sCommand.Instruction        = W25Qxx_CMD_ChipErase;               // ȫƬ����ָ��
    
    // ����дʹ��
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;		// дʹ��ʧ��
    }
    // ���Ͳ���ָ��
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }   
    
    // ���Ͳ�ѯ״̬�Ĵ�������
    sCommand.DataMode       = HAL_OSPI_DATA_1_LINE;          // һ������ģʽ
    sCommand.NbData         = 1;                             // ���ݳ���1
    sCommand.Instruction    = W25Qxx_CMD_ReadStatus_REG1;    // ״̬�Ĵ�������
    
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
    return W25Qxx_ERROR_AUTOPOLLING;	
    }

// ��ͣ�Ĳ�ѯ W25Qxx_CMD_ReadStatus_REG1 �Ĵ���������ȡ����״̬�ֽ��е� W25Qxx_Status_REG1_BUSY ��ͣ����0���Ƚ�
// ��״̬�Ĵ���1�ĵ�0λ��ֻ������Busy��־λ�������ڲ���/д������/д����ʱ�ᱻ��1�����л�ͨ�Ž���Ϊ0
        
    sConfig.Match         = 0;											//	ƥ��ֵ	
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;			//	������
    sConfig.Interval      = 0x10;										//	��ѯ���
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;	// �Զ�ֹͣģʽ
    sConfig.Mask          = W25Qxx_Status_REG1_BUSY; 			// ������ѯģʽ�½��յ�״̬�ֽڽ������Σ�ֻ�Ƚ���Ҫ�õ���λ
    
    // W25Q64��Ƭ�����ĵ��Ͳο�ʱ��Ϊ20s�����ʱ��Ϊ100s������ĳ�ʱ�ȴ�ֵ W25Qxx_ChipErase_TIMEOUT_MAX Ϊ 100S
    if (HAL_OSPI_AutoPolling(&hospi2, &sConfig,W25Qxx_ChipErase_TIMEOUT_MAX) != HAL_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING; // ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // �����ɹ�				
}

/**********************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_WritePage
*
*	��ڲ���: pBuffer 		 - Ҫд�������
*				 WriteAddr 		 - Ҫд�� W25Qxx �ĵ�ַ
*				 NumByteToWrite - ���ݳ��ȣ����ֻ��256�ֽ�
*
*	�� �� ֵ: OSPI_W25Qxx_OK 		     - д���ݳɹ�
*			    W25Qxx_ERROR_WriteEnable - дʹ��ʧ��
*				 W25Qxx_ERROR_TRANSMIT	  - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ��ҳд�룬���ֻ��256�ֽڣ�������д��֮ǰ���������ɲ�������
*
*	˵    ��: 1.Flash��д��ʱ��Ͳ���ʱ��һ�������޶��ģ�������˵OSPI����ʱ��133M�Ϳ���������ٶȽ���д��
*				 2.���� W25Q64JV �����ֲ������ ҳ(256�ֽ�) д��ο�ʱ�䣬����ֵΪ 0.4ms�����ֵΪ3ms
*				 3.ʵ�ʵ�д���ٶȿ��ܴ���0.4ms��Ҳ����С��0.4ms
*				 4.Flashʹ�õ�ʱ��Խ����д������ʱ��Ҳ��Խ��
*				 5.������д��֮ǰ���������ɲ�������
*
***********************************************************************************************************/
int8_t OSPI_W25Qxx_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    OSPI_RegularCmdTypeDef  sCommand;// OSPI��������
    
    sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // ͨ������
    sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID
    
    sCommand.Instruction             = W25Qxx_CMD_QuadInputPageProgram;        // 1-1-4ģʽ��(1��ָ��1�ߵ�ַ4������)��ҳ���ָ��
    sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1��ָ��ģʽ
    sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // ָ���8λ
    sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // ��ָֹ��DTRģʽ
    
    sCommand.Address                 = WriteAddr;                              // ��ַ
    sCommand.AddressMode             = HAL_OSPI_ADDRESS_1_LINE;                // 1�ߵ�ַģʽ
    sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // ��ַ����24λ
    sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // ��ֹ��ַDTRģʽ
    
    sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // �޽����ֽ�         
    sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // ��ֹ���ֽ�DTRģʽ
    
    sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4������ģʽ
    sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // ��ֹ����DTRģʽ
    sCommand.NbData                  = NumByteToWrite;                         // ���ݳ���
    
    sCommand.DummyCycles             = 0;                                      // �����ڸ���
    sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // ��ʹ��DQS
    sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // ÿ�δ������ݶ�����ָ��   

    // дʹ��
    if (OSPI_W25Qxx_WriteEnable() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_WriteEnable;	// дʹ��ʧ��
    }
    // д����  
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// �������ݴ���
    }   
    // ��ʼ��������
    if (HAL_OSPI_Transmit(&hospi2, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Qxx_ERROR_TRANSMIT;		// �������ݴ���
    }
    // ʹ���Զ���ѯ��־λ���ȴ�д��Ľ��� 
    if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
    {
        return W25Qxx_ERROR_AUTOPOLLING;		// ��ѯ�ȴ�����Ӧ
    }
    return OSPI_W25Qxx_OK; // д���ݳɹ�
}

/**********************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_WriteBuffer
*
*	��ڲ���: pBuffer 		 - Ҫд�������
*				 WriteAddr 		 - Ҫд�� W25Qxx �ĵ�ַ
*				 NumByteToWrite - ���ݳ��ȣ�����ܳ���flashоƬ�Ĵ�С
*
*	�� �� ֵ: OSPI_W25Qxx_OK 		     - д���ݳɹ�
*			    W25Qxx_ERROR_WriteEnable - дʹ��ʧ��
*				 W25Qxx_ERROR_TRANSMIT	  - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: д�����ݣ�����ܳ���flashоƬ�Ĵ�С���������ɲ�������
*
*	˵    ��: 1.Flash��д��ʱ��Ͳ���ʱ��һ���������޶��ģ�������˵OSPI����ʱ��133M�Ϳ���������ٶȽ���д��
*				 2.���� W25Q64JV �����ֲ������ ҳ д��ο�ʱ�䣬����ֵΪ 0.4ms�����ֵΪ3ms
*				 3.ʵ�ʵ�д���ٶȿ��ܴ���0.4ms��Ҳ����С��0.4ms
*				 4.Flashʹ�õ�ʱ��Խ����д������ʱ��Ҳ��Խ��
*				 5.������д��֮ǰ���������ɲ�������
*				 6.�ú�����ֲ�� stm32h743i_eval_qspi.c
*
**********************************************************************************************************/

int8_t OSPI_W25Qxx_WriteBuffer(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t Size)
{	
    uint32_t end_addr, current_size, current_addr;
    uint8_t *write_data;  // Ҫд�������
    
    current_size = W25Qxx_PageSize - (WriteAddr % W25Qxx_PageSize); // ���㵱ǰҳ��ʣ��Ŀռ�
    
    if (current_size > Size)	// �жϵ�ǰҳʣ��Ŀռ��Ƿ��㹻д����������
    {
        current_size = Size;		// ����㹻����ֱ�ӻ�ȡ��ǰ����
    }
    
    current_addr = WriteAddr;		// ��ȡҪд��ĵ�ַ
    end_addr = WriteAddr + Size;	// ���������ַ
    write_data = pBuffer;			// ��ȡҪд�������
    
    do
    {
        // ��ҳд������
        if(OSPI_W25Qxx_WritePage(write_data, current_addr, current_size) != OSPI_W25Qxx_OK)
        {
            return W25Qxx_ERROR_TRANSMIT;
        }
    
        else // ��ҳд�����ݳɹ���������һ��д���ݵ�׼������
        {
            current_addr += current_size;	// ������һ��Ҫд��ĵ�ַ
            write_data += current_size;	// ��ȡ��һ��Ҫд������ݴ洢����ַ
            // ������һ��д���ݵĳ���
            current_size = ((current_addr + W25Qxx_PageSize) > end_addr) ? (end_addr - current_addr) : W25Qxx_PageSize;
        }
    }
    while (current_addr < end_addr) ; // �ж������Ƿ�ȫ��д�����
    
    return OSPI_W25Qxx_OK;	// д�����ݳɹ�
}

/**********************************************************************************************************************************
*
*	�� �� ��: OSPI_W25Qxx_ReadBuffer
*
*	��ڲ���: pBuffer 		 - Ҫ��ȡ������
*				 ReadAddr 		 - Ҫ��ȡ W25Qxx �ĵ�ַ
*				 NumByteToRead  - ���ݳ��ȣ�����ܳ���flashоƬ�Ĵ�С
*
*	�� �� ֵ: OSPI_W25Qxx_OK 		     - �����ݳɹ�
*				 W25Qxx_ERROR_TRANSMIT	  - ����ʧ��
*				 W25Qxx_ERROR_AUTOPOLLING - ��ѯ�ȴ�����Ӧ
*
*	��������: ��ȡ���ݣ�����ܳ���flashоƬ�Ĵ�С
*
*	˵    ��: 1.Flash�Ķ�ȡ�ٶ�ȡ����OSPI��ͨ��ʱ�ӣ�����ܳ���133M
*				 2.����ʹ�õ���1-4-4ģʽ��(1��ָ��4�ߵ�ַ4������)�����ٶ�ȡָ�� Fast Read Quad I/O
*				 3.ʹ�ÿ��ٶ�ȡָ�����п����ڵģ�����ο�W25Q64JV���ֲ�  Fast Read Quad I/O  ��0xEB��ָ��
*				 4.ʵ��ʹ���У��Ƿ�ʹ��DMA�����������Ż��ȼ��Լ����ݴ洢����λ��(�ڲ� TCM SRAM ���� AXI SRAM)����Ӱ���ȡ���ٶ�
*  FANKE
*****************************************************************************************************************FANKE************/

int8_t OSPI_W25Qxx_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
   OSPI_RegularCmdTypeDef  sCommand;// OSPI��������

   sCommand.OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;             // ͨ������
   sCommand.FlashId                 = HAL_OSPI_FLASH_ID_1;                    // flash ID

   sCommand.Instruction             = W25Qxx_CMD_FastReadQuad_IO;             // 1-4-4ģʽ��(1��ָ��4�ߵ�ַ4������)�����ٶ�ȡָ��
   sCommand.InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;            // 1��ָ��ģʽ
   sCommand.InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;            // ָ���8λ
   sCommand.InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;       // ��ָֹ��DTRģʽ

   sCommand.Address                 = ReadAddr;                               // ��ַ
   sCommand.AddressMode             = HAL_OSPI_ADDRESS_4_LINES;               // 4�ߵ�ַģʽ
   sCommand.AddressSize             = HAL_OSPI_ADDRESS_24_BITS;               // ��ַ����24λ
   sCommand.AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;           // ��ֹ��ַDTRģʽ

   sCommand.AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;          // �޽����ֽ�    
   sCommand.AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;   // ��ֹ���ֽ�DTRģʽ 

   sCommand.DataMode                = HAL_OSPI_DATA_4_LINES;                  // 4������ģʽ
   sCommand.DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;              // ��ֹ����DTRģʽ 
   sCommand.NbData                  = NumByteToRead;                          // ���ݳ���

   sCommand.DummyCycles             = 6;                                      // �����ڸ���
   sCommand.DQSMode                 = HAL_OSPI_DQS_DISABLE;                   // ��ʹ��DQS 
   sCommand.SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;           // ÿ�δ������ݶ�����ָ��   

 	// д����  
	if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		return W25Qxx_ERROR_TRANSMIT;		// �������ݴ���
	}   
	//	��������
	if (HAL_OSPI_Receive(&hospi2, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		return W25Qxx_ERROR_TRANSMIT;		// �������ݴ���
	}
	// ʹ���Զ���ѯ��־λ���ȴ����յĽ���  
	if (OSPI_W25Qxx_AutoPollingMemReady() != OSPI_W25Qxx_OK)
	{
		return W25Qxx_ERROR_AUTOPOLLING;		// ��ѯ�ȴ�����Ӧ
	}
	return OSPI_W25Qxx_OK;	// ��ȡ���ݳɹ�
}



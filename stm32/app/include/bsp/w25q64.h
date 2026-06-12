#ifndef __W25Q64_H__
#define __W25Q64_H__

#include "main.h"
#include <string.h>

/*----------------------------------------------- ���������� -------------------------------------------*/

#define OSPI_W25Qxx_OK                      0       // W25Qxxͨ������
#define W25Qxx_ERROR_INIT                   -1      // ��ʼ������
#define W25Qxx_ERROR_WriteEnable            -2      // дʹ�ܴ���
#define W25Qxx_ERROR_AUTOPOLLING            -3      // ��ѯ�ȴ���������Ӧ
#define W25Qxx_ERROR_Erase                  -4      // ��������
#define W25Qxx_ERROR_TRANSMIT               -5      // �������
#define W25Qxx_ERROR_MemoryMapped           -6      // �ڴ�ӳ��ģʽ����

#define W25Qxx_CMD_EnableReset              0x66    // ʹ�ܸ�λ
#define W25Qxx_CMD_ResetDevice              0x99    // ��λ����
#define W25Qxx_CMD_JedecID                  0x9F    // JEDEC ID  
#define W25Qxx_CMD_WriteEnable              0X06    // дʹ��

#define W25Qxx_CMD_SectorErase              0x20    // ����������4K�ֽڣ� �ο�����ʱ�� 45ms
#define W25Qxx_CMD_BlockErase_32K           0x52    // �������  32K�ֽڣ��ο�����ʱ�� 120ms
#define W25Qxx_CMD_BlockErase_64K           0xD8    // �������  64K�ֽڣ��ο�����ʱ�� 150ms
#define W25Qxx_CMD_ChipErase                0xC7    // ��Ƭ�������ο�����ʱ�� 20S

#define W25Qxx_CMD_QuadInputPageProgram     0x32    // 1-1-4ģʽ��(1��ָ��1�ߵ�ַ4������)��ҳ���ָ��ο�д��ʱ�� 0.4ms 
#define W25Qxx_CMD_FastReadQuad_IO          0xEB    // 1-4-4ģʽ��(1��ָ��4�ߵ�ַ4������)�����ٶ�ȡָ��

#define W25Qxx_CMD_ReadStatus_REG1          0X05    // ��״̬�Ĵ���1
#define W25Qxx_Status_REG1_BUSY             0x01    // ��״̬�Ĵ���1�ĵ�0λ��ֻ������Busy��־λ�������ڲ���/д������/д����ʱ�ᱻ��1
#define W25Qxx_Status_REG1_WEL              0x02    // ��״̬�Ĵ���1�ĵ�1λ��ֻ������WELдʹ�ܱ�־λ���ñ�־λΪ1ʱ��������Խ���д����

#define W25Qxx_PageSize                     256         // ҳ��С��256�ֽ�
#define W25Qxx_FlashSize                    0x800000    // W25Q64��С��8M�ֽ�
#define W25Qxx_FLASH_ID                     0Xef4017    // W25Q64 JEDEC ID
#define W25Qxx_ChipErase_TIMEOUT_MAX        100000U     // ��ʱ�ȴ�ʱ�䣬W25Q64��Ƭ�����������ʱ����100S
#define W25Qxx_Mem_Addr                     0x90000000  // �ڴ�ӳ��ģʽ�ĵ�ַ

/*----------------------------------------------- �������� ---------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

int8_t OSPI_W25Qxx_Init(void);         // W25Qxx��ʼ��
uint32_t OSPI_W25Qxx_ReadID(void);     // ��ȡ����ID

int8_t OSPI_W25Qxx_MemoryMappedMode(void);  // ��OSPI����Ϊ�ڴ�ӳ��ģʽ
    
int8_t OSPI_W25Qxx_SectorErase(uint32_t SectorAddress);        // ����������4K�ֽڣ� �ο�����ʱ�� 45ms
int8_t OSPI_W25Qxx_BlockErase_32K (uint32_t SectorAddress);    // �������  32K�ֽڣ��ο�����ʱ�� 120ms
int8_t OSPI_W25Qxx_BlockErase_64K (uint32_t SectorAddress);    // �������  64K�ֽڣ��ο�����ʱ�� 150ms��ʵ�ʽ���ʹ��64K������������ʱ�����
int8_t OSPI_W25Qxx_ChipErase (void);                           // ��Ƭ�������ο�����ʱ�� 20S

int8_t OSPI_W25Qxx_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);    // ��ҳд�룬���256�ֽ�
int8_t OSPI_W25Qxx_WriteBuffer(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t Size);            // д�����ݣ�����ܳ���flashоƬ�Ĵ�С
int8_t OSPI_W25Qxx_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);     // ��ȡ���ݣ�����ܳ���flashоƬ�Ĵ�С

#ifdef __cplusplus
}
#endif

#endif

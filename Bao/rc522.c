#include "rc522.h"
#include <string.h>

/* RC522 registers */
#define CommandReg       0x01
#define ComIEnReg        0x02
#define DivIEnReg        0x03
#define ComIrqReg        0x04
#define DivIrqReg        0x05
#define ErrorReg         0x06
#define Status2Reg       0x08
#define FIFODataReg      0x09
#define FIFOLevelReg     0x0A
#define ControlReg       0x0C
#define BitFramingReg    0x0D
#define ModeReg          0x11
#define TxControlReg     0x14
#define TxASKReg         0x15
#define RFCfgReg         0x26
#define CRCResultRegH    0x21
#define CRCResultRegL    0x22
#define TModeReg         0x2A
#define TPrescalerReg    0x2B
#define TReloadRegH      0x2C
#define TReloadRegL      0x2D
#define VersionReg       0x37

/* RC522 commands */
#define PCD_Idle         0x00
#define PCD_CalcCRC      0x03
#define PCD_Transceive   0x0C
#define PCD_SoftReset    0x0F

/* PICC commands */
#define PICC_REQA        0x26
#define PICC_HLTA        0x50
#define PICC_SEL_CL1     0x93
#define PICC_SEL_CL2     0x95
#define PICC_SEL_CL3     0x97

extern SPI_HandleTypeDef hspi1;

static void RC522_Select(void)
{
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_RESET);
}

static void RC522_Deselect(void)
{
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);
}

/* Ghi mot thanh ghi */
static void RC522_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t data[2];

    data[0] = (reg << 1) & 0x7E;
    data[1] = value;

    RC522_Select();
    HAL_SPI_Transmit(&hspi1, data, 2, 100);
    RC522_Deselect();
}

/* Doc mot thanh ghi (dung TransmitReceive de dam bao clock lien tuc) */
static uint8_t RC522_ReadReg(uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2] = {0, 0};

    tx[0] = ((reg << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    RC522_Select();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
    RC522_Deselect();

    return rx[1];
}

/* Tinh CRC_A bang phan cung RC522 */
static uint8_t RC522_CalculateCRC(
    uint8_t *data,
    uint8_t length,
    uint8_t *result)
{
    RC522_WriteReg(CommandReg, PCD_Idle);
    RC522_WriteReg(DivIrqReg, 0x04);      /* xoa co CRCIRq */
    RC522_WriteReg(FIFOLevelReg, 0x80);   /* xoa FIFO */

    for (uint8_t i = 0; i < length; i++)
    {
        RC522_WriteReg(FIFODataReg, data[i]);
    }

    RC522_WriteReg(CommandReg, PCD_CalcCRC);

    uint16_t timeout = 5000;

    while (timeout--)
    {
        if (RC522_ReadReg(DivIrqReg) & 0x04)
        {
            RC522_WriteReg(CommandReg, PCD_Idle);
            result[0] = RC522_ReadReg(CRCResultRegL);
            result[1] = RC522_ReadReg(CRCResultRegH);
            return RC522_OK;
        }
    }

    RC522_WriteReg(CommandReg, PCD_Idle);
    return RC522_ERROR;
}

/* Gui lenh va nhan du lieu tu the */
static uint8_t RC522_Transceive(
    uint8_t *sendData,
    uint8_t sendLen,
    uint8_t *backData,
    uint8_t *backLen,
    uint8_t txLastBits)
{
    RC522_WriteReg(CommandReg, PCD_Idle);
    RC522_WriteReg(ComIrqReg, 0x7F);      /* xoa tat ca co ngat */
    RC522_WriteReg(FIFOLevelReg, 0x80);   /* xoa FIFO */

    for (uint8_t i = 0; i < sendLen; i++)
    {
        RC522_WriteReg(FIFODataReg, sendData[i]);
    }

    RC522_WriteReg(BitFramingReg, txLastBits);
    RC522_WriteReg(CommandReg, PCD_Transceive);

    /* Bat dau gui du lieu (StartSend) */
    RC522_WriteReg(BitFramingReg, txLastBits | 0x80);

    uint16_t timeout = 5000;
    uint8_t irq = 0;
    uint8_t done = 0;

    while (timeout--)
    {
        irq = RC522_ReadReg(ComIrqReg);

        if (irq & 0x30)                   /* RxIRq hoac IdleIRq */
        {
            done = 1;
            break;
        }

        if (irq & 0x01)                   /* TimerIRq: the khong tra loi */
        {
            RC522_WriteReg(BitFramingReg, txLastBits);
            return RC522_NOTAG;
        }
    }

    /* Tat StartSend */
    RC522_WriteReg(BitFramingReg, txLastBits);

    if (!done)
        return RC522_ERROR;

    if (RC522_ReadReg(ErrorReg) & 0x13)   /* BufferOvfl, ParityErr, ProtocolErr */
        return RC522_ERROR;

    uint8_t fifoLevel = RC522_ReadReg(FIFOLevelReg);

    if (fifoLevel > *backLen)
        return RC522_ERROR;

    *backLen = fifoLevel;

    for (uint8_t i = 0; i < fifoLevel; i++)
    {
        backData[i] = RC522_ReadReg(FIFODataReg);
    }

    return RC522_OK;
}

/* Yeu cau the tra loi REQA */
static uint8_t RC522_RequestA(void)
{
    uint8_t command = PICC_REQA;
    uint8_t answer[2];
    uint8_t answerLen = sizeof(answer);

    uint8_t status = RC522_Transceive(
        &command, 1,
        answer, &answerLen,
        0x07
    );

    if (status != RC522_OK || answerLen != 2)
        return RC522_NOTAG;

    return RC522_OK;
}

/* Doc UID theo tung cascade level */
static uint8_t RC522_ReadCascade(
    uint8_t selectCode,
    uint8_t *part,
    uint8_t *sak)
{
    uint8_t buffer[18];
    uint8_t length = sizeof(buffer);

    /* Anti-collision */
    uint8_t anticoll[2] = {selectCode, 0x20};

    uint8_t status = RC522_Transceive(
        anticoll, 2,
        buffer, &length,
        0
    );

    if (status != RC522_OK || length != 5)
        return RC522_ERROR;

    /* Kiem tra BCC */
    if ((buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3]) != buffer[4])
        return RC522_ERROR;

    /* Select the */
    uint8_t selectBuffer[9];

    selectBuffer[0] = selectCode;
    selectBuffer[1] = 0x70;

    for (uint8_t i = 0; i < 5; i++)
    {
        selectBuffer[2 + i] = buffer[i];
    }

    if (RC522_CalculateCRC(selectBuffer, 7, &selectBuffer[7]) != RC522_OK)
        return RC522_ERROR;

    length = sizeof(buffer);

    status = RC522_Transceive(
        selectBuffer, 9,
        buffer, &length,
        0
    );

    /* Phan hoi = SAK (1 byte) + CRC_A (2 byte) */
    if (status != RC522_OK || length != 3)
        return RC522_ERROR;

    uint8_t crc[2];

    if (RC522_CalculateCRC(buffer, 1, crc) != RC522_OK)
        return RC522_ERROR;

    if (crc[0] != buffer[1] || crc[1] != buffer[2])
        return RC522_ERROR;

    *sak = buffer[0];

    for (uint8_t i = 0; i < 5; i++)
    {
        part[i] = selectBuffer[2 + i];
    }

    return RC522_OK;
}

/* Khoi tao RC522 */
void RC522_Init(void)
{
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);

    HAL_Delay(50);

    RC522_WriteReg(CommandReg, PCD_SoftReset);
    HAL_Delay(50);

    /* Cau hinh timer noi cua RC522 */
    RC522_WriteReg(TModeReg, 0x8D);
    RC522_WriteReg(TPrescalerReg, 0x3E);
    RC522_WriteReg(TReloadRegL, 30);
    RC522_WriteReg(TReloadRegH, 0);

    RC522_WriteReg(TxASKReg, 0x40);
    RC522_WriteReg(ModeReg, 0x3D);

    /* Tang do loi anten len 48 dB */
    RC522_WriteReg(RFCfgReg, 0x70);

    /* Bat song RF */
    uint8_t txControl = RC522_ReadReg(TxControlReg);

    if ((txControl & 0x03) != 0x03)
    {
        RC522_WriteReg(TxControlReg, txControl | 0x03);
    }
}

/* Reset module */
void RC522_Reset(void)
{
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50);

    RC522_Init();
}

/* Doc UID 4, 7 hoac 10 byte */
uint8_t RC522_ReadUID(uint8_t *uid, uint8_t *uid_len)
{
    if (uid == NULL || uid_len == NULL)
        return RC522_ERROR;

    *uid_len = 0;

    if (RC522_RequestA() != RC522_OK)
        return RC522_NOTAG;

    uint8_t part[5];
    uint8_t sak = 0;
    uint8_t selectCode[3] =
    {
        PICC_SEL_CL1,
        PICC_SEL_CL2,
        PICC_SEL_CL3
    };

    for (uint8_t level = 0; level < 3; level++)
    {
        if (RC522_ReadCascade(selectCode[level], part, &sak) != RC522_OK)
        {
            *uid_len = 0;
            return RC522_ERROR;
        }

        /* CT = Cascade Tag (0x88) */
        if (part[0] == 0x88)
        {
            if (level == 2)
            {
                *uid_len = 0;
                return RC522_ERROR;
            }

            uid[(*uid_len)++] = part[1];
            uid[(*uid_len)++] = part[2];
            uid[(*uid_len)++] = part[3];
        }
        else
        {
            uid[(*uid_len)++] = part[0];
            uid[(*uid_len)++] = part[1];
            uid[(*uid_len)++] = part[2];
            uid[(*uid_len)++] = part[3];
        }

        /* Bit cascade cua SAK: 0 = UID da day du */
        if ((sak & 0x04) == 0)
        {
            if (*uid_len == 4 || *uid_len == 7 || *uid_len == 10)
                return RC522_OK;

            *uid_len = 0;
            return RC522_ERROR;
        }
    }

    *uid_len = 0;
    return RC522_ERROR;
}

/* Dua the ve trang thai HALT */
void RC522_HaltA(void)
{
    uint8_t buf[4];
    uint8_t dummy[2];
    uint8_t dummyLen = sizeof(dummy);

    buf[0] = PICC_HLTA;
    buf[1] = 0x00;

    if (RC522_CalculateCRC(buf, 2, &buf[2]) != RC522_OK)
        return;

    /* The khong phan hoi lenh HLTA, bo qua ket qua tra ve */
    (void)RC522_Transceive(buf, 4, dummy, &dummyLen, 0);
}

uint8_t RC522_GetVersion(void)
{
    return RC522_ReadReg(VersionReg);
}
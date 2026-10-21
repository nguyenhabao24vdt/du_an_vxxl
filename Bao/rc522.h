#ifndef RC522_H
#define RC522_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* Cau hinh chan ket noi RC522 (CS va RST la GPIO_Output, muc mac dinh High) */
#define RC522_CS_PORT       GPIOA
#define RC522_CS_PIN        GPIO_PIN_4

#define RC522_RST_PORT      GPIOB
#define RC522_RST_PIN       GPIO_PIN_0

/* Kich thuoc toi da cua UID, mang uid phia goi phai >= gia tri nay */
#define RC522_MAX_UID_SIZE  10

/* Ma ket qua */
#define RC522_OK            0
#define RC522_NOTAG         1
#define RC522_ERROR         2

/* Khoi tao module RC522 (can goi MX_GPIO_Init va MX_SPI1_Init truoc) */
void RC522_Init(void);

/* Dat lai module RC522 bang chan RST roi khoi tao lai */
void RC522_Reset(void);

/* Doc thanh ghi VersionReg, gia tri dung thuong la 0x91 hoac 0x92 */
uint8_t RC522_GetVersion(void);

/* Doc UID:
   uid     : mang nhan UID (toi thieu RC522_MAX_UID_SIZE byte)
   uid_len : do dai UID thuc te (4, 7 hoac 10 byte)
   Tra ve  : RC522_OK neu doc thanh cong, RC522_NOTAG neu khong co the,
             RC522_ERROR neu loi giao tiep */
uint8_t RC522_ReadUID(uint8_t *uid, uint8_t *uid_len);

/* Dua the dang ACTIVE ve trang thai HALT.
   Goi sau khi doc xong UID de tranh doc lap lai cung mot the lien tuc. */
void RC522_HaltA(void);

#ifdef __cplusplus
}
#endif

#endif /* RC522_H */
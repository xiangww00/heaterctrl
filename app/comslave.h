

#ifndef __SLAVE_H__
#define __SLAVE_H__


typedef enum {
    ECODE_SUCCESS = 0,
    ECODE_NOT_SUPPORT_CMD    = 1,
    ECODE_CRC_ERROR          = 2,
    ECODE_INVALID_PARAM      = 3,
    ECODE_EXE_ERROR          = 4,

} eCode;


void ComSlave_Init(void);
void ComSlave_Service(void);

#endif



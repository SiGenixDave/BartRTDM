/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Transportation BTPC
 *****************************************************************************
 *
 *  MODULE      : RtdmFileIO.h
 *
 *  ABSTRACT    : Interface definition for resource 'RtdmFileIO'
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef RTDMFILEIO_H
#define RTDMFILEIO_H

#ifndef TARGET_SIM_DLL
#include "mwt_types.h"
#include "usertypes.h"
#endif

typedef struct dataBlock_RtdmFileIO
{
    /* Group: Default */
    BOOL              RTDMSendMessage_trig;                    /* input  */
}   TYPE_RTDMFILEIO_IF;

#ifdef __cplusplus
extern "C" {
#endif

void RtdmFileIO(TYPE_RTDMFILEIO_IF *interface);

#ifdef __cplusplus
}
#endif

#endif /* RTDMFILEIO_H */


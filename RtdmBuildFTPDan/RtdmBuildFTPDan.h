/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Transportation BTPC
 *****************************************************************************
 *
 *  MODULE      : RtdmBuildFTPDan.h
 *
 *  ABSTRACT    : Interface definition for resource 'RtdmBuildFTPDan'
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef RTDMBUILDFTPDAN_H
#define RTDMBUILDFTPDAN_H

#ifndef TARGET_SIM_DLL
#include "mwt_types.h"
#include "usertypes.h"
#endif

typedef struct dataBlock_RtdmBuildFTPDan
{
}   TYPE_RTDMBUILDFTPDAN_IF;

#ifdef __cplusplus
extern "C" {
#endif

void RtdmBuildFTPDan(TYPE_RTDMBUILDFTPDAN_IF *interface);

#ifdef __cplusplus
}
#endif

#endif /* RTDMBUILDFTPDAN_H */


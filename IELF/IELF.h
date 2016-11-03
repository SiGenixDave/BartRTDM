/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Transportation BTPC
 *****************************************************************************
 *
 *  MODULE      : IELF.h
 *
 *  ABSTRACT    : Interface definition for resource 'IELF'
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef IELF_H
#define IELF_H

#ifndef TARGET_SIM_DLL
#include "mwt_types.h"
#include "usertypes.h"
#endif

typedef struct dataBlock_IELF
{
}   TYPE_IELF_IF;

#ifdef __cplusplus
extern "C" {
#endif

void IELF(TYPE_IELF_IF *interface);

#ifdef __cplusplus
}
#endif

#endif /* IELF_H */


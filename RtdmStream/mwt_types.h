/*****************************************************************************
 * COPYRIGHT : (c) 2002-2013 Bombardier Inc. or its subsidiaries
 *****************************************************************************
 *
 * MODULE    : mwt_types.h
 *
 * ABSTRACT  : Data type definitions for MWT
 *
 * REMARKS   : only data definitions no code !
 *
 ****************************************************************************/

#ifndef MWT_TYPES_H
#define MWT_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../RtdmStream/mitraclib.h"

typedef CC_ANALOG        MWT_ANALOG;
typedef CC_BCD4          MWT_BCD4;
typedef CC_BIFRACT200    MWT_BIFRACT200;
typedef CC_BOOL          MWT_BOOL;
typedef CC_BOOLEAN2      MWT_BOOLEAN2;
typedef CC_BYTE          MWT_BYTE;
typedef CC_DATE          MWT_DATE;
typedef CC_DATE_AND_TIME MWT_DATE_AND_TIME;
typedef CC_DINT          MWT_DINT;
typedef CC_DWORD         MWT_DWORD;
typedef CC_ENUM4         MWT_ENUM4;
typedef CC_FIXED         MWT_FIXED;
typedef CC_INT           MWT_INT;
typedef CC_REAL          MWT_REAL;
typedef CC_SINT          MWT_SINT;
typedef CC_STRING        MWT_STRING;
typedef CC_TIME          MWT_TIME;
typedef CC_TIME_OF_DAY   MWT_TIME_OF_DAY;
typedef CC_TIMEDATE48    MWT_TIMEDATE48;
typedef CC_UDINT         MWT_UDINT;
typedef CC_UINT          MWT_UINT;
typedef CC_UNIFRACT      MWT_UNIFRACT;
typedef CC_USINT         MWT_USINT;
typedef CC_WORD          MWT_WORD;
typedef double           MWT_LREAL;

typedef void*            MWT_PVOID;

#ifdef __cplusplus
}
#endif

#endif /* MWT_TYPES_H */


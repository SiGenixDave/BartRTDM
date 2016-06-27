/*****************************************************************************
 *  COPYRIGHT   : (c) 2006 Bombardier Inc. or its subsidiaries
 *****************************************************************************
 *
 *  MODULE      : mitraclib.h
 *
 *  ABSTRACT    : CSS-specific run time library for IEC 1131 programs
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

/*        ********************************************
|         *                                          *
|         *        Copyright (c) 1997-1999 by        *
|         *                                          *
|         *   ABB Daimler-Benz Transportation Ltd.   *
|         *                                          *
|         *                CHTRA/BALT                *
|         *                                          *
|         ********************************************
|
|
|
|  Filename          :   capec.h
|
|  Category          :   CSS-specific run time library for IEC 1131 programs
|
|  Project           :   CAPE/C 2.3
|
|  Author            :   U.Schult (BALT)
|
|  Abstract          :   This library is used in conjunction with the
|                        IEC 1131 code generator and floating point library.
|                        It provides functions that cannot be expressed
|                        directly in the IEC 1131 languages. In particular,
|                        CSS calls and string operations are offered.
|                        This library can also be used by "black box" software.
|
|  History           :	  97-10-02 / US - created
|                         98-04-07 / US - TCN data types added
|                         99-03-30 / US - CC_BITSET256 added
|                         99-05-12 / US - DCU support added
|                         00-02-08 / US - TIMEDATE48 reused from CSS
|                         05-11-15 / fht- added: definition of INT64
|                         08-10-06 / fht- added: definition of UINT64,
|                                                modified for MSC_VER
|
|----------------------------------------------------------------------------*/

#ifndef _MITRACLIB_H
#define _MITRACLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define __RTS_FULL_VERSION \
	(RTS_VERSION*0x1000000+RTS_RELEASE*0x10000+RTS_UPDATE*0x100+RTS_EVOLUTION)

/*---------------------------------------------------------------------------
Includes
-----------------------------------------------------------------------------*/
#ifndef TARGET_SIM_DLL
  #include "rts.h"
  #if (__RTS_FULL_VERSION >= 0x03020300)
    #ifndef CSS_DEFINES_INT64_TYPES
      #define CSS_DEFINES_INT64_TYPES
    #endif
  #endif
  #include "rts_api.h"
#endif

/*---------------------------------------------------------------------------
Typedefs
-----------------------------------------------------------------------------*/

#ifdef _MSC_VER
    typedef unsigned __int64 UINT64;
    typedef signed   __int64 INT64;
#else
  #if (RTS_VERSION==1)
    typedef float   FLOAT32;
    typedef double  FLOAT64;
  #endif
  #if (__RTS_FULL_VERSION < 0x03010205)
    typedef unsigned long long UINT64;
    typedef signed long long INT64;
  #endif
#endif

typedef INT16  CC_ANALOG;
typedef UINT8  CC_BCD4;
typedef INT16  CC_BIFRACT200;
typedef UINT16 CC_BITSET256[16];
typedef UINT8  CC_BOOL;
typedef UINT8  CC_BOOLEAN2;
typedef BYTE   CC_BYTE;
typedef UINT32 CC_DATE;
typedef struct dt
{
	UINT32 date;
	UINT16 ms;
} CC_DATE_AND_TIME;
typedef INT32  CC_DINT;
typedef DWORD  CC_DWORD;
typedef UINT8  CC_ENUM4;
typedef INT32  CC_FIXED;
typedef INT16  CC_INT;
typedef float  CC_REAL;
typedef INT8   CC_SINT;
typedef char   CC_STRING[81];
typedef INT32  CC_TIME;
typedef OS_TIMEDATE48 CC_TIMEDATE48;
typedef INT32  CC_TIME_OF_DAY;
typedef UINT32 CC_UDINT;
typedef UINT16 CC_UINT;
typedef UINT16 CC_UNIFRACT;
typedef UINT8  CC_USINT;
typedef WORD   CC_WORD;

typedef UINT8  BOOLEAN8;
typedef UINT16 MT_UNICODE16;
typedef char   MT_CHAR8;

#ifdef __cplusplus
}
#endif

/*--------------------------------------------------------------------------*/
#endif   /* _MITRACLIB_H */


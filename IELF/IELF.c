/*****************************************************************************/
/* This document and its contents are the property of Bombardier
 * Inc or its subsidiaries.  This document contains confidential
 * proprietary information.  The reproduction, distribution,
 * utilization or the communication of this document or any part
 * thereof, without express authorization is strictly prohibited.
 * Offenders will be held liable for the payment of damages.
 *
 * (C) 2016, Bombardier Inc. or its subsidiaries.  All rights reserved.
 *
 * Project      :  RTDM (Embedded)
 *//**
 * \file RtdmStream.c
 *//*
 *
 * Revision: 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
typedef struct
{
    UINT8 subsystemId;
    UINT16 eventId;
    UINT16 eventCount;
    UINT8 overflowFlag;
    UINT8 rateLimitFlag;
    UINT8 _reserved[3];
} EventCounterStr;

typedef struct
{
    UINT32 failureBeginning;
    UINT32 failureEnd;
    UINT8 subsystemId;
    UINT16 eventId;
    UINT8 timeInacuurate;
    UINT8 dstFlag;
    UINT8 _reserved[2];
} EventStr;

typedef struct
{
    char version[4];
    UINT8 systemId;
    UINT16 numberOfRecords;
    INT16 firstRecordIndex;
    INT16 lastRecordIndex;
    UINT32 timeOfLastReset;
    UINT8 reasonForReset;
    UINT32 _reserved;
    EventCounterStr eventCounter[1024];
    EventStr event[2100];
};

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Holds the current sample count; incremented every 50 msecs if data has been placed in the stream buffer */
static UINT16 m_X = 0;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void IelfInit(void)
{
    // Determine if IELF file exists;

    // If not, create it using default values

    // If it does, verify contents

}

INT32 InsertEvent(UINT16 eventId)
{


}


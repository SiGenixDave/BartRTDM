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
 * \file IELFCallback.c
 *//*
 *
 * Revision: 01OCT2016 - D.Smail : Original Release
 *
 *****************************************************************************/

/* This file exists because this feature must support the ability to detect the end of an
 * event in case of a power cycle or reset while an event is active.
 */

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

#include "../IELF/IELF.h"

typedef struct
{
    UINT16 eventCode;
    EventOverCallback callback;

} IELFCallbackMap;

#if !TEST_ON_PC
static IELFCallbackMap ielfCallbackMap[] =
{
    {   1, x},

};
#else
static IELFCallbackMap ielfCallbackMap[] =
    {
        { 1, Sim1EventOver },
          { 2, Sim2EventOver }, };
#endif

EventOverCallback GetIELFCallback (UINT16 eventCode)
{
    EventOverCallback callback = NULL;
    UINT16 index = 0;

    while (index < sizeof(ielfCallbackMap) / sizeof(IELFCallbackMap))
    {
        if (ielfCallbackMap[index].eventCode == eventCode)
        {
            callback = ielfCallbackMap[index].callback;
            break;
        }
        index++;
    }

    return (callback);

}


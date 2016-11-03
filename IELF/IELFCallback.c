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
 * Revision: 01DEC2016 - D.Smail : Original Release
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

#include "IELFTask.h"

/** @brief maps the event Id to the callback function that determines when the event conditions
 * are no longer present */
typedef struct
{
    /** event Id */
    UINT16 eventCode;
    /** callback function associated with the eventCode */
    EventOverCallback callback;

} IELFCallbackMap;

#ifndef TEST_ON_PC
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

/*****************************************************************************/
/**
 * @brief       Gets the callback function for the selected event
 *
 *              This function scans the event callback list and retrieves
 *              the callback function used to determine when the event
 *              conditions are no longer present.
 *
 * @param eventCode - the event id
 *
 * @returns EventOverCallback - callback function used to determine when event conditions
 *                              are no longer present
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
EventOverCallback GetIELFCallback (UINT16 eventCode)
{
    EventOverCallback callback = NULL;  /* default return value, calling function should check for NULL */
    UINT16 index = 0;   /* loop index */

    /* Scan through the map */
    while (index < sizeof(ielfCallbackMap) / sizeof(IELFCallbackMap))
    {
        if (ielfCallbackMap[index].eventCode == eventCode)
        {
            /* Callback found */
            callback = ielfCallbackMap[index].callback;
            break;
        }
        index++;
    }

    return (callback);

}


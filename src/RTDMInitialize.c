/*
 * RTDMInitialize.c
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */


#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include <stdio.h>
#include "MyTypes.h"
#include "usertypes.h"
#endif

#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmUtils.h"
#include "RtdmDataLog.h"
#include "RtdmFileIO.h"

void InitializeRtdmStream (RtdmXmlStr *rtdmXmlData);


void TestDateTime(void)
{
#ifndef TEST_ON_PC
    RTDMTimeStr rtdmTime;
    OS_STR_TIME_ANSI ansiTime;
char dateTime[200];

    GetEpochTime (&rtdmTime);

    os_c_localtime (rtdmTime.seconds, &ansiTime); /* OUT: Local time */

    sprintf (dateTime, "%02d%02d%02d-%02d%02d%02d", ansiTime.tm_year % 100, ansiTime.tm_mon + 1, ansiTime.tm_mday,
                    ansiTime.tm_hour, ansiTime.tm_min, ansiTime.tm_sec);


    debugPrintf(DBG_INFO, "ANSI Date time = %s", dateTime);

#endif

}

void RTDMInitialize (TYPE_RTDMSTREAM_IF *interface)
{

    RtdmXmlStr *rtdmXmlData = NULL;

    /* Read XML file and update all XML and interface parameters. This call must be
     * performed first because other initialization functions use parameters read from the
     * XML configuration file */
    InitializeXML (interface, &rtdmXmlData);

    InitializeRtdmStream (rtdmXmlData);

    InitializeDataLog (rtdmXmlData);

    InitializeFileIO (interface, rtdmXmlData);

}


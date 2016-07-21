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

#include "RtdmUtils.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmDataLog.h"
#include "RtdmFileIO.h"

void InitializeRtdmStream (RtdmXmlStr *rtdmXmlData);


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


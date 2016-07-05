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
#include "MyTypes.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "RTDM_Stream_ext.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmDataLog.h"

void RTDMInitialize(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{

	// Read XML file
    InitializeXML(interface, rtdmXmlData);

	InitializeRtdmStream(rtdmXmlData);

	InitializeDataLog(interface, rtdmXmlData);

}


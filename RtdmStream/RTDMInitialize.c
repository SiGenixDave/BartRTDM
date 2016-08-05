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
 * \file RtdmInitialize.c
 *//*
 *
 * Revision: 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/


#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include <stdio.h>
#include "../RtdmStream/MyTypes.h"
#include "../RtdmStream/usertypes.h"
#endif

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmDataLog.h"
#include "../RtdmFileIO/RtdmFileIO.h"


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

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/

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


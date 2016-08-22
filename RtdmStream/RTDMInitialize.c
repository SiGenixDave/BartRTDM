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
#else
#include <stdio.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmDataLog.h"
#include "../RtdmFileIO/RtdmFileExt.h"
#include "../RtdmStream/RtdmStreamExt.h"


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

/*****************************************************************************/
/**
 * @brief       Responsible for initializing the RTDM functionality
 *
 *              This function is called at startup and executes on the event driven
 *              file I/O task (low priority). It calls all functions required to initialize
 *              the RTDM functionality. It is intentionally called on a low priority task due
 *              to all of the file operations performed during initialization.
 *
 *  @param interface - pointer to the RTDM stream interface (MTPE tool creates this structure)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void RtdmInitializeAllFunctions (TYPE_RTDMSTREAM_IF *interface)
{

    RtdmXmlStr *rtdmXmlData = NULL;

    /* Read XML file and update all XML and interface parameters. This call must be
     * performed first because other initialization functions use parameters read from the
     * XML configuration file */
    InitializeXML (interface, &rtdmXmlData);

    InitializeRtdmStream (rtdmXmlData);

    InitializeDataLog (rtdmXmlData);

    InitializeFileIO (interface, rtdmXmlData);

    /* Inform the 50 msec RTDM stream task that initialization is complete */
    SetRtdmInitFinished();

}


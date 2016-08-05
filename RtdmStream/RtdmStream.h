/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Transportation BTPC
 *****************************************************************************
 *
 *  MODULE      : RtdmStream.h
 *
 *  ABSTRACT    : Interface definition for resource 'RtdmStream'
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef RTDMSTREAM_H
#define RTDMSTREAM_H

#ifndef TARGET_SIM_DLL
#include "../RtdmStream/mwt_types.h"
#include "../RtdmStream/usertypes.h"
#endif

typedef struct dataBlock_RtdmStream
{
    /* Group: INPUT */
    DS_8805001        oPCU_I1;                                 /* input ECN Output 880500100 */
    MWT_UINT          RTCTimeAccuracy;                         /* input  */
    MWT_STRING        VNC_CarData_X_ConsistID;                 /* input Consist ID */
    MWT_STRING        VNC_CarData_X_CarID;                     /* input Car ID */
    MWT_STRING        VNC_CarData_X_DeviceID;                  /* input Device ID */
    MWT_BOOL          VNC_CarData_S_WhoAmISts;                 /* input Who Am I Status */
    UINT8             RTDMDataLogStop;                         /* input Stop the RTDM Data Log */
    /* Group: OUTPUT */
    UINT8             RTDMDataLogState;                        /* output State of RTDM Data Log, RUN, STOP, RESTART, FULL */
    UINT16            RTDMMainBuffCount;                       /* output MD data stream message */
    UINT32            RTDM_Send_Counter;                       /* output counter for number of messages sent */
    BOOL              RTDMSendMessage_trig;                    /* output trigger to send MD data stream */
    UINT16            RTDMSignalCount;                         /* output number of signals read from config.xml */
    UINT16            RTDMSampleSize;                          /* output size of the current sample */
    UINT16            RTDMMainBuffSize;                        /* output Main buffer size [n] */
    UINT16            RTDMSendTime;                            /* output Max time before sending stream */
    UINT16            RTDMStreamError;                         /* output Error Code for Stream */
    UINT16            RTDMSampleCount;                         /* output Number of samples in stream */
}   TYPE_RTDMSTREAM_IF;

#ifdef __cplusplus
extern "C" {
#endif

void RtdmStream(TYPE_RTDMSTREAM_IF *interface);

#ifdef __cplusplus
}
#endif

#endif /* RTDMSTREAM_H */


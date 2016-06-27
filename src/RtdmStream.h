/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Transportation BTPC
 *****************************************************************************
 *
 *  MODULE      : RTDM_Stream.h
 *
 *  ABSTRACT    : Interface definition for resource 'RTDM_Stream'
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef RTDM_STREAM_H
#define RTDM_STREAM_H

#ifdef TEST_ON_PC
#include "MyTypes.h"
#include "mwt_types.h"
#include "usertypes.h"
#endif


#ifndef TARGET_SIM_DLL
#include "mwt_types.h"
#include "usertypes.h"
#endif


//DAS Autogenerated from MTPE
typedef struct dataBlock_RTDM_Stream
{

    /* Group: INPUTS */ //DAS coming from MTPE
    DS_8805001        oPCU_I1;                                 /* input ECN Output 880500100 */
    MWT_UINT          RTCTimeAccuracy;                         /* input  */
    MWT_STRING        VNC_CarData_X_ConsistID;                 /* input Consist ID */
    MWT_STRING        VNC_CarData_X_CarID;                     /* input Car ID */
    MWT_STRING        VNC_CarData_X_DeviceID;                  /* input Device ID */
    MWT_BOOL          VNC_CarData_S_WhoAmISts;                 /* input Who Am I Status */
    UINT8             RTDMDataLogStop;                         /* input Stop the RTDM Data Log */
    /* Group: OUTPUTS */ //DAS going to MTPE
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
}   TYPE_RTDM_STREAM_IF;

typedef struct
{
    uint16_t IBufferSize		__attribute__ ((packed));
    uint8_t header[85]			__attribute__ ((packed));
    uint8_t IBufferArray[]		__attribute__ ((packed));
} RTDMStream_str;



#ifdef __cplusplus
extern "C" {
#endif

void RTDM_Stream(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData);

#ifdef __cplusplus
}
#endif

#endif /* RTDM_STREAM_H */


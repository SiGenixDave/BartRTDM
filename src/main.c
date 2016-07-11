/*
 ============================================================================
 Name        : BART_RTDM.c
 Author      : DAS
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include "MyTypes.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RTDMInitialize.h"
#include "RTDM_Stream_ext.h"
#include "MySleep.h"

TYPE_RTDM_STREAM_IF mStreamInfo;
extern RtdmXmlStr m_RtdmXmlData;
StreamHeaderStr STRM_Header;

#if 0
53 54 52 4d 00 00 55 ba d7 7b 19 02 64 33 39 31
31 00 00 00 00 00 00 00 00 00 00 00 64 33 39 31
31 00 00 00 00 00 00 00 00 00 00 00 63 64 70 31
00 00 00 00 00 00 00 00 00 00 00 00 00 0b 00 01
56 c4 91 c9 02 22 01 56 c4 91 c9 02 22 00 9b de
21 84 28 00 0b

typedef struct
{
    char Delimiter[4];
    uint8_t Endiannes;
    uint16_t Header_Size __attribute__ ((packed));
    uint32_t Header_Checksum __attribute__ ((packed));
    uint8_t Header_Version;
    uint8_t Consist_ID[16];
    uint8_t Car_ID[16];
    uint8_t Device_ID[16];
    uint16_t Data_Record_ID __attribute__ ((packed));
    uint16_t Data_Record_Version __attribute__ ((packed));
    uint32_t TimeStamp_S __attribute__ ((packed));
    uint16_t TimeStamp_mS __attribute__ ((packed));
    uint8_t TimeStamp_accuracy;
    uint32_t MDS_Receive_S __attribute__ ((packed));
    uint16_t MDS_Receive_mS __attribute__ ((packed));
    uint16_t Sample_Size_for_header __attribute__ ((packed));
    uint32_t Sample_Checksum __attribute__ ((packed));
    uint16_t Num_Samples __attribute__ ((packed));
} StreamHeaderStr;



memcpy (&STRM_Header.Consist_ID, m_Interface1Ptr->VNC_CarData_X_ConsistID,
                sizeof(STRM_Header.Consist_ID));

/* Car ID */
memcpy (&STRM_Header.Car_ID, m_Interface1Ptr->VNC_CarData_X_CarID,
                sizeof(STRM_Header.Car_ID));

/* Device ID */
memcpy (&STRM_Header.Device_ID, m_Interface1Ptr->VNC_CarData_X_DeviceID,
                sizeof(STRM_Header.Device_ID));
#endif

// Comment for commit
int main (void)
{

    mStreamInfo.VNC_CarData_S_WhoAmISts = TRUE;

    /* Consist ID */
    memcpy (&mStreamInfo.VNC_CarData_X_ConsistID, "d1234",
                    sizeof(STRM_Header.Consist_ID));

    /* Car ID */
    memcpy (&mStreamInfo.VNC_CarData_X_CarID, "d1234",
                    sizeof(STRM_Header.Car_ID));

    /* Device ID */
    memcpy (&mStreamInfo.VNC_CarData_X_DeviceID, "pcux",
                    sizeof(STRM_Header.Device_ID));

    setvbuf (stdout, NULL, _IONBF, 0);
    setvbuf (stderr, NULL, _IONBF, 0);
#ifdef _DEBUG
    setbuf(stdout,NULL); // this disables buffering for stdout.
#endif

    RTDMInitialize (&mStreamInfo, &m_RtdmXmlData);

    while (TRUE)
    {
        RTDM_Stream (&mStreamInfo, &m_RtdmXmlData);
        MySleep (50);
        mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
    }

    puts ("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}

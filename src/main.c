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
#include <string.h>
#include <conio.h>

#include "MyTypes.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RTDMInitialize.h"
#include "RTDM_Stream_ext.h"
#include "MySleep.h"

TYPE_RTDM_STREAM_IF mStreamInfo;
extern RtdmXmlStr m_RtdmXmlData;


extern void CreateUIThread (void);




// Comment for commit
int main (void)
{

    mStreamInfo.VNC_CarData_S_WhoAmISts = TRUE;

    /* Consist ID */
    strcpy (mStreamInfo.VNC_CarData_X_ConsistID, "d1234");

    /* Car ID */
    strcpy (mStreamInfo.VNC_CarData_X_CarID, "d1234");

    /* Device ID */
    strcpy (mStreamInfo.VNC_CarData_X_DeviceID, "pcux");

    setvbuf (stdout, NULL, _IONBF, 0);
    setvbuf (stderr, NULL, _IONBF, 0);
    setvbuf (stdin, NULL, _IONBF, 0);
#ifdef _DEBUG
    setbuf(stdout,NULL); // this disables buffering for stdout.
#endif

    printf("Hello World\n");
    RTDMInitialize (&mStreamInfo, &m_RtdmXmlData);
    CreateUIThread();

    while (TRUE)
    {
        RTDM_Stream (&mStreamInfo, &m_RtdmXmlData);
        MySleep (50);
        mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
    }

    puts ("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}

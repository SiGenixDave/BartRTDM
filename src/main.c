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
extern RtdmXmlStr RtdmXmlData;
RTDMStream_str RTDMStreamData;
STRM_Header_Struct STRM_Header;
RTDM_Header_Struct RTDM_Header_Array[1];

int main (void)
{

    mStreamInfo.VNC_CarData_S_WhoAmISts = TRUE;

    setvbuf (stdout, NULL, _IONBF, 0);
    setvbuf (stderr, NULL, _IONBF, 0);
#ifdef _DEBUG
    setbuf(stdout,NULL); // this disables buffering for stdout.
#endif

    RTDMInitialize (&mStreamInfo, &RtdmXmlData);

    //TODO Need to place in 50 msec loop
    while (TRUE)
    {
        RTDM_Stream (&mStreamInfo, &RtdmXmlData);
        MySleep (50);
    }

    puts ("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}

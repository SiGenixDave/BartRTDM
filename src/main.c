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
#include "MyFuncs.h"
#include "usertypes.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RTDMInitialize.h"
#include "MySleep.h"


TYPE_RTDMSTREAM_IF mStreamInfo;



extern void CreateUIThread (void);


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
    //RTDMInitialize (&mStreamInfo);
    //DAS can't single step debug when another thread is spawned
    //CreateUIThread();

    while (TRUE)
    {
        RtdmStream (&mStreamInfo);
        MySleep (50);
        //mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
    }

    puts ("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}

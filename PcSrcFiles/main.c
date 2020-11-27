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

#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#include "../PcSrcFiles/MySleep.h"

#include "../IELF/IELF.h"
#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RTDMInitialize.h"

TYPE_RTDMSTREAM_IF mStreamInfo;

extern void CreateUIThread (void);
int debug;
char fileName[15];

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
    setbuf(stdout,NULL); /* this disables buffering for stdout. */
#endif

    printf ("Hello World\n");

    /* RTDMInitialize (&mStreamInfo); */
    /* DAS can't single step debug when another thread is spawned */
#if 0
    CreateUIThread();
#endif

    srand(time(NULL));

    while (TRUE)
    {
        int r = rand();

        RtdmStream (&mStreamInfo);
        IELF (NULL);
        MySleep (50);

        r %= 5;

        switch (r)
        {
            case 0:
                mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
                break;

            case 1:
                mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
                mStreamInfo.oPCU_I1.Analog801.ILineVoltage += 2;
                break;

            case 2:
                mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
                mStreamInfo.oPCU_I1.Analog801.ILineVoltage += 2;
                mStreamInfo.oPCU_I1.Analog801.IRate += 3;
                break;

            case 3:
            default:
                mStreamInfo.oPCU_I1.Analog801.ICarSpeed++;
                mStreamInfo.oPCU_I1.Analog801.ILineVoltage += 2;
                mStreamInfo.oPCU_I1.Analog801.IRate += 3;
                mStreamInfo.oPCU_I1.Analog801.IDiffCurr += 4;
                break;


        }

    }

    puts ("!!!Shouldn't get here!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}

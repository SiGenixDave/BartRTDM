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
#include "RtdmXml.h"
#include "RtdmStream.h"
#include "RTDMInitialize.h"
#include "RTDM_Stream_ext.h"

TYPE_RTDM_STREAM_IF mStreamInfo;
RtdmXmlStr RtdmXmlData;
RTDMStream_str RTDMStreamData;
RTDM_Struct RTDM_Sample_Array[1];
STRM_Header_Struct STRM_Header_Array[1];
RTDM_Header_Struct RTDM_Header_Array[1];


int main(void) {

	mStreamInfo.VNC_CarData_S_WhoAmISts = TRUE;


	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	#ifdef _DEBUG
	setbuf(stdout,NULL); // this disables buffering for stdout.
	#endif


	RTDMInitialize(&mStreamInfo, &RtdmXmlData);

	//TODO Need to place in 50 msec loop
	while (TRUE)
		RTDM_Stream(&mStreamInfo, &RtdmXmlData);

	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}

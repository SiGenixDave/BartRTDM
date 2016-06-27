/*
 * RtdmXml.h
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */

#include "RTDM_Stream_ext.h"

#ifndef RTDMXML_H_
#define RTDMXML_H_

/* Structure to contain all variables read from RTDM_config.xml file */
typedef struct
{
  int16_t   DataRecorderCfgID;
  int16_t   DataRecorderCfgVersion;
  uint16_t	SamplingRate; /*Not currently used - fixed at 50mS */
  uint8_t	Compression_enabled;
  uint16_t  MinRecordingRate;
  uint8_t   DataLogFileCfg_enabled;
  uint16_t  FilesCount;
  uint16_t	NumberSamplesInFile;
  uint8_t	FilesFullPolicy;
  uint16_t	NumberSamplesBeforeSave;
  uint16_t  MaxTimeBeforeSaveMs;
  uint8_t   OutputStream_enabled;
  uint32_t  comId;
  uint16_t  bufferSize;
  uint16_t  maxTimeBeforeSendMs;
  uint16_t  signal_id_num[MAX_PCU_SIGNALS];		/* unique ID number for each signal */
  int16_t   signal_id_size[MAX_PCU_SIGNALS];	/* size in bytes of signal */
  int16_t   sample_size; 						/* calculated size of sample including the sample header */
  uint16_t  max_main_buffer_count; 				/* calculated size of main buffer (max number of samples) */
  uint16_t  signal_count; 						/* number of signals */
} RtdmXmlStr;


int ReadXmlFile(RtdmXmlStr *rtdmXmlData);

#endif /* RTDMXML_H_ */

/*
 * RtdmXml.h
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */

#ifndef RTDMXML_H_
#define RTDMXML_H_

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
/** @brief */
typedef enum
{
    /** */
    UINT8_XML_TYPE,
    /** */
    INT8_XML_TYPE,
    /** */
    UINT16_XML_TYPE,
    /** */
    INT16_XML_TYPE,
    /** */
    UINT32_XML_TYPE,
    /** */
    INT32_XML_TYPE
} XmlSignalType;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/** @brief */
typedef struct
{
    /** */
    UINT16 id;
    /** */
    void *variableAddr;
    /** */
    XmlSignalType signalType;
} SignalDescription;

/** @brief Contains all variables read from RTDM_config.xml file as well as calculated values */
typedef struct tRtdmXmlStr
{
    /** */
    UINT16 DataRecorderCfgID;
    /** */
    UINT16 DataRecorderCfgVersion;
    /** */
    UINT16 SamplingRate; /*Not currently used - fixed at 50mS */
    /** */
    UINT8 Compression_enabled;
    /** */
    UINT16 MinRecordingRate;
    /** */
    UINT8 DataLogFileCfg_enabled;
    /** */
    UINT16 FilesCount;
    /** */
    UINT16 NumberSamplesInFile;
    /** */
    UINT8 FilesFullPolicy;
    /** */
    UINT16 NumberSamplesBeforeSave;
    /** */
    UINT16 MaxTimeBeforeSaveMs; /* max time between stream samples if data hasn't changed */
    /** */
    UINT8 OutputStream_enabled;
    /** */
    UINT32 comId;
    /** */
    UINT16 bufferSize;
    /** */
    UINT16 maxTimeBeforeSendMs;
    /** */
    SignalDescription *signalDesription; /* allocated at runtime based on the number of signals discovered */
    /** */
    UINT16 dataAllocationSize;
    /** */
    UINT16 sample_size; /* calculated size of sample including the sample header */
    /** */
    UINT16 max_main_buffer_count; /* calculated size of main buffer (max number of samples) */
    /** */
    UINT16 signal_count; /* number of signals */
} RtdmXmlStr;

/* Forward declaration required to avoid compiler error */
#ifndef RTDMSTREAM_H
struct dataBlock_RtdmStream;
typedef struct dataBlock_RtdmStream TYPE_RTDMSTREAM_IF;
#endif
/*******************************************************************
 *
 *    E  X  T  E  R  N      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    E  X  T  E  R  N      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/

UINT16 InitializeXML (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr **rtdmXmlData);
char *GetXMLConfigFileBuffer (void);

#endif /* RTDMXML_H_ */

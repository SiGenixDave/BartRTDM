/*****************************************************************************/
/* This document and its contents are the property of Bombardier
 * Inc or its subsidiaries.  This document contains confidential
 * proprietary information.  The reproduction, distribution,
 * utilization or the communication of this document or any part
 * thereof, without express authorization is strictly prohibited.
 * Offenders will be held liable for the payment of damages.
 *
 * (C) 2016, Bombardier Inc. or its subsidiaries.  All rights reserved.
 *
 * Project    : Communication Controller (Embedded)
 *//**
 * @file RtdmXmlh
 *//*
 *
 * Revision : 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

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
} SignalDescriptionStr;

typedef struct
{
    UINT32 id;
    UINT32 version;
    char *deviceId;
    char *name;
    char *description;
    char *type;
    UINT32 samplingRate;
    BOOL compressionEnabled;
    UINT32 minRecordingRate;
    UINT32 noChangeFailurePeriod;

} XmlDataRecorderCfgStr;

typedef struct
{
    BOOL enabled;
    UINT32 filesCount;
    UINT32 numberSamplesInFile;
    char *filesFullPolicy;
    UINT32 numberSamplesBeforeSave;
    UINT32 maxTimeBeforeSaveMs;
    char *folderPath;

} XmlDataLogFileCfgStr;

typedef struct
{
    BOOL enabled;
    UINT32 comId;
    UINT32 bufferSize;
    UINT32 maxTimeBeforeSendMs;
} XmlOutputStreamCfgStr;

typedef struct
{
    UINT32 maxStreamHeaderDataSize;
    UINT32 maxStreamDataSize;
    UINT32 signalCount;

} XmlMetaDataStr;

typedef struct
{
    /** */
    XmlDataRecorderCfgStr dataRecorderCfg;
    /** */
    XmlDataLogFileCfgStr dataLogFileCfg;
    /** */
    XmlOutputStreamCfgStr outputStreamCfg;
    /** */
    XmlMetaDataStr metaData;
    /** allocated at runtime based on the number of signals discovered in the configuration file */
    SignalDescriptionStr *signalDesription;

} RtdmXmlStr;


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

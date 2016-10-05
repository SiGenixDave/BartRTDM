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
 * Project      :  RTDM (Embedded)
 *//**
 * \file RtdmXml.c
 *//*
 *
 * Revision: 01OCT2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmStream.h"
#include "../RtdmFileIO/RtdmFileExt.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/** @brief XML file name on VCUC */
#define RTDM_XML_FILE           "RTDMConfiguration_PCU.xml"

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
/** @brief All possible XML attribute data types. Determines how the attribute
 * values are processed */
typedef enum
{
    /** Attribute value is either "TRUE" or "FALSE" */
    BOOLEAN_DTYPE,
    /** Attribute value is a signed integer */
    I32_DTYPE,
    /** Attribute value is an unsigned integer */
    U32_DTYPE,
    /** Attribute value is a string */
    STRING_DTYPE,
    /** TODO */
    FILESFULL_DTYPE
} XmlDataType;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/** @brief Maps an XML signal data type used for storing the signal properly
 * in the stream */
typedef struct
{
    /** String in XML file that contains the data type for an XML signal */
    const char *dataType;
    /** Handles storing the data value for the signal */
    XmlSignalType signalType;

} DataTypeMapStr;

/** @brief Maps the XML signal name to the variable's address in memory */
typedef struct
{
    /** Variable's signal name in the XML file */
    const char *signalName;
    /** Variable's address */
    void *variableAddr;
} VariableMapStr;

/** @brief Describes in full the XML configuration parameter */
typedef struct
{
    /** The name of the attribute */
    const char *name;
    /** The data type of the attribute */
    const XmlDataType dataType;
    /** The location in memory where the attribute value is stored */
    void *xmlData;
    /** The error code returned if any errors were encountered while processing the
     *  configuration parameter */
    const UINT16 errorCode;
} XmlAttributeDataStr;

typedef struct
{
    /** The name of the element */
    const char *elementName;
    /** Pointer to all attribute related data */
    const XmlAttributeDataStr *xmlAttibuteData;
    /** */
    const UINT16 numAttributes;

} XmlElementDataStr;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Pointer to memory where the entire XML configuration file is stored */
static char *m_ConfigXmlBufferPtr = NULL;

/** @brief XML configuration file size */
static INT32 m_ConfigXmlFileSize = 0;

/** @brief Maps the XML data type to the local storage data type */
static const DataTypeMapStr dataTypeMap[] =
    {
        { "UINT8", UINT8_XML_TYPE },
          { "INT8", INT8_XML_TYPE },
          { "UINT16", UINT16_XML_TYPE },
          { "INT16", INT16_XML_TYPE },
          { "UINT32", UINT32_XML_TYPE },
          { "INT32", INT32_XML_TYPE }, };

/** @brief Stores all of the XML configuration data */
static RtdmXmlStr m_RtdmXmlData;

/** @brief Maps all XML signal names to the memory location where the signal is read. The variable
 * addresses are populated at runTime since they are declared external to this module and thus
 * are initialized to NULL */
static VariableMapStr m_VariableMap[] =
    {
        { "oPCU_I1.PCU_I1.Analog801.CTractEffortReq", NULL },
          { "oPCU_I1.PCU_I1.Analog801.ICarSpeed", NULL },
          { "oPCU_I1.PCU_I1.Analog801.IDcLinkCurr", NULL },
          { "oPCU_I1.PCU_I1.Analog801.IDcLinkVoltage", NULL },
          { "oPCU_I1.PCU_I1.Analog801.IDiffCurr", NULL },
          { "oPCU_I1.PCU_I1.Analog801.ILineVoltage", NULL },
          { "oPCU_I1.PCU_I1.Analog801.IRate", NULL },
          { "oPCU_I1.PCU_I1.Analog801.IRateRequest", NULL },
          { "oPCU_I1.PCU_I1.Analog801.ITractEffortDeli", NULL },
          { "oPCU_I1.PCU_I1.Counter801.IOdometer", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.CHscbCmd", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.CRunRelayCmd", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.CScContCmd", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IDynBrkCutOut", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IMCSSModeSel", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IPKOStatus", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IPKOStatusPKOnet", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IPropCutout", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IPropSystMode", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IRegenCutOut", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.ITractionSafeSts", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.PRailGapDet", NULL },
          { "oPCU_I1.PCU_I1.Discrete801.IDcuState", NULL },
          { "oPCU_I1.PCU_I1.Analog801.ILineCurr", NULL } };

/** @brief Maps all XML configuration parameter names to the data type, memory location,
 * and error code */
static const XmlAttributeDataStr m_DataRecorderCfgAttributes[] =
    {
        { "id", U32_DTYPE, &m_RtdmXmlData.dataRecorderCfg.id, NO_CONFIG_ID },
          { "version", U32_DTYPE, &m_RtdmXmlData.dataRecorderCfg.version,
          NO_VERSION },
          { "deviceId", STRING_DTYPE, &m_RtdmXmlData.dataRecorderCfg.deviceId, NO_NEED_DEFINE },
          { "name", STRING_DTYPE, &m_RtdmXmlData.dataRecorderCfg.name,
          NO_NEED_DEFINE },
          { "description", STRING_DTYPE, &m_RtdmXmlData.dataRecorderCfg.description,
          NO_NEED_DEFINE },
          { "type", STRING_DTYPE, &m_RtdmXmlData.dataRecorderCfg.type,
          NO_NEED_DEFINE },
          { "samplingRate", U32_DTYPE, &m_RtdmXmlData.dataRecorderCfg.samplingRate,
          NO_SAMPLING_RATE },
          { "compressionEnabled", BOOLEAN_DTYPE, &m_RtdmXmlData.dataRecorderCfg.compressionEnabled,
          NO_COMPRESSION_ENABLED },
          { "minRecordingRate", U32_DTYPE, &m_RtdmXmlData.dataRecorderCfg.minRecordingRate,
          NO_MIN_RECORD_RATE },
          { "noChangeFailurePeriod", U32_DTYPE,
            &m_RtdmXmlData.dataRecorderCfg.noChangeFailurePeriod,
            NO_NO_CHANGE_FAILURE_PERIOD } };

/** @brief */
static const XmlAttributeDataStr m_DataLogFileCfgAttributes[] =
    {
        { "enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.dataLogFileCfg.enabled, NO_DATALOG_CFG_ENABLED },
          { "filesCount", U32_DTYPE, &m_RtdmXmlData.dataLogFileCfg.filesCount,
          NO_FILES_COUNT },
          { "numberSamplesInFile", U32_DTYPE, &m_RtdmXmlData.dataLogFileCfg.numberSamplesInFile,
          NO_NUM_SAMPLES_IN_FILE },
          { "filesFullPolicy", STRING_DTYPE, &m_RtdmXmlData.dataLogFileCfg.filesFullPolicy,
          NO_FILE_FULL_POLICY },
          { "numberSamplesBeforeSave", U32_DTYPE,
            &m_RtdmXmlData.dataLogFileCfg.numberSamplesBeforeSave,
            NO_NUM_SAMPLES_BEFORE_SAVE },
          { "maxTimeBeforeSaveMs", U32_DTYPE, &m_RtdmXmlData.dataLogFileCfg.maxTimeBeforeSaveMs,
          NO_MAX_TIME_BEFORE_SAVE },
          { "folderPath", STRING_DTYPE, &m_RtdmXmlData.dataLogFileCfg.folderPath,
          NO_NEED_DEFINE }, };

/** @brief */
static const XmlAttributeDataStr m_OutputStreamCfgAttributes[] =
    {
        { "enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.outputStreamCfg.enabled, NO_DATALOG_CFG_ENABLED },
          { "comId", U32_DTYPE, &m_RtdmXmlData.outputStreamCfg.comId,
          NO_COMID },
          { "bufferSize", U32_DTYPE, &m_RtdmXmlData.outputStreamCfg.bufferSize, NO_BUFFERSIZE },
          { "maxTimeBeforeSendMs", U32_DTYPE, &m_RtdmXmlData.outputStreamCfg.maxTimeBeforeSendMs,
          NO_MAX_TIME_BEFORE_SEND }, };

/** @brief */
static XmlElementDataStr m_DataRecorderCfg =
    { "DataRecorderCfg", m_DataRecorderCfgAttributes, sizeof(m_DataRecorderCfgAttributes)
                      / sizeof(XmlAttributeDataStr) };

/** @brief */
static XmlElementDataStr m_DataLogFileCfg =
    { "DataLogFileCfg", m_DataLogFileCfgAttributes, sizeof(m_DataLogFileCfgAttributes)
                      / sizeof(XmlAttributeDataStr) };

/** @brief */
static XmlElementDataStr m_OutputStreamCfg =
    { "OutputStreamCfg", m_OutputStreamCfgAttributes, sizeof(m_OutputStreamCfgAttributes)
                      / sizeof(XmlAttributeDataStr) };

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static UINT16 ReadProcessXmlFile (void);
static UINT16 OpenXMLConfigurationFile (void);
static UINT16 ProcessXmlFileParams (XmlElementDataStr *xmlElementPtr);
static UINT16 ProcessXMLSignals (UINT16 *numberofSignals);
static void PopulateVariableAddressesInMap (struct dataBlock_RtdmStream *interface);

/*****************************************************************************/
/**
 * @brief       Reads and processes the XML configuration file
 *
 *              This function reads, processes and stores the XML configuration file.
 *              attributes. It updates all desired parameters into the Rtdm Data
 *              Structure.
 *
 *  @param interface - pointer to MTPE module interface data
 *  @param rtdmXmlData - pointer to pointer for XML configuration data (pointer
 *                       to XML data returned to calling function)
 *
 *  @return UINT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 InitializeXML (struct dataBlock_RtdmStream *interface, RtdmXmlStr **rtdmXmlData)
{
    UINT16 errorCode = NO_ERROR; /* error code from XML processing; return value */

    PopulateVariableAddressesInMap (interface);

    /* Read and process the XML file */
    errorCode = ReadProcessXmlFile ();

    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
        return (errorCode);
    }

    interface->RTDMSignalCount = (UINT16) m_RtdmXmlData.metaData.signalCount;
    interface->RTDMSampleSize = (UINT16) m_RtdmXmlData.metaData.maxStreamHeaderDataSize;
    interface->RTDMSendTime = (UINT16) m_RtdmXmlData.outputStreamCfg.maxTimeBeforeSendMs;
    interface->RTDMMainBuffSize = (UINT16) m_RtdmXmlData.outputStreamCfg.bufferSize;

    /* Return the address of the XML configuration data so that other functions can use
     * it. */
    *rtdmXmlData = &m_RtdmXmlData;

    return (errorCode);
}

/*****************************************************************************/
/**
 * @brief       Returns a pointer to memory that contains the XML configuration file.
 *
 *              This function returns the pointer that contains the entire
 *              XML configuration file. This pointer was dynamically allocated.
 *
 *  @return char * - pointer to memory where XML configuration file is stored
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
char *GetXMLConfigFileBuffer (void)
{
    return (m_ConfigXmlBufferPtr);
}

/*****************************************************************************/
/**
 * @brief       Copies the XML configuration file.
 *
 *              This function copies the XML configuration file to the desired
 *              location. This is done only to serve this file to the FTP
 *              RTDM download program. That Windows application needs the file
 *              to build a viewable DAN file.
 *
 *  @return UINT16 - NO_ERROR on success, other value if failure
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 CopyXMLConfigFile (void)
{
    const char *copyFileName = DRIVE_NAME DIRECTORY_NAME RTDM_XML_FILE;
    FILE *copyFile = NULL;

    if (FileOpenMacro ((char *) copyFileName, "wb+", &copyFile) == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn'open file %s ---> File: %s  Line#: %d\n",
                        copyFileName, __FILE__, __LINE__);
        /* TODO Error type */
        return (1);
    }

    FileWriteMacro(copyFile, &m_ConfigXmlBufferPtr, m_ConfigXmlFileSize, TRUE);

    return (NO_ERROR);

}

/*****************************************************************************/
/**
 * @brief       Reads and process the XML configuration file
 *
 *              Opens the XML configuration file, then reads all and stores all of
 *              the configuration attribute values. It then reads and stores all
 *              signal data information.
 *
 *
 *  @return INT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 ReadProcessXmlFile (void)
{
    UINT16 signalCount = 0; /* Maintains the number of signals discovered */
    UINT16 returnValue = 0; /* Return value */

    /* Try to open XML configuration file */
    returnValue = OpenXMLConfigurationFile ();
    if (returnValue != NO_ERROR)
    {
        return (returnValue);
    }

    ProcessXmlFileParams (&m_DataRecorderCfg);
    ProcessXmlFileParams (&m_DataLogFileCfg);
    ProcessXmlFileParams (&m_OutputStreamCfg);

    /*************************************************************************
     *  POST PROCESS SOME OF THE XML DATA THAT IS NOT A DIRECT CONVERSION
     *************************************************************************/
    /* Time must be a minimum of 1 second */
    if (m_RtdmXmlData.dataRecorderCfg.minRecordingRate < 1000)
    {
        m_RtdmXmlData.dataRecorderCfg.minRecordingRate = 1000;
    }

    /* Time must be a minimum of 1 second */
    if (m_RtdmXmlData.dataLogFileCfg.maxTimeBeforeSaveMs < 1000)
    {
        m_RtdmXmlData.dataLogFileCfg.maxTimeBeforeSaveMs = 1000;
    }

    /* Time must be a minimum of 2 seconds, do not want to overload the CPU */
    if (m_RtdmXmlData.outputStreamCfg.maxTimeBeforeSendMs < 2000)
    {
        m_RtdmXmlData.outputStreamCfg.maxTimeBeforeSendMs = 2000;
    }

    if (m_RtdmXmlData.outputStreamCfg.bufferSize < 2000)
    {
        /* buffer size is not big enough, will overload the CPU */
        dm_free (0, m_ConfigXmlBufferPtr);
        return (NO_BUFFERSIZE);
    }

    /***************************************************************************/
    /* Start loop for finding signal Id's. This section determines which PCU
     * variable are included in the stream sample and data recorder */
    returnValue = ProcessXMLSignals (&signalCount);
    if (returnValue != NO_ERROR)
    {
        return (returnValue);
    }
    /***************************************************************************/

    /* Add sample header size */
    m_RtdmXmlData.metaData.maxStreamHeaderDataSize = (UINT16) (m_RtdmXmlData.metaData.maxStreamDataSize
                    + sizeof(DataSampleStr));

    /* original code but now returning the amount of memory needed */
    if (signalCount == 0)
    {
        /* No signals found */
        return (NO_SIGNALS);
    }

    /* no errors */
    return (NO_ERROR);
}

/*****************************************************************************/
/**
 * @brief       Attempts to open the XML configuration file
 *
 *              Attempts to open the XML configuration file from the specified
 *              location. If successful, the number of bytes in the file is calculated
 *              and then memory is dynamically allocated so that the entire file
 *              can reside in memory and not have to read again. The reason the
 *              file is left in memory is not only for processing, but is required
 *              when creating a data log file.
 *
 *
 *  @return INT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 OpenXMLConfigurationFile (void)
{
    FILE* filePtr = NULL; /* OS file pointer to XML configuration file */
    INT32 returnValue = 0; /* return value from memory allocation function */

    /* open the existing configuration file for reading TEXT MODE ("rb" needed because if "r" only
     * \n gets discarded ) */
    if (FileOpenMacro (RTDM_XML_FILE, "rb", &filePtr) != ERROR)
    {
        /* Get the number of bytes */
        fseek (filePtr, 0L, SEEK_END);
        m_ConfigXmlFileSize = ftell (filePtr);

        /* reset the file position indicator to the beginning of the file */
        fseek (filePtr, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text - clear all bytes. Add
         * an additional byte to ensure a NULL character (end of string) is encountered */
        returnValue = AllocateMemoryAndClear ((m_ConfigXmlFileSize + 1),
                        (void **) &m_ConfigXmlBufferPtr);
        if (returnValue != OK)
        {
            FileCloseMacro(filePtr);
            debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                            __FILE__, __LINE__);
            return (BAD_READ_BUFFER);
        }

        /* copy all the text into the buffer */
        fread (m_ConfigXmlBufferPtr, sizeof(char), (size_t) m_ConfigXmlFileSize, filePtr);

        /* Close the file, no longer needed */
        FileCloseMacro(filePtr);
    }
    /* File does not exist or internal error */
    else
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "Can't Open RTDMConfiguration_PCU.xml or file doesn't exist\n");
        return (NO_XML_INPUT_FILE);
    }

    return (NO_ERROR);
}

/*****************************************************************************/
/**
 * @brief       Reads and stores all XML configuration file parameters
 *
 *              Parses all configuration parameters and stores the attribute
 *              values in the proper memory location
 *
 *
 *  @return INT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 ProcessXmlFileParams (XmlElementDataStr *xmlElementPtr)
{
    char *pStringLocation1 = NULL; /* Pointer used to find keyword "xml_DataRecorderCfg" in memory */
    char *pStringLocation2 = NULL; /* Pointer used to navigate around the file stored in memory */
    UINT16 index = 0; /* Used as a loop index */
    char tempArray[10]; /* temporary storage for a non-integer attribute value */
    UINT32 charCount = 0;
    char *stringPtr = NULL;

    while (index < xmlElementPtr->numAttributes)
    {
        /* Always search for the keyword where configuration parameters are found */
        pStringLocation1 = strstr (m_ConfigXmlBufferPtr, xmlElementPtr->elementName);
        if (pStringLocation1 == NULL)
        {
            /* DataRecorderCfg string not found */
            /* free the memory we used for the buffer */
            dm_free (0, m_ConfigXmlBufferPtr);
            return (NO_DATARECORDERCFG);
        }
        /* Search for the next XML attribute (configuration parameter name) */
        pStringLocation2 = strstr (pStringLocation1, xmlElementPtr->xmlAttibuteData[index].name);
        if (pStringLocation2 != NULL)
        {
            /* Move pointer to attribute value... first double quote (") past the equals (=) */
            pStringLocation2 = strstr (pStringLocation2, "\"");

            /* NOTE: Always assume the attribute value is after the first double quote ("). White spaces and tabs are
             * supported for unsigned and integer values */
            pStringLocation2++;

            /* Skip any spaces */
            while (*pStringLocation2 == ' ')
            {
                pStringLocation2++;
            }

            /* Extract the data value based on the data type associated with the XML attribute */
            switch (xmlElementPtr->xmlAttibuteData[index].dataType)
            {
                case I32_DTYPE:
                    sscanf (pStringLocation2, "%d",
                                    (INT32 *) xmlElementPtr->xmlAttibuteData[index].xmlData);
                    break;

                case U32_DTYPE:
                    sscanf (pStringLocation2, "%u",
                                    (UINT32 *) xmlElementPtr->xmlAttibuteData[index].xmlData);
                    break;

                case BOOLEAN_DTYPE:
                    memset (tempArray, 0, sizeof(tempArray));
                    strncpy (tempArray, pStringLocation2, 5);
                    if (strncmp (tempArray, "TRUE", 4) == 0)
                    {
                        *(BOOL *) xmlElementPtr->xmlAttibuteData[index].xmlData = TRUE;
                    }
                    else
                    {
                        *(BOOL *) xmlElementPtr->xmlAttibuteData[index].xmlData = FALSE;
                    }
                    break;

                case STRING_DTYPE:
                    charCount = 0;
                    while (pStringLocation2[charCount] != '\"')
                    {
                        charCount++;
                    }
                    AllocateMemoryAndClear ((charCount + 1), (void **) &stringPtr);

                    /* NOTE Assume a 32 bit native address architecture, xmlData now contains the
                     * address of the dynamically allocated memory */
                    *(UINT32 *) xmlElementPtr->xmlAttibuteData[index].xmlData = (UINT32) (stringPtr);
                    strncpy ((char *) (*(UINT32 *) xmlElementPtr->xmlAttibuteData[index].xmlData),
                                    pStringLocation2, charCount);
                    break;

                case FILESFULL_DTYPE:
                    strncpy (tempArray, pStringLocation2, 5);
                    if (strncmp (tempArray, "FIFO", 4) == 0)
                    {
                        *(UINT8 *) xmlElementPtr->xmlAttibuteData[index].xmlData = FIFO_POLICY;
                    }
                    else
                    {
                        *(UINT8 *) xmlElementPtr->xmlAttibuteData[index].xmlData = STOP_POLICY;
                    }
                    break;
                default:
                    break;
            }
        }
        else
        {
            return (xmlElementPtr->xmlAttibuteData[index].errorCode);
        }

        /* Move to next attribute */
        index++;

    }

    return (NO_ERROR);

}

/*****************************************************************************/
/**
 * @brief       Reads and stores all XML signal parameters and associated attributes
 *
 *              Parses all configuration signals and stores the mapping of the signal
 *              id, memory location and data size so that data can be streamed properly
 *
 *  @param numberofSignals - pointer to the number of signals discovered in the XML file
 *
 *  @return UINT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 ProcessXMLSignals (UINT16 *numberofSignals)
{
    const char *xmlSignalElement = "<Signal";
    const char *xmlIdAttr = "id";
    const char *xmlDataTypeAttr = "dataType";
    const char *xmlNameAttr = "name";
    char *pStringLocation1 = NULL;
    char *pStringLocationSignal = NULL;
    char tempStr[255];
    UINT32 requiredMemorySize = 0;
    UINT16 i = 0;
    INT32 dataAllocationBytes = 0;
    UINT32 signalId = 0;
    UINT16 signalCount = 0;
    INT32 returnValue = OK;

    pStringLocation1 = m_ConfigXmlBufferPtr;

    /* count the number of signals so that memory can be allocated for the signal definition */
    while ((pStringLocation1 = strstr (pStringLocation1, xmlSignalElement)) != NULL)
    {
        pStringLocation1 = pStringLocation1 + strlen (xmlSignalElement) + 2;
        signalCount++;
    }

    m_RtdmXmlData.metaData.signalCount = signalCount;
    requiredMemorySize = sizeof(SignalDescriptionStr) * signalCount;

    /* allocate memory */
    returnValue = AllocateMemoryAndClear (requiredMemorySize,
                    (void **) &m_RtdmXmlData.signalDesription);

    if (returnValue == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        /* TODO flag error */
    }

    /*********************************************************************************************/
    /* This section determines which PCU variable are included in the stream sample and
     * data recorder */
    signalCount = 0;
    pStringLocation1 = strstr (m_ConfigXmlBufferPtr, xmlSignalElement);
    while ((pStringLocation1 = strstr (pStringLocation1, xmlSignalElement)) != NULL)
    {
        pStringLocationSignal = pStringLocation1;

        /* find signal_id */
        pStringLocation1 = strstr (pStringLocation1, xmlIdAttr);
        /* Move pointer to attribute value... first double quote (") past the equals (=) */
        pStringLocation1 = strstr (pStringLocation1, "\"");
        /* NOTE: Always assume the attribute value is after the first double quote ("). White spaces and tabs are
         * supported for integer values */
        pStringLocation1++;

        /* Skip any spaces */
        while (*pStringLocation1 == ' ')
        {
            pStringLocation1++;
        }

        /* convert signalId to a # and save as int */
        sscanf (pStringLocation1, "%u", &signalId);
        m_RtdmXmlData.signalDesription[signalCount].id = (UINT16) signalId;

        PrintIntegerContents(RTDM_IELF_DBG_LOG, signalId);

        pStringLocation1 = pStringLocationSignal;

        /* Get the variable name and map it to the actual variable address */
        pStringLocation1 = strstr (pStringLocation1, xmlNameAttr);
        /* Move pointer to attribute value... first double quote (") past the equals (=) */
        pStringLocation1 = strstr (pStringLocation1, "\"");
        /* NOTE: Always assume the attribute value is after the first double quote ("). White spaces and tabs are
         * supported for integer values */
        pStringLocation1++;

        /* Skip any spaces */
        while (*pStringLocation1 == ' ')
        {
            pStringLocation1++;
        }

        /* Get the XML signal name */
        i = 0;
        memset (tempStr, 0, sizeof(tempStr));
        while (pStringLocation1[i] != '\"')
        {
            tempStr[i] = pStringLocation1[i];
            i++;
        }

        /* Map the variable name to the actual variable */
        i = 0;
        while ((strcmp (tempStr, m_VariableMap[i].signalName) != 0)
                        && (i < sizeof(m_VariableMap) / sizeof(VariableMapStr)))
        {
            i++;
        }
        if (i < sizeof(m_VariableMap) / sizeof(VariableMapStr))
        {
            m_RtdmXmlData.signalDesription[signalCount].variableAddr = m_VariableMap[i].variableAddr;
        }
        else
        {
            /* handle error - variable name not found in current index */
            return (VARIABLE_NOT_FOUND);
        }

        pStringLocation1 = pStringLocationSignal;

        /* Get the data type */
        pStringLocation1 = strstr (pStringLocation1, xmlDataTypeAttr);
        /* Move pointer to attribute value... first double quote (") past the equals (=) */
        pStringLocation1 = strstr (pStringLocation1, "\"");
        /* NOTE: Always assume the attribute value is after the first double quote ("). White spaces and tabs are
         * supported for integer values */
        pStringLocation1++;

        /* Skip any spaces */
        while (*pStringLocation1 == ' ')
        {
            pStringLocation1++;
        }

        memset (tempStr, 0, sizeof(tempStr));
        i = 0;
        while (pStringLocation1[i] != '\"')
        {
            tempStr[i] = pStringLocation1[i];
            i++;
        }

        i = 0;
        while ((strcmp (tempStr, dataTypeMap[i].dataType) != 0)
                        && (i < sizeof(dataTypeMap) / sizeof(DataTypeMapStr)))
        {
            i++;
        }

        if (i < sizeof(dataTypeMap) / sizeof(DataTypeMapStr))
        {
            m_RtdmXmlData.signalDesription[signalCount].signalType = dataTypeMap[i].signalType;
        }
        else
        {
            /* flag an error - not supported data type */
            return (UNSUPPORTED_DATA_TYPE);
        }

        /* Running calculation of the max amount of memory required if all signals change  */
        dataAllocationBytes += (INT32) sizeof(UINT16);
        switch (m_RtdmXmlData.signalDesription[signalCount].signalType)
        {
            case UINT8_XML_TYPE:
            case INT8_XML_TYPE:
                dataAllocationBytes++;
                break;
            case UINT16_XML_TYPE:
            case INT16_XML_TYPE:
                dataAllocationBytes += 2;
                break;
            case UINT32_XML_TYPE:
            case INT32_XML_TYPE:
                dataAllocationBytes += 4;
                break;
        }

        signalCount++;
    }

    /* This final total indicates the amount of memory needed to store all signal ids
     * an signals. Used for allocating memory later in the initialization process
     */
    m_RtdmXmlData.metaData.maxStreamDataSize = (UINT16) dataAllocationBytes;

    /* Update the final signal count to the calling funtion */
    *numberofSignals = signalCount;

    /* End loop for finding signal ID's */
    return (NO_ERROR);
}

/*****************************************************************************/
/**
 * @brief       Updates the variable map with the memory addresses
 *
 *              The variable addresses of the signals to be read is not known
 *              until runtime. Therefore, the addresses can't be populated
 *              at compile time and this must be populated here. The addresses
 *
 *  @param interface - pointer to the signals that are read
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateVariableAddressesInMap (struct dataBlock_RtdmStream *interface)
{
    m_VariableMap[0].variableAddr = &interface->oPCU_I1.Analog801.CTractEffortReq;
    m_VariableMap[1].variableAddr = &interface->oPCU_I1.Analog801.ICarSpeed;
    m_VariableMap[2].variableAddr = &interface->oPCU_I1.Analog801.IDcLinkCurr;
    m_VariableMap[3].variableAddr = &interface->oPCU_I1.Analog801.IDcLinkVoltage;
    m_VariableMap[4].variableAddr = &interface->oPCU_I1.Analog801.IDiffCurr;
    m_VariableMap[5].variableAddr = &interface->oPCU_I1.Analog801.ILineVoltage;
    m_VariableMap[6].variableAddr = &interface->oPCU_I1.Analog801.IRate;
    m_VariableMap[7].variableAddr = &interface->oPCU_I1.Analog801.IRateRequest;
    m_VariableMap[8].variableAddr = &interface->oPCU_I1.Analog801.ITractEffortDeli;
    m_VariableMap[9].variableAddr = &interface->oPCU_I1.Counter801.IOdometer;
    m_VariableMap[10].variableAddr = &interface->oPCU_I1.Discrete801.CHscbCmd;
    m_VariableMap[11].variableAddr = &interface->oPCU_I1.Discrete801.CRunRelayCmd;
    m_VariableMap[12].variableAddr = &interface->oPCU_I1.Discrete801.CScContCmd;
    m_VariableMap[13].variableAddr = &interface->oPCU_I1.Discrete801.IDynBrkCutOut;
    m_VariableMap[14].variableAddr = &interface->oPCU_I1.Discrete801.IMCSSModeSel;
    m_VariableMap[15].variableAddr = &interface->oPCU_I1.Discrete801.IPKOStatus;
    m_VariableMap[16].variableAddr = &interface->oPCU_I1.Discrete801.IPKOStatusPKOnet;
    m_VariableMap[17].variableAddr = &interface->oPCU_I1.Discrete801.IPropCutout;
    m_VariableMap[18].variableAddr = &interface->oPCU_I1.Discrete801.IPropSystMode;
    m_VariableMap[19].variableAddr = &interface->oPCU_I1.Discrete801.IRegenCutOut;
    m_VariableMap[20].variableAddr = &interface->oPCU_I1.Discrete801.ITractionSafeSts;
    m_VariableMap[21].variableAddr = &interface->oPCU_I1.Discrete801.PRailGapDet;
    m_VariableMap[22].variableAddr = &interface->oPCU_I1.Discrete801.IDcuState;
    m_VariableMap[23].variableAddr = &interface->oPCU_I1.Analog801.ILineCurr;
}

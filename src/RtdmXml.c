/*******************************************************************************
 * PROJECT    : BART
 *
 * MODULE     : RTDM_read_xml.c
 *
 * DESCRIPTON : 	Reads the rtdm_config.xml file and extracts parameters for Real Time Data Streaming (RTDM)
 * 				Write the above rtdm_config.xml file to the data recorder file rtdm.dan This is the required header for the
 *				rtdm.dan file. The header will be written for a new device or if device was formatted to clear data
 *
 * FUNCTIONS:
 *	read_xml_file()
 *
 * INPUTS:
 *	rtdm_config.xml
 *
 * OUTPUTS:
 *	rtdm_config.dan
 *	DataRecorderCfg - id
 *	DataRecorderCfg - version
 *	samplingRate
 *	compressionEnabled
 *	minRecordingRate
 *	DataLogFileCfg enabled
 *	filesCount
 *	numberSamplesInFile
 *	filesFullPolicy
 *	numberSamplesBeforeSave
 *	maxTimeBeforeSaveMs
 *	OutputStreamCfg enabled
 *	comId
 *	bufferSize
 *	maxTimeBeforeSendMs
 *	Signal id[]
 *	dataType[]
 *	signal_dataType
 *	sample_size
 *
 * RETURN:
 *	error_code
 *
 *
 * RC - 10/26/2015
 *
 *******************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif


#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmUtils.h"

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
    /** Attribute value is an signed integer */
    INTEGER_DTYPE,
    /** Attribute value is a large unsigned integer */
    U32_DTYPE,
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

} DataTypeMap;


/** @brief Maps the XML signal name to the variable's address in memory */
typedef struct
{
    /** Variable's signal name in the XML file */
    const char *signalName;
    /** Variable's address */
    void *variableAddr;
} VariableMap;

/** @brief Describes in full the XML configuration parameter */
typedef struct
{
    /** The name of the attribute */
    char *subString;
    /** The data type of the attribute */
    XmlDataType dataType;
    /** The location memory where the attribute value is stored */
    void *xmlData;
    /** The error code returned if any errors were encountered while processing the
     *  configuration parameter */
    UINT16 errorCode;
} XMLConfigReader;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Pointer to memory where the entire XML configuration file is stored */
static char *m_ConfigXmlBufferPtr = NULL;

/** @brief Maps the XML data type to the local storage data type */
static const DataTypeMap dataTypeMap[] =
{
    {
        "UINT8", UINT8_XML_TYPE },
    {
        "INT8", INT8_XML_TYPE },
    {
        "UINT16", UINT16_XML_TYPE },
    {
        "INT16", INT16_XML_TYPE },
    {
        "UINT32", UINT32_XML_TYPE },
    {
        "INT32", INT32_XML_TYPE }, };

extern TYPE_RTDMSTREAM_IF mStreamInfo;

/** @brief Stores all of the XML configuration data plus additional parameters that
 * are calculated from the configuration data */
static RtdmXmlStr m_RtdmXmlData;

/** @brief Maps all XML signal names to the memory location where the signal is read */
#if 0

static const VariableMap variableMap[] =
    {
        {
            "oPCU_I1.PCU_I1.Analog801.CTractEffortReq",
            &mStreamInfo.oPCU_I1.Analog801.CTractEffortReq },
        {
            "oPCU_I1.PCU_I1.Analog801.ICarSpeed", &mStreamInfo.oPCU_I1.Analog801.ICarSpeed },
        {
            "oPCU_I1.PCU_I1.Analog801.IDcLinkCurr", &mStreamInfo.oPCU_I1.Analog801.IDcLinkCurr },
        {
            "oPCU_I1.PCU_I1.Analog801.IDcLinkVoltage", &mStreamInfo.oPCU_I1.Analog801.IDcLinkVoltage },
        {
            "oPCU_I1.PCU_I1.Analog801.IDiffCurr", &mStreamInfo.oPCU_I1.Analog801.IDiffCurr },
        {
            "oPCU_I1.PCU_I1.Analog801.ILineVoltage", &mStreamInfo.oPCU_I1.Analog801.ILineVoltage },

        {
            "oPCU_I1.PCU_I1.Analog801.IRate", &mStreamInfo.oPCU_I1.Analog801.IRate },

        {
            "oPCU_I1.PCU_I1.Analog801.IRateRequest", &mStreamInfo.oPCU_I1.Analog801.IRateRequest },
        {
            "oPCU_I1.PCU_I1.Analog801.ITractEffortDeli",
            &mStreamInfo.oPCU_I1.Analog801.ITractEffortDeli },
        {
            "oPCU_I1.PCU_I1.Counter801.IOdometer", &mStreamInfo.oPCU_I1.Counter801.IOdometer },
        {
            "oPCU_I1.PCU_I1.Discrete801.CHscbCmd", &mStreamInfo.oPCU_I1.Discrete801.CHscbCmd },
        {
            "oPCU_I1.PCU_I1.Discrete801.CRunRelayCmd", &mStreamInfo.oPCU_I1.Discrete801.CRunRelayCmd },
        {
            "oPCU_I1.PCU_I1.Discrete801.CScContCmd", &mStreamInfo.oPCU_I1.Discrete801.CScContCmd },
        {
            "oPCU_I1.PCU_I1.Discrete801.IDynBrkCutOut",
            &mStreamInfo.oPCU_I1.Discrete801.IDynBrkCutOut },
        {
            "oPCU_I1.PCU_I1.Discrete801.IMCSSModeSel", &mStreamInfo.oPCU_I1.Discrete801.IMCSSModeSel },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPKOStatus", &mStreamInfo.oPCU_I1.Discrete801.IPKOStatus },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPKOStatusPKOnet",
            &mStreamInfo.oPCU_I1.Discrete801.IPKOStatusPKOnet },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPropCutout", &mStreamInfo.oPCU_I1.Discrete801.IPropCutout },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPropSystMode",
            &mStreamInfo.oPCU_I1.Discrete801.IPropSystMode },
        {
            "oPCU_I1.PCU_I1.Discrete801.IRegenCutOut", &mStreamInfo.oPCU_I1.Discrete801.IRegenCutOut },
        {
            "oPCU_I1.PCU_I1.Discrete801.ITractionSafeSts",
            &mStreamInfo.oPCU_I1.Discrete801.ITractionSafeSts },
        {
            "oPCU_I1.PCU_I1.Discrete801.PRailGapDet", &mStreamInfo.oPCU_I1.Discrete801.PRailGapDet },
        {
            "oPCU_I1.PCU_I1.Discrete801.IDcuState", &mStreamInfo.oPCU_I1.Discrete801.IDcuState },
        {
            "oPCU_I1.PCU_I1.Analog801.ILineCurr", &mStreamInfo.oPCU_I1.Analog801.ILineCurr }

    };
#else
static VariableMap variableMap[] =
    {
        {
            "oPCU_I1.PCU_I1.Analog801.CTractEffortReq",
            NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.ICarSpeed", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.IDcLinkCurr", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.IDcLinkVoltage", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.IDiffCurr", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.ILineVoltage", NULL },

        {
            "oPCU_I1.PCU_I1.Analog801.IRate", NULL },

        {
            "oPCU_I1.PCU_I1.Analog801.IRateRequest", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.ITractEffortDeli",
            NULL },
        {
            "oPCU_I1.PCU_I1.Counter801.IOdometer", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.CHscbCmd", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.CRunRelayCmd", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.CScContCmd", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IDynBrkCutOut",
            NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IMCSSModeSel", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPKOStatus", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPKOStatusPKOnet",
            NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPropCutout", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IPropSystMode",
            NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IRegenCutOut", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.ITractionSafeSts",
            NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.PRailGapDet", NULL },
        {
            "oPCU_I1.PCU_I1.Discrete801.IDcuState", NULL },
        {
            "oPCU_I1.PCU_I1.Analog801.ILineCurr", NULL }

    };
#endif

/** @brief Maps all XML configuration parameter names to the data type, memory location,
 * and error code */
static const XMLConfigReader m_XmlConfigReader[] =
{
    {
        "id", INTEGER_DTYPE, &m_RtdmXmlData.DataRecorderCfgID, NO_CONFIG_ID },
    {
        "version", INTEGER_DTYPE, &m_RtdmXmlData.DataRecorderCfgVersion,
        NO_VERSION },
    {
        "samplingRate", INTEGER_DTYPE, &m_RtdmXmlData.SamplingRate,
        NO_SAMPLING_RATE },
    {
        "compressionEnabled", BOOLEAN_DTYPE, &m_RtdmXmlData.Compression_enabled,
        NO_COMPRESSION_ENABLED },
    {
        "minRecordingRate", INTEGER_DTYPE, &m_RtdmXmlData.MinRecordingRate,
        NO_MIN_RECORD_RATE },
    {
        "DataLogFileCfg enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.DataLogFileCfg_enabled,
        NO_DATALOG_CFG_ENABLED },
    {
        "filesCount", INTEGER_DTYPE, &m_RtdmXmlData.FilesCount, NO_FILES_COUNT },
    {
        "numberSamplesInFile", INTEGER_DTYPE, &m_RtdmXmlData.NumberSamplesInFile,
        NO_NUM_SAMPLES_IN_FILE },
    {
        "filesFullPolicy", FILESFULL_DTYPE, &m_RtdmXmlData.FilesFullPolicy,
        NO_FILE_FULL_POLICY },
    {
        "numberSamplesBeforeSave", INTEGER_DTYPE, &m_RtdmXmlData.NumberSamplesBeforeSave,
        NO_NUM_SAMPLES_BEFORE_SAVE },
    {
        "maxTimeBeforeSaveMs", INTEGER_DTYPE, &m_RtdmXmlData.MaxTimeBeforeSaveMs,
        NO_MAX_TIME_BEFORE_SAVE },
    {
        "OutputStreamCfg enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.OutputStream_enabled,
        NO_OUTPUT_STREAM_ENABLED },
    {
        "comId", U32_DTYPE, &m_RtdmXmlData.comId,
        NO_COMID },

    {
        "bufferSize", INTEGER_DTYPE, &m_RtdmXmlData.bufferSize,
        NO_BUFFERSIZE },

    {
        "maxTimeBeforeSendMs", INTEGER_DTYPE, &m_RtdmXmlData.maxTimeBeforeSendMs,
        NO_MAX_TIME_BEFORE_SEND },

};

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static UINT16 ReadProcessXmlFile (void);
static UINT16 OpenXMLConfigurationFile (void);
static UINT16 ProcessXmlFileParams (void);
static UINT16 ProcessXMLSignals (UINT16 *numberofSignals);

/*****************************************************************************/
/**
 * @brief       Reads and processes the XML configuration file
 *
 *              This function reads and processes the XML configuration file.
 *              It updates all desired parameters into the
 *
 *  @param interface - pointer to MTPE module interface data
 *  @param rtdmXmlData - pointer to pointer for XML configuration data (pointer
 *                       to XML data returned to calling function)
 *
 *  @return UINT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 InitializeXML (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr **rtdmXmlData)
{
    UINT16 errorCode = NO_ERROR; /* error code from XML processing; return value */

    variableMap[0].variableAddr = &interface->oPCU_I1.Analog801.CTractEffortReq;
    variableMap[1].variableAddr = &interface->oPCU_I1.Analog801.ICarSpeed;
    variableMap[2].variableAddr = &interface->oPCU_I1.Analog801.IDcLinkCurr;
    variableMap[3].variableAddr = &interface->oPCU_I1.Analog801.IDcLinkVoltage;
    variableMap[4].variableAddr = &interface->oPCU_I1.Analog801.IDiffCurr;
    variableMap[5].variableAddr = &interface->oPCU_I1.Analog801.ILineVoltage;
    variableMap[6].variableAddr = &interface->oPCU_I1.Analog801.IRate;
    variableMap[7].variableAddr = &interface->oPCU_I1.Analog801.IRateRequest;
    variableMap[8].variableAddr = &interface->oPCU_I1.Analog801.ITractEffortDeli;
    variableMap[9].variableAddr = &interface->oPCU_I1.Counter801.IOdometer;
    variableMap[10].variableAddr = &interface->oPCU_I1.Discrete801.CHscbCmd;
    variableMap[11].variableAddr = &interface->oPCU_I1.Discrete801.CRunRelayCmd;
    variableMap[12].variableAddr = &interface->oPCU_I1.Discrete801.CScContCmd;
    variableMap[13].variableAddr = &interface->oPCU_I1.Discrete801.IDynBrkCutOut;
    variableMap[14].variableAddr = &interface->oPCU_I1.Discrete801.IMCSSModeSel;
    variableMap[15].variableAddr = &interface->oPCU_I1.Discrete801.IPKOStatus;
    variableMap[16].variableAddr = &interface->oPCU_I1.Discrete801.IPKOStatusPKOnet;
    variableMap[17].variableAddr = &interface->oPCU_I1.Discrete801.IPropCutout;
    variableMap[18].variableAddr = &interface->oPCU_I1.Discrete801.IPropSystMode;
    variableMap[19].variableAddr = &interface->oPCU_I1.Discrete801.IRegenCutOut;
    variableMap[20].variableAddr = &interface->oPCU_I1.Discrete801.ITractionSafeSts;
    variableMap[21].variableAddr = &interface->oPCU_I1.Discrete801.PRailGapDet;
    variableMap[22].variableAddr = &interface->oPCU_I1.Discrete801.IDcuState;
    variableMap[23].variableAddr = &interface->oPCU_I1.Analog801.ILineCurr;


    /* Read and process the XML file */
    errorCode = ReadProcessXmlFile ();

    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
        return (errorCode);
    }

    /* Calculate MAX buffer size - subtract 2 to make room for Main Header - how many samples
     * will fit into buffer size from .xml ex: 60,000 */
    m_RtdmXmlData.max_main_buffer_count = (UINT16)((m_RtdmXmlData.bufferSize / m_RtdmXmlData.sample_size)
                    - 2);

    /* Set to interface so we can see in DCUTerm */
    interface->RTDMMainBuffCount = m_RtdmXmlData.max_main_buffer_count;
    interface->RTDMSignalCount = m_RtdmXmlData.signal_count;
    interface->RTDMSampleSize = m_RtdmXmlData.sample_size;
    interface->RTDMSendTime = m_RtdmXmlData.maxTimeBeforeSendMs;
    interface->RTDMMainBuffSize = m_RtdmXmlData.bufferSize;

    /* Return the address of the XML configuration data so that other functions can use
     * it. */
    *rtdmXmlData = &m_RtdmXmlData;

    return (NO_ERROR);
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
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
char *GetXMLConfigFileBuffer (void)
{
    return m_ConfigXmlBufferPtr;
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
 * Date & Author : 01SEP2016 - D.Smail
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

    ProcessXmlFileParams ();

    /*************************************************************************
     *  POST PROCESS SOME OF THE XML DATA THAT IS NOT A DIRECT CONVERSION
     *************************************************************************/
    /* Time must be a minimum of 1 second */
    if (m_RtdmXmlData.MinRecordingRate < 1000)
    {
        m_RtdmXmlData.MinRecordingRate = 1000;
    }

    /* Time must be a minimum of 1 second */
    if (m_RtdmXmlData.MaxTimeBeforeSaveMs < 1000)
    {
        m_RtdmXmlData.MaxTimeBeforeSaveMs = 1000;
    }

    /* Time must be a minimum of 2 seconds, do not want to overload the CPU */
    if (m_RtdmXmlData.maxTimeBeforeSendMs < 2000)
    {
        m_RtdmXmlData.maxTimeBeforeSendMs = 2000;
    }

    if (m_RtdmXmlData.bufferSize < 2000)
    {
        /* buffer size is not big enough, will overload the CPU */
        free (m_ConfigXmlBufferPtr);
        return (NO_BUFFERSIZE);
    }

    /***************************************************************************/
    /* Start loop for finding signal Id's. This section determines which PCU
     * variable are included in the stream sample and data recorder */
    returnValue = ProcessXMLSignals (&signalCount);
    if (returnValue != NO_ERROR)
    {
        return returnValue;
    }
    /***************************************************************************/

    /* Add sample header size */
    m_RtdmXmlData.sample_size = (UINT16)(m_RtdmXmlData.dataAllocationSize + sizeof(DataSampleStr));

    /* original code but now returning the amount of memory needed */
    if (signalCount <= 0)
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
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 OpenXMLConfigurationFile (void)
{
    FILE* filePtr = NULL; /* OS file pointer to XML configuration file */
    INT32 numBytes = 0; /* Number of bytes in the XML configuration file  */

    /* open the existing configuration file for reading TEXT MODE */
    if (os_io_fopen (RTDM_XML_FILE, "r", &filePtr) != ERROR)
    {
        /* Get the number of bytes */
        fseek (filePtr, 0L, SEEK_END);
        numBytes = ftell (filePtr);

        /* reset the file position indicator to the beginning of the file */
        fseek (filePtr, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text - clear all bytes. Add
         * an additional byte to ensure a NULL character (end of string) is encountered */
        m_ConfigXmlBufferPtr = (char*) calloc ((size_t)(numBytes + 1), sizeof(char));

        /* memory error */
        if (m_ConfigXmlBufferPtr == NULL)
        {
            os_io_fclose(filePtr);
            debugPrintf("Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__, __LINE__);

            /* free the memory we used for the buffer */
            free (m_ConfigXmlBufferPtr);
            return (BAD_READ_BUFFER);
        }

        /* copy all the text into the buffer */
        fread (m_ConfigXmlBufferPtr, sizeof(char), (size_t)numBytes, filePtr);

        /* Close the file, no longer needed */
        os_io_fclose(filePtr);
    }
    /* File does not exist or internal error */
    else
    {
        debugPrintf("Can't Open RTDMConfiguration_PCU.xml or file doesn't exist\n");
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
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 ProcessXmlFileParams (void)
{
    const char *xml_DataRecorderCfg = "DataRecorderCfg"; /* Keyword where configuration
     parameters are stored */
    char *pStringLocation1 = NULL; /* Pointer used to find keyword "xml_DataRecorderCfg" in memory */
    char *pStringLocation2 = NULL; /* Pointer used to navigate around the file stored in memory */
    UINT16 index = 0;       /* Used as a loop index */
    char tempArray[10];     /* temporary storage for a non-integer attribute value */

    while (index < sizeof(m_XmlConfigReader) / sizeof(XMLConfigReader))
    {
        /* Always search for the keyword where configuration parameters are found */
        pStringLocation1 = strstr (m_ConfigXmlBufferPtr, xml_DataRecorderCfg);
        if (pStringLocation1 == NULL)
        {
            /* DataRecorderCfg string not found */
            /* free the memory we used for the buffer */
            free (m_ConfigXmlBufferPtr);
            return (NO_DATARECORDERCFG);
        }
        /* Search for the next XML attribute (configuration parameter name) */
        pStringLocation2 = strstr (pStringLocation1, m_XmlConfigReader[index].subString);
        if (pStringLocation2 != NULL)
        {
            /* Move pointer to attribute value... first double quote (") past the equals (=) */
            pStringLocation2 = strstr (pStringLocation2, "\"");

            /* NOTE: Always assume the attribute value is after the first double quote ("). White spaces and tabs are
             * supported for integer values */
            pStringLocation2++;

            /* Skip any spaces */
            while (*pStringLocation2 == ' ')
            {
                pStringLocation2++;
            }

            /* Extract the data value based on the data type associated with the XML attribute */
            switch (m_XmlConfigReader[index].dataType)
            {
                case INTEGER_DTYPE:
                    sscanf (pStringLocation2, "%d", (INT32 *) m_XmlConfigReader[index].xmlData);
                    break;

                case U32_DTYPE:
                    sscanf (pStringLocation2, "%u", (UINT32 *) m_XmlConfigReader[index].xmlData);
                    break;

                case BOOLEAN_DTYPE:
                    strncpy (tempArray, pStringLocation2, 5);
                    if (strncmp (tempArray, "TRUE", 4) == 0)
                    {
                        *(UINT8 *) m_XmlConfigReader[index].xmlData = TRUE;
                    }
                    else
                    {
                        *(UINT8 *) m_XmlConfigReader[index].xmlData = FALSE;
                    }
                    break;

                case FILESFULL_DTYPE:
                    strncpy (tempArray, pStringLocation2, 5);
                    if (strncmp (tempArray, "FIFO", 4) == 0)
                    {
                        *(UINT8 *) m_XmlConfigReader[index].xmlData = FIFO_POLICY;
                    }
                    else
                    {
                        *(UINT8 *) m_XmlConfigReader[index].xmlData = STOP_POLICY;
                    }
                    break;
                default:
                    break;
            }
        }
        else
        {
            return (m_XmlConfigReader[index].errorCode);
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
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 ProcessXMLSignals (UINT16 *numberofSignals)
{
    const char *xmlSignalId = "Signal id";
    const char *xmlDataType = "dataType";
    const char *xmlName = "name";
    char *pStringLocation1 = NULL;
    char tempStr[255];
    UINT32 requiredMemorySize = 0;
    UINT16 i = 0;
    INT32 dataAllocationBytes = 0;
    UINT16 signalId = 0;
    UINT16 signalCount = 0;

    pStringLocation1 = m_ConfigXmlBufferPtr;

    /* count the number of signals so that memory can be allocated for the signal definition */
    while ((pStringLocation1 = strstr (pStringLocation1, xmlSignalId)) != NULL)
    {
        pStringLocation1 = pStringLocation1 + strlen (xmlSignalId) + 2;
        signalCount++;
    }

    m_RtdmXmlData.signal_count = signalCount;
    requiredMemorySize = sizeof(SignalDescription) * signalCount;

    /* allocate memory */
    m_RtdmXmlData.signalDesription = (SignalDescription *) calloc (requiredMemorySize,
                    sizeof(UINT8));

    /*********************************************************************************************/
    /* This section determines which PCU variable are included in the stream sample and
     * data recorder */
    signalCount = 0;
    pStringLocation1 = strstr (m_ConfigXmlBufferPtr, xmlSignalId);
    while ((pStringLocation1 = strstr (pStringLocation1, xmlSignalId)) != NULL)
    {
        /* find signal_id */
        pStringLocation1 = strstr (pStringLocation1, xmlSignalId);
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
        sscanf (pStringLocation1, "%u", (UINT32 *) &signalId);
        m_RtdmXmlData.signalDesription[signalCount].id = signalId;

        /* Get the variable name and map it to the actual variable address */
        pStringLocation1 = strstr (pStringLocation1, xmlName);
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
        while ((strcmp (tempStr, variableMap[i].signalName) != 0)
                        && (i < sizeof(variableMap) / sizeof(VariableMap)))
        {
            i++;
        }
        if (i < sizeof(variableMap) / sizeof(VariableMap))
        {
            m_RtdmXmlData.signalDesription[signalCount].variableAddr = variableMap[i].variableAddr;
        }
        else
        {
            /* handle error - variable name not found in current index */
            return (VARIABLE_NOT_FOUND);
        }

        /* Get the data type */
        pStringLocation1 = strstr (pStringLocation1, xmlDataType);
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
                        && (i < sizeof(dataTypeMap) / sizeof(DataTypeMap)))
        {
            i++;
        }

        if (i < sizeof(dataTypeMap) / sizeof(DataTypeMap))
        {
            m_RtdmXmlData.signalDesription[signalCount].signalType = dataTypeMap[i].signalType;
        }
        else
        {
            /* flag an error - not supported data type */
            return (UNSUPPORTED_DATA_TYPE);
        }

        /* Running calculation of the max amount of memory required if all signals change  */
        dataAllocationBytes += (INT32)sizeof(UINT16);
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
    m_RtdmXmlData.dataAllocationSize = (UINT16)dataAllocationBytes;

    /* Update the final signal count to the calling funtion */
    *numberofSignals = signalCount;

    /* End loop for finding signal ID's */
    return (NO_ERROR);
}

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TEST_ON_PC
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif

#include "RtdmUtils.h"
#include "RtdmStream.h"
#include "RtdmXml.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/* Main Header Size */
#define SAMPLE_HEADER_SIZE      9

/* XML file on VCUC */
#define RTDM_XML_FILE           "RTDMConfiguration_PCU.xml"

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
typedef enum
{
    BOOLEAN_DTYPE, INTEGER_DTYPE, U32_DTYPE, FILESFULL_DTYPE
} XmlDataType;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
typedef struct
{
    const char *dataType;
    XmlSignalType signalType;

} DataTypeMap;

typedef struct
{
    const char *signalName;
    void *variableAddr;
} VariableMap;

typedef struct
{
    char *subString;
    XmlDataType dataType;
    void *xmlData;
    UINT16 errorCode;
} XMLConfigReader;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
static char *m_ConfigXmlBufferPtr = NULL;

static const DataTypeMap dataTypeMap[] =
{
{ "UINT8", UINT8_XML_TYPE },
{ "INT8", INT8_XML_TYPE },
{ "UINT16", UINT16_XML_TYPE },
{ "INT16", INT16_XML_TYPE },
{ "UINT32", UINT32_XML_TYPE },
{ "INT32", INT32_XML_TYPE }, };

extern TYPE_RTDM_STREAM_IF mStreamInfo;

static const VariableMap variableMap[] =
                {
                    {
                        "oPCU_I1.PCU_I1.Analog801.CTractEffortReq",
                        &mStreamInfo.oPCU_I1.Analog801.CTractEffortReq },
                    { "oPCU_I1.PCU_I1.Analog801.ICarSpeed", &mStreamInfo.oPCU_I1.Analog801.ICarSpeed },
                    {
                        "oPCU_I1.PCU_I1.Analog801.IDcLinkCurr",
                        &mStreamInfo.oPCU_I1.Analog801.IDcLinkCurr },
                    {
                        "oPCU_I1.PCU_I1.Analog801.IDcLinkVoltage",
                        &mStreamInfo.oPCU_I1.Analog801.IDcLinkVoltage },
                    { "oPCU_I1.PCU_I1.Analog801.IDiffCurr", &mStreamInfo.oPCU_I1.Analog801.IDiffCurr },
                    {
                        "oPCU_I1.PCU_I1.Analog801.ILineVoltage",
                        &mStreamInfo.oPCU_I1.Analog801.ILineVoltage },

                    { "oPCU_I1.PCU_I1.Analog801.IRate", &mStreamInfo.oPCU_I1.Analog801.IRate },

                    {
                        "oPCU_I1.PCU_I1.Analog801.IRateRequest",
                        &mStreamInfo.oPCU_I1.Analog801.IRateRequest },
                    {
                        "oPCU_I1.PCU_I1.Analog801.ITractEffortDeli",
                        &mStreamInfo.oPCU_I1.Analog801.ITractEffortDeli },
                    {
                        "oPCU_I1.PCU_I1.Counter801.IOdometer",
                        &mStreamInfo.oPCU_I1.Counter801.IOdometer },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.CHscbCmd",
                        &mStreamInfo.oPCU_I1.Discrete801.CHscbCmd },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.CRunRelayCmd",
                        &mStreamInfo.oPCU_I1.Discrete801.CRunRelayCmd },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.CScContCmd",
                        &mStreamInfo.oPCU_I1.Discrete801.CScContCmd },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IDynBrkCutOut",
                        &mStreamInfo.oPCU_I1.Discrete801.IDynBrkCutOut },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IMCSSModeSel",
                        &mStreamInfo.oPCU_I1.Discrete801.IMCSSModeSel },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IPKOStatus",
                        &mStreamInfo.oPCU_I1.Discrete801.IPKOStatus },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IPKOStatusPKOnet",
                        &mStreamInfo.oPCU_I1.Discrete801.IPKOStatusPKOnet },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IPropCutout",
                        &mStreamInfo.oPCU_I1.Discrete801.IPropCutout },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IPropSystMode",
                        &mStreamInfo.oPCU_I1.Discrete801.IPropSystMode },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IRegenCutOut",
                        &mStreamInfo.oPCU_I1.Discrete801.IRegenCutOut },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.ITractionSafeSts",
                        &mStreamInfo.oPCU_I1.Discrete801.ITractionSafeSts },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.PRailGapDet",
                        &mStreamInfo.oPCU_I1.Discrete801.PRailGapDet },
                    {
                        "oPCU_I1.PCU_I1.Discrete801.IDcuState",
                        &mStreamInfo.oPCU_I1.Discrete801.IDcuState },
                    { "oPCU_I1.PCU_I1.Analog801.ILineCurr", &mStreamInfo.oPCU_I1.Analog801.ILineCurr }

                };

/* TODO make static */
RtdmXmlStr m_RtdmXmlData;

const XMLConfigReader m_XmlConfigReader[] =
{
{ "id", INTEGER_DTYPE, &m_RtdmXmlData.DataRecorderCfgID, NO_CONFIG_ID },
{ "version", INTEGER_DTYPE, &m_RtdmXmlData.DataRecorderCfgVersion,
NO_VERSION },
{ "samplingRate", INTEGER_DTYPE, &m_RtdmXmlData.SamplingRate,
NO_SAMPLING_RATE },
{ "compressionEnabled", BOOLEAN_DTYPE, &m_RtdmXmlData.Compression_enabled,
NO_COMPRESSION_ENABLED },
{ "minRecordingRate", INTEGER_DTYPE, &m_RtdmXmlData.MinRecordingRate,
NO_MIN_RECORD_RATE },
{ "DataLogFileCfg enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.DataLogFileCfg_enabled,
NO_DATALOG_CFG_ENABLED },
{ "filesCount", INTEGER_DTYPE, &m_RtdmXmlData.FilesCount, NO_FILES_COUNT },
{ "numberSamplesInFile", INTEGER_DTYPE, &m_RtdmXmlData.NumberSamplesInFile,
NO_NUM_SAMPLES_IN_FILE },
{ "filesFullPolicy", FILESFULL_DTYPE, &m_RtdmXmlData.FilesFullPolicy,
NO_FILE_FULL_POLICY },
{ "numberSamplesBeforeSave", INTEGER_DTYPE, &m_RtdmXmlData.NumberSamplesBeforeSave,
NO_NUM_SAMPLES_BEFORE_SAVE },
{ "maxTimeBeforeSaveMs", INTEGER_DTYPE, &m_RtdmXmlData.MaxTimeBeforeSaveMs,
NO_MAX_TIME_BEFORE_SAVE },
{ "OutputStreamCfg enabled", BOOLEAN_DTYPE, &m_RtdmXmlData.OutputStream_enabled,
NO_OUTPUT_STREAM_ENABLED },
{ "comId", U32_DTYPE, &m_RtdmXmlData.comId,
NO_COMID },

{ "bufferSize", INTEGER_DTYPE, &m_RtdmXmlData.bufferSize,
NO_BUFFERSIZE },

{ "maxTimeBeforeSendMs", INTEGER_DTYPE, &m_RtdmXmlData.maxTimeBeforeSendMs,
NO_MAX_TIME_BEFORE_SEND },

};

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static INT16 ReadXmlFile (void);
static INT16 OpenXMLConfigurationFile (char **configFileXMLBufferPtr);
static UINT16 ProcessXmlFileParams (char *pStringLocation1, INT16 index);
static UINT16 FindSignals (char* pStringLocation1);

UINT16 InitializeXML (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    UINT16 errorCode = NO_ERROR;

    /* Read the XML file */
    errorCode = ReadXmlFile ();

    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
        return (errorCode);
    }

    /* Calculate MAX buffer size - subtract 2 to make room for Main Header - how many samples
     * will fit into buffer size from .xml ex: 60,000 */
    rtdmXmlData->max_main_buffer_count = ((rtdmXmlData->bufferSize / rtdmXmlData->sample_size) - 2);

    /* Set to interface so we can see in DCUTerm */
    interface->RTDMMainBuffCount = rtdmXmlData->max_main_buffer_count;
    interface->RTDMSignalCount = rtdmXmlData->signal_count;
    interface->RTDMSampleSize = rtdmXmlData->sample_size;
    interface->RTDMSendTime = rtdmXmlData->maxTimeBeforeSendMs;
    interface->RTDMMainBuffSize = rtdmXmlData->bufferSize;

    return (NO_ERROR);
}

char *GetXMLConfigFileBuffer (void)
{
    return m_ConfigXmlBufferPtr;
}

/*********************************************************************************************************
 Open RTDMConfiguration_PCU.xml
 copy contents to a temp buffer (calloc)
 close file
 write RTDMConfiguration_PCU.xml file to rtdm.dan file.  This is for the data recording portion of rtdm.
 Check for Num of Streams in .dan file.  This means a restart of a previous data recorder
 parse temp buffer for needed data
 free temp buffer
 ***********************************************************************************************************/
static INT16 ReadXmlFile (void)
{
    const char *xml_DataRecorderCfg = "DataRecorderCfg";
    char *pStringLocation1 = NULL;
    UINT16 signalCount = 0;
    UINT16 index = 0;
    UINT16 errorCode = 0;
    INT16 returnValue = 0;

    /* Try to open XML configuration file */
    returnValue = OpenXMLConfigurationFile (&m_ConfigXmlBufferPtr);
    if (returnValue != NO_ERROR)
    {
        return returnValue;
    }

    /***********************************************************************************************************************/
    /* This is where we extract the data from the config.xml file which gets sent to the stream data */
    /* DataRecorderCfg, id, version, enabled, and maxtime */
    /* Find DataRecorderCfg - pointer to text "DataRecorderCfg", pointer contains all remaining text in the file */
    pStringLocation1 = strstr (m_ConfigXmlBufferPtr, xml_DataRecorderCfg);

    if (pStringLocation1 != NULL)
    {
        while (index < sizeof(m_XmlConfigReader) / sizeof(XMLConfigReader))
        {
            errorCode = ProcessXmlFileParams (pStringLocation1, index);
            if (errorCode != NO_ERROR)
            {
                free (m_ConfigXmlBufferPtr);
                return errorCode;
            }
            index++;
        }

        /*************************************************************************
         *  POST PROCESS SOME OF THE XML DATA THAT IS NOT A DIRECT CONVERSION
         *************************************************************************/
        /* Convert from msecs to seconds */
        m_RtdmXmlData.MinRecordingRate /= 1000;
        /* Time must be a minimum of 1 second */
        if (m_RtdmXmlData.MinRecordingRate < 1)
        {
            m_RtdmXmlData.MinRecordingRate = 1;
        }

        /* Convert from msecs to seconds */
        m_RtdmXmlData.MaxTimeBeforeSaveMs /= 1000;
        /* Time must be a minimum of 1 second */
        if (m_RtdmXmlData.MaxTimeBeforeSaveMs < 1)
        {
            m_RtdmXmlData.MaxTimeBeforeSaveMs = 1;
        }

        /* Convert from msecs to seconds */
        m_RtdmXmlData.maxTimeBeforeSendMs /= 1000;
        /* Time must be a minimum of 2 seconds, do not want to overload the CPU */
        if (m_RtdmXmlData.maxTimeBeforeSendMs < 2)
        {
            m_RtdmXmlData.maxTimeBeforeSendMs = 2;
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
        signalCount = FindSignals (pStringLocation1);
        /***************************************************************************/
    }
    else
    {
        /* DataRecorderCfg string not found */
        /* free the memory we used for the buffer */
        free (m_ConfigXmlBufferPtr);

        return (NO_DATARECORDERCFG);
    }

    /* Add sample header size */
    m_RtdmXmlData.sample_size = m_RtdmXmlData.dataAllocationSize + SAMPLE_HEADER_SIZE;

    /* original code but now returning the amount of memory needed */
    if (signalCount <= 0)
    {
        /* No signals found */
        return (NO_SIGNALS);
    }

    /* no errors */
    return (NO_ERROR);
}

static INT16 OpenXMLConfigurationFile (char **configFileXMLBufferPtr)
{
    FILE* filePtr = NULL;
    INT32 numBytes = 0;

    /* open the existing configuration file for reading TEXT MODE */
    if (os_io_fopen (RTDM_XML_FILE, "r", &filePtr) != ERROR)
    {
        /* Get the number of bytes */
        fseek (filePtr, 0L, SEEK_END);
        numBytes = ftell (filePtr);

        /* reset the file position indicator to the beginning of the file */
        fseek (filePtr, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text - clear to zero */
        *configFileXMLBufferPtr = (char*) calloc (numBytes, sizeof(char));

        /* memory error */
        if (configFileXMLBufferPtr == NULL)
        {
            os_io_fclose(filePtr);

            /* free the memory we used for the buffer */
            free (*configFileXMLBufferPtr);
            return (BAD_READ_BUFFER);
        }

        /* copy all the text into the buffer */
        fread (*configFileXMLBufferPtr, sizeof(char), numBytes, filePtr);

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

static UINT16 ProcessXmlFileParams (char *pStringLocation1, INT16 index)
{

    UINT16 errorCode = NO_ERROR;
    char tempArray[10];

    char *pStringLocation2 = strstr (pStringLocation1, m_XmlConfigReader[index].subString);

    if (pStringLocation2 != NULL)
    {
        /* move pointer to id # */
        pStringLocation2 = pStringLocation2 + strlen (m_XmlConfigReader[index].subString) + 2;

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
        errorCode = m_XmlConfigReader[index].errorCode;
    }

    return (errorCode);

}

static UINT16 FindSignals (char* pStringLocation1)
{
    const char *xml_signal_id = "Signal id";
    const char *xml_dataType = "dataType";
    const char *xml_name = "name";

    char* startLocation = pStringLocation1;
    char tempStr[255];
    UINT32 requiredMemorySize = 0;
    UINT16 i = 0;
    UINT16 dataAllocationBytes = 0;

    UINT16 signalCount = 0;
    UINT16 signalId = 0;

    /* count the number of signals so that memory can be allocated for the signal definition */
    while ((pStringLocation1 = strstr (pStringLocation1, xml_signal_id)) != NULL)
    {
        pStringLocation1 = pStringLocation1 + strlen (xml_signal_id) + 2;
        signalCount++;
    }

    m_RtdmXmlData.signal_count = signalCount;
    requiredMemorySize = sizeof(SignalDescription) * signalCount;

    /* allocate memory */
    m_RtdmXmlData.signalDesription = (SignalDescription *) calloc (requiredMemorySize,
                    sizeof(UINT8));

    /***********************************************************************************************************************/
    /* start loop for finding signal Id's */
    /* This section determines which PCU variable are included in the stream sample and data recorder */
    /* find signal_id */
    signalCount = 0;
    pStringLocation1 = startLocation;
    while ((pStringLocation1 = strstr (pStringLocation1, xml_signal_id)) != NULL)
    {
        /* find signal_id */
        pStringLocation1 = strstr (pStringLocation1, xml_signal_id);
        /* move pointer to id # */
        pStringLocation1 = pStringLocation1 + strlen (xml_signal_id) + 2;

        i = 0;
        memset (tempStr, 0, 255);
        while (pStringLocation1[i] != '\"')
        {
            tempStr[i] = pStringLocation1[i];
            i++;
        }

        /* convert signal_id to a # and save as int */
        sscanf (tempStr, "%u", (UINT32 *) &signalId);
        m_RtdmXmlData.signalDesription[signalCount].id = signalId;

        /* Get the variable name and map it to the actual variable address */
        pStringLocation1 = strstr (pStringLocation1, xml_name);
        /* move pointer to dataType */
        pStringLocation1 = pStringLocation1 + strlen (xml_name) + 2;

        /* Get the XML signal name */
        i = 0;
        memset (tempStr, 0, 255);
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
            /* TODO handle error - variable name not found in current index */
        }

        /* Get the data type */
        pStringLocation1 = strstr (pStringLocation1, xml_dataType);
        /* move pointer to dataType */
        pStringLocation1 = pStringLocation1 + strlen (xml_dataType) + 2;

        memset (tempStr, 0, 255);
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
            m_RtdmXmlData.signalDesription[signalCount].signalType = UINT8_XML_TYPE;
        }

        /* Running calculation of the max amount of memory required if all signals change  */
        dataAllocationBytes += sizeof(UINT16);
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

    m_RtdmXmlData.dataAllocationSize = dataAllocationBytes;

    /* End loop for finding signal ID's */
    return signalCount;
}

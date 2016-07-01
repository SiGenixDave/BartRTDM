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
#ifdef TEST_ON_PC
#include "MyTypes.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RTDM_Stream_ext.h"
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
RtdmXmlStr RtdmXmlData;


/* dynamically allocated buffer which contains the whole RTDMConfiguration_PCU.xml file */
static char *m_ConfigXmlBufferPtr = NULL;


const XMLConfigReader m_XmlConfigReader[] =
{
{ "id", INTEGER_DTYPE, &RtdmXmlData.DataRecorderCfgID, NO_CONFIG_ID },
{ "version", INTEGER_DTYPE, &RtdmXmlData.DataRecorderCfgVersion, NO_VERSION },
{ "samplingRate", INTEGER_DTYPE, &RtdmXmlData.SamplingRate, NO_SAMPLING_RATE },
{ "compressionEnabled", BOOLEAN_DTYPE, &RtdmXmlData.Compression_enabled,
NO_COMPRESSION_ENABLED },
{ "minRecordingRate", INTEGER_DTYPE, &RtdmXmlData.MinRecordingRate,
NO_MIN_RECORD_RATE },
{ "DataLogFileCfg enabled", BOOLEAN_DTYPE, &RtdmXmlData.DataLogFileCfg_enabled,
NO_DATALOG_CFG_ENABLED },
{ "filesCount", INTEGER_DTYPE, &RtdmXmlData.FilesCount, NO_FILES_COUNT },
{ "numberSamplesInFile", INTEGER_DTYPE, &RtdmXmlData.NumberSamplesInFile,
NO_NUM_SAMPLES_IN_FILE },
{ "filesFullPolicy", FILESFULL_DTYPE, &RtdmXmlData.FilesFullPolicy,
NO_FILE_FULL_POLICY },
{
    "numberSamplesBeforeSave", INTEGER_DTYPE,
    &RtdmXmlData.NumberSamplesBeforeSave,
    NO_NUM_SAMPLES_BEFORE_SAVE },
{ "maxTimeBeforeSaveMs", INTEGER_DTYPE, &RtdmXmlData.MaxTimeBeforeSaveMs,
NO_MAX_TIME_BEFORE_SAVE },
{ "OutputStreamCfg enabled", BOOLEAN_DTYPE, &RtdmXmlData.OutputStream_enabled,
NO_OUTPUT_STREAM_ENABLED },
{ "comId", U32_DTYPE, &RtdmXmlData.comId,
NO_COMID },

{ "bufferSize", INTEGER_DTYPE, &RtdmXmlData.bufferSize,
NO_BUFFERSIZE },

{ "maxTimeBeforeSendMs", INTEGER_DTYPE, &RtdmXmlData.maxTimeBeforeSendMs,
NO_MAX_TIME_BEFORE_SEND },

};

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static int OpenXMLConfigurationFile (char **configFileXMLBufferPtr);
static int OpenFileTrackerFile (void);
static UINT16 ProcessXmlFileParams (char *pStringLocation1, int index);
static int FindSignals (char* pStringLocation1);


/*********************************************************************************************************
 Open RTDMConfiguration_PCU.xml
 copy contents to a temp buffer (calloc)
 close file
 write RTDMConfiguration_PCU.xml file to rtdm.dan file.  This is for the data recording portion of rtdm.
 Check for Num of Streams in .dan file.  This means a restart of a previous data recorder
 parse temp buffer for needed data
 free temp buffer
 ***********************************************************************************************************/
int ReadXmlFile (void)
{
    char xml_DataRecorderCfg[] = "DataRecorderCfg";
    char *pStringLocation1 = NULL;
    int signal_count = 0;
    unsigned int i = 0;
    int returnValue;

    // Try to open XML configuration file
    returnValue = OpenXMLConfigurationFile (&m_ConfigXmlBufferPtr);
    if (returnValue != NO_ERROR)
    {
        return returnValue;
    }

    // open file that maintains the file tracker to determine where to start writing
    returnValue = OpenFileTrackerFile ();
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
        UINT16 index = 0;
        UINT16 errorCode = 0;
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
        RtdmXmlData.MinRecordingRate /= 1000;
        /* Time must be a minimum of 1 second */
        if (RtdmXmlData.MinRecordingRate < 1)
        {
            RtdmXmlData.MinRecordingRate = 1;
        }

        /* Convert from msecs to seconds */
        RtdmXmlData.MaxTimeBeforeSaveMs /= 1000;
        /* Time must be a minimum of 1 second */
        if (RtdmXmlData.MaxTimeBeforeSaveMs < 1)
        {
            RtdmXmlData.MaxTimeBeforeSaveMs = 1;
        }

        /* Convert from msecs to seconds */
        RtdmXmlData.maxTimeBeforeSendMs /= 1000;
        /* Time must be a minimum of 2 seconds, do not want to overload the CPU */
        if (RtdmXmlData.maxTimeBeforeSendMs < 2)
        {
            RtdmXmlData.maxTimeBeforeSendMs = 2;
        }

        if (RtdmXmlData.bufferSize < 2000)
        {
            /* buffer size is not big enough, will overload the CPU */
            free (m_ConfigXmlBufferPtr);
            return (NO_BUFFERSIZE);
        }

        /***********************************************************************************************************************/
        /* start loop for finding signal Id's */
        /* This section determines which PCU variable are included in the stream sample and data recorder */
        /* find signal_id */
        signal_count = FindSignals (pStringLocation1);
        /***********************************************************************************************************************/
    }
    else
    {
        /* DataRecorderCfg string not found */
        /* free the memory we used for the buffer */
        free (m_ConfigXmlBufferPtr);

        return (NO_DATARECORDERCFG);
    }

    /* free the memory we used for the buffer */
    free (m_ConfigXmlBufferPtr);

    /* Calculate the sample size as read from the config.xml file */
    /* don't care about the order (ID#), just the total bytes */
    for (i = 0; i < signal_count; i++)
    {
        /* loop thru each signal found and get the size */
        RtdmXmlData.sample_size += RtdmXmlData.signal_id_size[i];

        /* Add 2 bytes to cover the SignalId field */
        RtdmXmlData.sample_size += 2;
    }

    /* Add sample header size */
    RtdmXmlData.sample_size += SAMPLE_HEADER_SIZE;

    if (signal_count <= 0)
    {
        /* No signals found */
        return (NO_SIGNALS);
    }

    /* no errors */
    return (NO_ERROR);
}

static int OpenXMLConfigurationFile (char **configFileXMLBufferPtr)
{
    FILE* filePtr = NULL;
    long numbytes;

    /* open the existing configuration file for reading TEXT MODE */
    if (os_io_fopen (RTDM_XML_FILE, "r", &filePtr) != ERROR)
    {
        /* Get the number of bytes */
        fseek (filePtr, 0L, SEEK_END);
        numbytes = ftell (filePtr);

        /* reset the file position indicator to the beginning of the file */
        fseek (filePtr, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text - clear to zero */
        *configFileXMLBufferPtr = (char*) calloc (numbytes, sizeof(char));

        /* memory error */
        if (configFileXMLBufferPtr == NULL)
        {
            os_io_fclose(filePtr);

            /* free the memory we used for the buffer */
            free (*configFileXMLBufferPtr);
            return (BAD_READ_BUFFER);
        }

        /* copy all the text into the buffer */
        fread (*configFileXMLBufferPtr, sizeof(char), numbytes, filePtr);

        /* Close the file, no longer needed */
        os_io_fclose(filePtr);
    }
    /* File does not exist or internal error */
    else
    {
        printf ("Can't Open RTDMConfiguration_PCU.xml or file doesn't exist\n");
        return (NO_XML_INPUT_FILE);
    }

    return (NO_ERROR);
}

static UINT16 ProcessXmlFileParams (char *pStringLocation1, int index)
{

    UINT16 errorCode = NO_ERROR;
    char tempArray[10];

    char *pStringLocation2 = strstr (pStringLocation1,
                    m_XmlConfigReader[index].subString);

    if (pStringLocation2 != NULL)
    {
        /* move pointer to id # */
        pStringLocation2 = pStringLocation2
                        + strlen (m_XmlConfigReader[index].subString) + 2;

        switch (m_XmlConfigReader[index].dataType)
        {
            case INTEGER_DTYPE:
                sscanf (pStringLocation2, "%d",
                                (int *) m_XmlConfigReader[index].xmlData);
                break;

            case U32_DTYPE:
                sscanf (pStringLocation2, "%lu",
                                (unsigned long *) m_XmlConfigReader[index].xmlData);
                break;

            case BOOLEAN_DTYPE:
                strncpy (tempArray, pStringLocation2, 5);
                if (strncmp (tempArray, "TRUE", 4) == 0)
                {
                    *(uint8_t *) m_XmlConfigReader[index].xmlData = TRUE;
                }
                else
                {
                    *(uint8_t *) m_XmlConfigReader[index].xmlData = FALSE;
                }
                break;

            case FILESFULL_DTYPE:
                strncpy (tempArray, pStringLocation2, 5);
                if (strncmp (tempArray, "FIFO", 4) == 0)
                {
                    *(uint8_t *) m_XmlConfigReader[index].xmlData = FIFO_POLICY;
                }
                else
                {
                    *(uint8_t *) m_XmlConfigReader[index].xmlData = STOP_POLICY;
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

static int OpenFileTrackerFile (void)
{
    //TODO
    // Check if file exists, if not create it

    // If file exists, verify contents

    return (NO_ERROR);
}

static int FindSignals (char* pStringLocation1)
{
    const char xml_signal_id[] = "Signal id";
    const char xml_dataType[] = "dataType";
    const char xml_uint32[] = "UINT3";
    const char xml_uint16[] = "UINT1";
    const char xml_uint8[] = "UINT8";
    const char xml_int16[] = "INT16";
    const char xml_int32[] = "INT32";

    uint16_t signal_count = 0;
    uint16_t signalId = 0;
    int16_t dataType;

    char temp_array[5];

    /***********************************************************************************************************************/
    /* start loop for finding signal Id's */
    /* This section determines which PCU variable are included in the stream sample and data recorder */
    /* find signal_id */
    while ((pStringLocation1 = strstr (pStringLocation1, xml_signal_id)) != NULL)
    {
        /* find signal_id */
        pStringLocation1 = strstr (pStringLocation1, xml_signal_id);
        /* move pointer to id # */
        pStringLocation1 = pStringLocation1 + strlen(xml_signal_id) + 2;
        /* convert signal_id to a # and save as int */
        sscanf (pStringLocation1, "%u", &signalId);
        RtdmXmlData.signal_id_num[signal_count] = signalId;
        /* find dataType */
        pStringLocation1 = strstr (pStringLocation1, xml_dataType);
        /* move pointer to dataType */
        pStringLocation1 = pStringLocation1 + strlen(xml_dataType) + 2;
        /* dataType is of type char, this needs converted to a # which will be used to calculate the MAX_BUFFER_SIZE */
        strncpy (temp_array, pStringLocation1, 5);
        if ((strncmp (temp_array, xml_uint32, 5) == 0)
                        || (strncmp (temp_array, xml_int32, 5) == 0))
        {
            dataType = 4;
        }
        else if ((strncmp (temp_array, xml_uint16, 5) == 0)
                        || (strncmp (temp_array, xml_int16, 5) == 0))
        {
            dataType = 2;
        }
        else if ((strncmp (temp_array, xml_uint8, 5) == 0))
        {
            dataType = 1;
        }
        else
        {
            dataType = 1; /* default 1 */
        }

        RtdmXmlData.signal_id_size[signal_count] = dataType;
        /* Count # of signals found in config.xml file */
        signal_count++;
        RtdmXmlData.signal_count = signal_count;
    }
    /* End loop for finding signal ID's */
    return signal_count;
}

#if DAS
void ReferenceOnly(void)
{
    FILE* p_file = NULL;
    long numbytes;
    char xml_RTDMCfg[] = "</RtdmCfg>";
    char *pStringLocation = NULL;
    char line[300];
    UINT8 line_count = 0;

    //DAS Saved for later; not needed but saved for reference since we're going to be using multiple files

    /********************************************************************************************/
    /* Write the above RTDMConfiguration_PCU.xml file to the data recorder file rtdm.dan */
    /* This is the required header for the rtdm.dan file */
    /* The header will be written for a new device or if device was formatted to clear data */
    /********************************************************************************************/
    /* open file to check if the header is present */
    /* 'ab+' is append, open, or create binary file */
    if (os_io_fopen (RTDM_DATA_FILE, "ab+", &p_file) != ERROR)
    {
        /* Get the number of bytes */
        fseek (p_file, 0L, SEEK_END);
        numbytes = ftell (p_file);

        if (numbytes == 0)
        {
            /* file does not exist, so we need to write the .xml header */
            /* write RTDMConfiguration_PCU.xml to rtdm.dan, this is the required header (text format, not binary) */
            fprintf (p_file, "%s\n", m_ConfigXmlBufferPtr);

            /* Get location of RTDM Header for future writes */
            fseek (p_file, 0L, SEEK_END);
            DataLog_Info_str.RTDM_Header_Location_Offset = ftell (p_file);
            DataLog_Info_str.xml_header_is_present = TRUE;
        }
        else
        {
            /* Get location of RTDM Header for future writes */
            /* Go to beginning of file */
            fseek (p_file, 0L, SEEK_SET);

            while (fgets (line, 300, p_file) != NULL)
            {
                /* Find "</RtdmCfg>" in .dan file, the next line is the start of the RTDM header */
                pStringLocation = strstr (line, xml_RTDMCfg);

                if (pStringLocation != NULL)
                {
                    /* Cursor is now at the position 'R' of RTDM, this will be the start of the "RTDM" header */
                    fseek (p_file, 0, SEEK_CUR);

                    /* Get location of RTDM Header for future writes */
                    DataLog_Info_str.RTDM_Header_Location_Offset = ftell (
                                    p_file);
                    break;
                }

                line_count++;
            }

            /* .xml header is present so need to retrieve 1st timestamp and num streams from .dan */
            /* still should check for </RtdmCfg> */
            DataLog_Info_str.RTDMDataLogWriteState = RESTART;
        }

        os_io_fclose(p_file);
    }
    else
    {
        /* Do not return here - still need to get data from .xml config file and do the streaming */
        error_code_dan = OPEN_FAIL;
        /* Log fault if persists */
    }

}
#endif



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
typedef enum {
    BOOLEAN_DTYPE,
    UINT16_DTYPE
} XmlDataType;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
typedef struct {
    char *subString;
    UINT16 stringffset;
    XmlDataType dataType;

} XMLConfigReader;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static int OpenXMLConfigurationFile (char **configFileXMLBufferPtr);
static int OpenFileTrackerFile (void);

/*********************************************************************************************************
 Open RTDMConfiguration_PCU.xml
 copy contents to a temp buffer (calloc)
 close file
 write RTDMConfiguration_PCU.xml file to rtdm.dan file.  This is for the data recording portion of rtdm.
 Check for Num of Streams in .dan file.  This means a restart of a previous data recorder
 parse temp buffer for needed data
 free temp buffer
 ***********************************************************************************************************/
int ReadXmlFile (RtdmXmlStr *rtdmXmlData)
{
    FILE* p_file = NULL;
    long numbytes;

    char *configXmlBufferPtr = NULL; /* buffer which contains the whole RTDMConfiguration_PCU.xml file */
    char xml_DataRecorderCfg[] = "DataRecorderCfg";
    char xml_id[] = "id";
    char xml_version[] = "version";
    char xml_samplingRate[] = "samplingRate";
    char xml_compressionEnabled[] = "compressionEnabled";
    char xml_minRecordingRate[] = "minRecordingRate";
    char xml_DataLogFileCfg_enabled[] = "DataLogFileCfg enabled";
    char xml_filesCount[] = "filesCount";
    char xml_numberSamplesInFile[] = "numberSamplesInFile";
    char xml_filesFullPolicy[] = "filesFullPolicy";
    char xml_numberSamplesBeforeSave[] = "numberSamplesBeforeSave";
    char xml_maxTimeBeforeSaveMs[] = "maxTimeBeforeSaveMs";
    char xml_OutputStreamCfg_enabled[] = "OutputStreamCfg enabled";
    char xml_comID[] = "comId";
    char xml_bufferSize[] = "bufferSize";
    char xml_maxTimeBeforeSendMs[] = "maxTimeBeforeSendMs";
    char xml_signal_id[] = "Signal id";
    char xml_dataType[] = "dataType";
    char xml_uint32[] = "UINT3";
    char xml_uint16[] = "UINT1";
    char xml_uint8[] = "UINT8";
    char xml_int16[] = "INT16";
    char xml_int32[] = "INT32";
    char xml_true[] = "TRUE";
    char xml_fifo[] = "FIFO";
    char *pStringLocation1 = NULL;
    char *pStringLocation2 = NULL;
    char *pStringLocation3 = NULL;
    char temp_array[5];
    int signal_count = 0;
    int DataRecorderCfgID = 0;
    int DataRecorderCfgVersion = 0;
    unsigned int samplingRate = 0;
    unsigned int maxTimeBeforeSend = 0;
    unsigned int minRecordingRate = 0;
    unsigned int maxTimeBeforeSaveMs = 0;
    unsigned int filesCount = 0;
    unsigned int numberSamplesInFile = 0;
    unsigned int numberSamplesBeforeSave = 0;
    unsigned int bufferSize = 0;
    unsigned long comID = 0;
    unsigned int signalId = 0;
    int dataType = 0;
    unsigned int i = 0;
    UINT32 buff_32[1];
    UINT32 *ptr_32 = NULL;
    UINT16 buff_16[1];
    UINT16 *ptr_16 = NULL;
    char xml_RTDMCfg[] = "</RtdmCfg>";
    char *pStringLocation = NULL;
    char line[300];
    UINT8 line_count = 0;
    static long RTDM_Header_Location_Offset = 0;
    int returnValue;

    // Try to open XML configuration file
    returnValue = OpenXMLConfigurationFile (&configXmlBufferPtr);
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
        mon_broadcast_printf("numbytes = %d\n",numbytes); /* Test Print */

        /* if numbytes == 0 then file does not exist, so we need to write the .xml header */
        if (numbytes == 0)
        {
            /* write RTDMConfiguration_PCU.xml to rtdm.dan, this is the required header (text format, not binary) */
            fprintf (p_file, "%s\n", configXmlBufferPtr);

            /* Get location of RTDM Header for future writes */
            fseek (p_file, 0L, SEEK_END);
            DataLog_Info_str.RTDM_Header_Location_Offset = ftell (p_file);
            mon_broadcast_printf("Header Location from .xml (xml header is not present) %ld\n", DataLog_Info_str.RTDM_Header_Location_Offset);

            DataLog_Info_str.xml_header_is_present = TRUE;
        }
        else
        {
            /* Get location of RTDM Header for future writes */
            /* Go to bEGINNING of file */
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
                    mon_broadcast_printf("Header Location from .xml (xml header is present) %ld\n", DataLog_Info_str.RTDM_Header_Location_Offset);

                    break;
                }

                line_count++;
                /* mon_broadcast_printf("Line Count %d\n", line_count); */
            }

            /* .xml header is present so need to retrieve 1st timestamp and num streams from .dan */
            /* still should check for </RtdmCfg> */
            DataLog_Info_str.RTDMDataLogWriteState = RESTART;
        }

        os_io_fclose(p_file);
    }
    else
    {
        mon_broadcast_printf("RTDM_read_xml.c - Could Not Open rtdm.dan file %s For Writing\n", RTDM_DATA_FILE); /* Test Print */
        /* Do not return here - still need to get data from .xml config file and do the streaming */
        error_code_dan = OPEN_FAIL;
        /* Log fault if persists */
    }

    /***********************************************************************************************************************/
    /* This is where we extract the data from the config.xml file which gets sent to the stream data */
    /* DataRecorderCfg, id, version, enabled, and maxtime */
    /* Find DataRecorderCfg - pointer to text "DataRecorderCfg", pointer contains all remaining text in the file */
    pStringLocation1 = strstr (configXmlBufferPtr, xml_DataRecorderCfg);

    if (pStringLocation1 != NULL)
    {
        /***************** Find "id" ********************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_id);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 4;
            /*mon_broadcast_printf("The word is %s\n", pStringLocation2);*/
            /* copy id # as int */
            sscanf (pStringLocation2, "%d", &DataRecorderCfgID);
            rtdmXmlData->DataRecorderCfgID = DataRecorderCfgID;
            /* mon_broadcast_printf("The id is %d\n", DataRecorderCfgID);  Test Print */
            /* mon_broadcast_printf("The id str is %d\n", rtdmXmlData->DataRecorderCfgID);  Test Print */
        }
        else
        {
            /* No config ID return error */
            free (configXmlBufferPtr);
            return (NO_CONFIG_ID);
        }

        /**************** Find "version" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_version);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 9;
            /* copy id # as int */
            sscanf (pStringLocation2, "%d", &DataRecorderCfgVersion);
            rtdmXmlData->DataRecorderCfgVersion = DataRecorderCfgVersion;
            /* mon_broadcast_printf("The version str is %d\n", rtdmXmlData->DataRecorderCfgVersion);  Test Print */
        }
        else
        {
            /* No config version return error */
            free (configXmlBufferPtr);
            return (NO_VERSION);
        }

        /**************** Find "samplingRate" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_samplingRate);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 14;
            /* copy id # as uint */
            sscanf (pStringLocation2, "%d", &samplingRate);
            rtdmXmlData->SamplingRate = samplingRate;
            /*mon_broadcast_printf("The SamplingRate str is %d\n", rtdmXmlData->SamplingRate);   Test Print */
        }
        else
        {
            /* No SamplingRate return error */
            free (configXmlBufferPtr);
            return (NO_SAMPLING_RATE);
        }

        /*************** Find compressionEnabled enabled ****************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_compressionEnabled);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to TRUE/FALSE */
            pStringLocation2 = pStringLocation2 + 20;
            /*mon_broadcast_printf("The location is %s\n", pStringLocation2);*/
            strncpy (temp_array, pStringLocation2, 5);
            if (strncmp (temp_array, xml_true, 4) == 0)
            {
                rtdmXmlData->Compression_enabled = TRUE;
                /*mon_broadcast_printf("Compression_enabled is %s - %d\n", temp_array,rtdmXmlData->Compression_enabled);	 Test Print */
            }
            else
            {
                rtdmXmlData->Compression_enabled = FALSE;
                /*mon_broadcast_printf("Compression_enabled FALSE is %s - %d\n", temp_array,rtdmXmlData->Compression_enabled);	 Test Print */
            }
        }
        else
        {
            /* No compressionEnabled return error */
            free (configXmlBufferPtr);
            return (NO_COMPRESSION_ENABLED);
        }

        /**************** Find "minRecordingRate" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_minRecordingRate);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 18;
            /* copy id # as int */
            sscanf (pStringLocation2, "%d", &minRecordingRate);
            /* Convert from mS to Seconds */
            minRecordingRate = minRecordingRate / 1000;
            /* Time must be a minimum of 1 second */
            if (minRecordingRate < 1)
            {
                minRecordingRate = 1;
            }
            rtdmXmlData->MinRecordingRate = minRecordingRate;
            /* mon_broadcast_printf("The minRecordingRate is %d\n", minRecordingRate);  Test Print */
            /*mon_broadcast_printf("The MinRecordingRate str is %d\n", rtdmXmlData->MinRecordingRate);  Test Print */
        }
        else
        {
            /* No minRecordingRate return error */
            free (configXmlBufferPtr);
            return (NO_MIN_RECORD_RATE);
        }

        /*************** Find DataLogFileCfg enabled ****************************************/
        pStringLocation2 = strstr (pStringLocation1,
                xml_DataLogFileCfg_enabled);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to TRUE/FALSE */
            pStringLocation2 = pStringLocation2 + 24;
            /*mon_broadcast_printf("The location is %s\n", pStringLocation2);*/
            strncpy (temp_array, pStringLocation2, 5);
            if (strncmp (temp_array, xml_true, 4) == 0)
            {
                rtdmXmlData->DataLogFileCfg_enabled = TRUE;
                /*mon_broadcast_printf("DataLogFileCfg_enabled is %s - %d\n", temp_array,rtdmXmlData->DataLogFileCfg_enabled);	 Test Print */
            }
            else
            {
                rtdmXmlData->DataLogFileCfg_enabled = FALSE;
                /*mon_broadcast_printf("DataLogFileCfg_enabled FALSE is %s - %d\n", temp_array,rtdmXmlData->DataLogFileCfg_enabled);	 Test Print */
            }
        }
        else
        {
            /* No DataLogFileCfg_enabled error */
            free (configXmlBufferPtr);
            return (NO_DATALOG_CFG_ENABLED);
        }

        /**************** Find "filesCount" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_filesCount);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 12;
            /* copy id # as uint */
            sscanf (pStringLocation2, "%d", &filesCount);
            rtdmXmlData->FilesCount = filesCount;
            /*mon_broadcast_printf("The FilesCount str is %d\n", rtdmXmlData->FilesCount);   Test Print */
        }
        else
        {
            /* No FilesCount return error */
            free (configXmlBufferPtr);
            return (NO_FILES_COUNT);
        }

        /*************** Find numberSamplesInFile ****************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_numberSamplesInFile);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 21;
            /* copy id # as uint */
            sscanf (pStringLocation2, "%d", &numberSamplesInFile);
            rtdmXmlData->NumberSamplesInFile = numberSamplesInFile;
            /*mon_broadcast_printf("The NumberSamplesInFile str is %d\n", rtdmXmlData->NumberSamplesInFile);   Test Print */
        }
        else
        {
            /* No numberSamplesInFile return error */
            free (configXmlBufferPtr);
            return (NO_NUM_SAMPLES_IN_FILE);
        }

        /*************** Find filesFullPolicy ****************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_filesFullPolicy);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to FIFO/STOP */
            pStringLocation2 = pStringLocation2 + 17;
            /*mon_broadcast_printf("The location is %s\n", pStringLocation2);*/
            strncpy (temp_array, pStringLocation2, 5);
            if (strncmp (temp_array, xml_fifo, 4) == 0)
            {
                rtdmXmlData->FilesFullPolicy = FIFO_POLICY;
                /*mon_broadcast_printf("FilesFullPolicy is %s - %d\n", temp_array,rtdmXmlData->FilesFullPolicy);	 Test Print */
            }
            else
            {
                rtdmXmlData->FilesFullPolicy = STOP_POLICY;
                /*mon_broadcast_printf("FilesFullPolicy is %s - %d\n", temp_array,rtdmXmlData->FilesFullPolicy);	 Test Print */
            }
        }
        else
        {
            /* No FilesFullPolicy error */
            free (configXmlBufferPtr);
            return (NO_FILE_FULL_POLICY);
        }

        /*************** Find numberSamplesBeforeSave ****************************************/
        pStringLocation2 = strstr (pStringLocation1,
                xml_numberSamplesBeforeSave);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 25;
            /* copy id # as uint */
            sscanf (pStringLocation2, "%d", &numberSamplesBeforeSave);
            rtdmXmlData->NumberSamplesBeforeSave = numberSamplesBeforeSave;
            /*mon_broadcast_printf("The NumberSamplesBeforeSave str is %d\n", rtdmXmlData->NumberSamplesBeforeSave);   Test Print */
        }
        else
        {
            /* No numberSamplesBeforeSave return error */
            free (configXmlBufferPtr);
            return (NO_NUM_SAMPLES_BEFORE_SAVE);
        }

        /**************** Find "maxTimeBeforeSaveMs" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_maxTimeBeforeSaveMs);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 21;
            /* copy id # as uint */
            sscanf (pStringLocation2, "%d", &maxTimeBeforeSaveMs);
            /* Convert from mS to Seconds */
            maxTimeBeforeSaveMs = maxTimeBeforeSaveMs / 1000;
            /* Time must be a minimum of 1 second */
            if (maxTimeBeforeSaveMs < 1)
            {
                maxTimeBeforeSaveMs = 1;
            }
            rtdmXmlData->MaxTimeBeforeSaveMs = maxTimeBeforeSaveMs;
            /* mon_broadcast_printf("The MaxTimeBeforeSaveMs is %d\n", MaxTimeBeforeSaveMs);  Test Print */
            /*mon_broadcast_printf("The MaxTimeBeforeSaveMs str is %d\n", rtdmXmlData->MaxTimeBeforeSaveMs);  Test Print */
        }
        else
        {
            /* No maxTimeBeforeSaveMs return error */
            free (configXmlBufferPtr);
            return (NO_MAX_TIME_BEFORE_SAVE);
        }

        /*************** Find OutputStreamCfg enabled ****************************************/
        pStringLocation2 = strstr (pStringLocation1,
                xml_OutputStreamCfg_enabled);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to TRUE/FALSE */
            pStringLocation2 = pStringLocation2 + 25;
            /*mon_broadcast_printf("The location is %s\n", pStringLocation2);*/
            strncpy (temp_array, pStringLocation2, 5);
            if (strncmp (temp_array, xml_true, 4) == 0)
            {
                rtdmXmlData->OutputStream_enabled = TRUE;
                /* mon_broadcast_printf("OutputStream_enabled is %s - %d\n", temp_array,rtdmXmlData->OutputStream_enabled);	 Test Print */
            }
            else
            {
                rtdmXmlData->OutputStream_enabled = FALSE;
                /* mon_broadcast_printf("OutputStream_enabled FALSE is %s - %d\n", temp_array,rtdmXmlData->OutputStream_enabled);	 Test Print */
            }
        }
        else
        {
            /* No stream output enabled return error */
            free (configXmlBufferPtr);
            return (NO_OUTPUT_STREAM_ENABLED);
        }

        /*************** Find comID ********************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_comID);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to comID */
            pStringLocation2 = pStringLocation2 + 7;

            /* copy comid # as long */
            sscanf (pStringLocation2, "%lu", &comID);
            rtdmXmlData->comId = comID;
            /* mon_broadcast_printf("The comID is %lu\n", rtdmXmlData->comId);   Test Print */
        }
        else
        {
            /* No stream output enabled return error */
            free (configXmlBufferPtr);
            return (NO_COMID);
        }

        /*************** Find bufferSize ********************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_bufferSize);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to bufferSize */
            pStringLocation2 = pStringLocation2 + 12;

            /* copy bufferSize # as int */
            sscanf (pStringLocation2, "%d", &bufferSize);
            rtdmXmlData->bufferSize = bufferSize;
            /* mon_broadcast_printf("The buffersize is %d\n", rtdmXmlData->bufferSize);   Test Print */
            if (bufferSize < 2000)
            {
                /* buffer size is not big enough, will overload the CPU */
                free (configXmlBufferPtr);
                return (NO_BUFFERSIZE);
            }
        }
        else
        {
            /* No stream output enabled return error */
            free (configXmlBufferPtr);
            return (NO_BUFFERSIZE);
        }

        /**************** Find "maxTimeBeforeSendMs" *****************************************************/
        pStringLocation2 = strstr (pStringLocation1, xml_maxTimeBeforeSendMs);
        if (pStringLocation2 != NULL)
        {
            /* move pointer to id # */
            pStringLocation2 = pStringLocation2 + 21;
            /* copy id # as int */
            sscanf (pStringLocation2, "%d", &maxTimeBeforeSend);
            /* Convert from mS to Seconds */
            maxTimeBeforeSend = maxTimeBeforeSend / 1000;
            /* Time must be a minimum of 2 seconds, do not want to overload the CPU */
            if (maxTimeBeforeSend < 2)
            {
                maxTimeBeforeSend = 2;
            }
            rtdmXmlData->maxTimeBeforeSendMs = maxTimeBeforeSend;
            /* mon_broadcast_printf("The maxTimeBeforeSendMs is %d\n", maxTimeBeforeSend);  Test Print */
            /* mon_broadcast_printf("The maxTimeBeforeSendMs str is %d\n", rtdmXmlData->maxTimeBeforeSendMs);  Test Print */
        }
        else
        {
            /* No MaxTime return error */
            free (configXmlBufferPtr);
            return (NO_MAX_TIME_BEFORE_SEND);
        }

        /***********************************************************************************************************************/
        /***********************************************************************************************************************/

        /* Set pointer to remainder of the file/string */
        pStringLocation3 = pStringLocation2;

        /***********************************************************************************************************************/
        /* start loop for finding signal Id's */
        /* This section determines which PCU variable are included in the stream sample and data recorder */
        /* find signal_id */
        while ((pStringLocation3 = strstr (pStringLocation3, xml_signal_id))
                != NULL)
        {
            /* find signal_id */
            pStringLocation3 = strstr (pStringLocation3, xml_signal_id);
            /*printf("pStringLocation3 contains this text\n\n%s", pStringLocation3);*/
            /* move pointer to id # */
            pStringLocation3 = pStringLocation3 + 11;
            /* convert signal_id to a # and save as int */
            sscanf (pStringLocation3, "%d", &signalId);
            rtdmXmlData->signal_id_num[signal_count] = signalId;
            /* mon_broadcast_printf("Signal ID str[%d]  id=%d\n", signal_count, rtdmXmlData->signal_id_num[signal_count]);  Test Print */

            /* find dataType */
            pStringLocation3 = strstr (pStringLocation3, xml_dataType);
            /* move pointer to dataType */
            pStringLocation3 = pStringLocation3 + 10;
            /* dataType is of type char, this needs converted to a # which will be used to calculate the MAX_BUFFER_SIZE */
            strncpy (temp_array, pStringLocation3, 5);
            /* mon_broadcast_printf("The dataType is %s\n", temp_array);  Test Print */

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

            rtdmXmlData->signal_id_size[signal_count] = dataType;
            /* mon_broadcast_printf("The dataType[] size is %d\n", rtdmXmlData->signal_id_size[signal_count]);  Test Print */

            /* Count # of signals found in config.xml file */
            signal_count++;
            rtdmXmlData->signal_count = signal_count;

        } /* End loop for finding signal ID's */
        /***********************************************************************************************************************/
    }
    else
    {
        /* DataRecorderCfg string not found */
        /* free the memory we used for the buffer */
        free (configXmlBufferPtr);

        return (NO_DATARECORDERCFG);
    }

    /* free the memory we used for the buffer */
    free (configXmlBufferPtr);

    /* Calculate the sample size as read from the config.xml file */
    /* don't care about the order (ID#), just the total bytes */
    for (i = 0; i < signal_count; i++)
    {
        /* loop thru each signal found and get the size */
        rtdmXmlData->sample_size += rtdmXmlData->signal_id_size[i];

        /* Add 2 bytes to cover the SignalId field */
        rtdmXmlData->sample_size += 2;
    }

    /* Add sample header size */
    rtdmXmlData->sample_size += SAMPLE_HEADER_SIZE;

    /* mon_broadcast_printf("signal_count = %d i = %d\n",signal_count,i);			 Test Print */
    /* mon_broadcast_printf("The sample size = %d\n",rtdmXmlData->sample_size);	 Test Print */

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

static int OpenFileTrackerFile (void)
{
    //TODO
    // Check if file exists, if not create it

    // If file exists, verify contents

    return (NO_ERROR);
}


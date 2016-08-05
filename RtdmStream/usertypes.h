/*****************************************************************************
 *  COPYRIGHT   : (c) 2016 Bombardier Inc. or its subsidiaries
 *****************************************************************************
 *
 *  MODULE      : usertypes.h
 *
 *  ABSTRACT    : Type definitions for the user-defined data types
 *
 *  CREATOR     : PMAKE 5.5.0.4
 *
 *  REMARKS     : ANY CHANGES TO THIS FILE WILL BE LOST !!!
 *
 ****************************************************************************/

#ifndef _USERTYPES_H
#define _USERTYPES_H

#include "../RtdmStream/mwt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	MWT_UINT             LcuChannelStatus;
	MWT_UINT             VatcChannelStatus;
	MWT_UINT             NetStatus;
} Tcn_NetStat_Str;
#define MWT_Tcn_NetStat_Str Tcn_NetStat_Str

typedef struct
{
	MWT_UINT             LCUCircCnt;
	MWT_UINT             LCUActiveMessageId;
	MWT_UINT             LCUOperatingMode;
	MWT_UINT             LCUJogRevStatus;
	MWT_UINT             LCUJogRevTimer;
	MWT_UINT             LCUCouplingSpdStatus;
	MWT_UINT             LCUCouplingSpd;
	MWT_UINT             LCUCarWshBut;
	MWT_UINT             LCUCarWshSpd;
	MWT_UINT             LCUEmerBackContact;
	MWT_UINT             LCUDirection;
	MWT_UINT             LCUBRK3net;
	MWT_UINT             LCUFSFBnet;
	MWT_UINT             LCUPowerCmd;
	MWT_UINT             LCUBrakeCmd;
	MWT_UINT             LCUMCUHandlePos;
	MWT_UINT             LCUMCUPowerSwitch;
	MWT_UINT             LCUMCUBrakeSwitch;
	MWT_UINT             LCUMCUFullBrakeSwitch;
	MWT_UINT             LCUEM;
	MWT_UINT             LCUSpeedRestriction;
	MWT_UINT             LCUBO_LockedAxle;
	MWT_UINT             LCUBOBP;
	MWT_UINT             LCUADCL;
	MWT_UINT             LCUADCBP;
	MWT_UINT             LCUREB;
} Tcn_LCU_Str;
#define MWT_Tcn_LCU_Str Tcn_LCU_Str

typedef struct
{
	MWT_UINT             VATCCircCnt;
	MWT_UINT             VATCActiveMessageId;
	MWT_UINT             VATCOperatingMode;
	MWT_INT              VATCSpeed;
	MWT_INT              VATCPrevBlckSpeed;
	MWT_INT              VATCOperSpeed;
	MWT_UINT             VATCDirection;
	MWT_UINT             VATCBRK3net;
	MWT_UINT             VATCFSFBnet;
	MWT_UINT             VATCPKOnet;
	MWT_UINT             VATCPowerCmd;
	MWT_UINT             VATCBrakeCmd;
	MWT_UINT             VATCTestMode;
	MWT_UINT             VATCADCL;
	MWT_UINT             VATCADCBP;
} Tcn_VATC_str;
#define MWT_Tcn_VATC_str Tcn_VATC_str

typedef struct
{
	MWT_UINT             ActiveNetworkInd;
} Tcn_Str;
#define MWT_Tcn_Str Tcn_Str

typedef struct
{
	UINT16               event_trig;
	UINT16               confirm_flag;
	UINT16               event_active;
	UINT16               max_num;
	UINT16               mtbf;
} PTU_Flt_Str;
#define MWT_PTU_Flt_Str PTU_Flt_Str

typedef struct
{
	UINT16               var1_u16;
	UINT16               var2_u16;
	UINT16               var3_u16;
	UINT16               var4_u16;
	INT16                var5_16;
	INT16                var6_16;
	INT16                var7_16;
	INT16                var8_16;
	UINT32               var9_u32;
	UINT32               var10_u32;
	UINT32               var11_u32;
	UINT32               var12_u32;
	INT32                var13_32;
	INT32                var14_32;
	INT32                var15_32;
	INT32                var16_32;
	INT32                var17_32;
	INT32                var18_32;
	INT32                var19_32;
	INT32                var20_32;
} PTU_generic_Env_str;
#define MWT_PTU_generic_Env_str PTU_generic_Env_str

typedef PTU_Flt_Str PTU_Flt_Array[101];
#define MWT_PTU_Flt_Array PTU_Flt_Array

typedef PTU_generic_Env_str PTU_Flt_Env_Arrray[101];
#define MWT_PTU_Flt_Env_Arrray PTU_Flt_Env_Arrray

typedef MWT_REAL Buffer32_Real_DCU2[32];
#define MWT_Buffer32_Real_DCU2 Buffer32_Real_DCU2

typedef MWT_INT Buffer32_Int_DCU2[33];
#define MWT_Buffer32_Int_DCU2 Buffer32_Int_DCU2

typedef UINT8 AR_UINT8_29[29];
#define MWT_AR_UINT8_29 AR_UINT8_29

typedef UINT8 usint8_array_16[16];
#define MWT_usint8_array_16 usint8_array_16

typedef MWT_UINT array_uint[32];
#define MWT_array_uint array_uint

typedef MWT_REAL wheel_dia_Array[4];
#define MWT_wheel_dia_Array wheel_dia_Array

typedef INT8 array_11[12];
#define MWT_array_11 array_11

typedef UINT8 usint8_array_8[8];
#define MWT_usint8_array_8 usint8_array_8

typedef UINT8 usint8_array_6[6];
#define MWT_usint8_array_6 usint8_array_6

typedef UINT8 usint8_array_11[11];
#define MWT_usint8_array_11 usint8_array_11

typedef struct
{
	UINT8                IOverSpdLimRd;
	UINT8                IOverSpdLimYd;
	UINT8                IDirCmd;
	UINT8                IDynBrkFadOut;
	UINT8                INoMotion;
	UINT8                Reserved1;
	UINT8                ILineVoltLow;
	UINT8                Reserved2;
	UINT8                ISlfTstInPrgrs;
	UINT8                IWhlSpnDet;
	UINT8                IWhlSldDet;
	UINT8                IWhlSpnCorrInPrg;
	UINT8                IWhlSldCorrInPrg;
	UINT8                ICarID;
	UINT8                IDrClosedLocked;
	UINT8                IDrClosedByp;
	UINT8                IBrkRelSt;
	UINT8                IMCSSModeSel;
	UINT8                IMcuBrkSwitch;
	UINT8                IMCUPwrSwitch;
	UINT8                ITrainLength;
	UINT8                IDynBrkCutOut;
	UINT8                IRegenCutOut;
	UINT8                IDcuState;
	UINT8                IAccelCurveAAct;
	UINT8                IAccelCurveBAct;
	UINT8                IBRK3St;
	UINT8                IPropCutout;
	UINT8                CScContCmd;
	UINT8                IScContStFb;
	UINT8                CCcContCmd;
	UINT8                ICcContStFb;
	UINT8                CZSRelayCommand;
	UINT8                IZsrRelayStFb;
	UINT8                CRunRelayCmd;
	UINT8                IRunRelayStFb;
	UINT8                CHscbCmd;
	UINT8                IHscbCmdStFb;
	UINT8                IMotionDirection;
	UINT8                PRailGapDet;
	UINT8                IZeroSpeed;
	UINT8                IPropSystMode;
	UINT8                ICarWshBtnSts;
	UINT8                ICplgPushBtnStatus;
	UINT8                ICplgSpeedStatus;
	UINT8                IFSFBnetActive;
	UINT8                IJogReverse;
	UINT8                IPKOStatus;
	UINT8                IPKOStatusPKOnet;
	UINT8                IRDCompareStatus;
	UINT8                IRDTimerStatus;
	UINT8                ITractionSafeSts;
	UINT8                ICarWashStatus;
	usint8_array_11      Reserved3;
} DS_1019;
#define MWT_DS_1019 DS_1019

typedef UINT8 usint8_array_3[3];
#define MWT_usint8_array_3 usint8_array_3

typedef UINT8 AR_UINT8_17[17];
#define MWT_AR_UINT8_17 AR_UINT8_17

typedef struct
{
	UINT8                IPwrCmdFb;
	UINT8                IBRakeCmdFb;
	INT16                IRateRequest;
	INT16                IRate;
	UINT8                ICarWashSpdLimi;
	UINT8                IRoadManlSpdLim;
	UINT8                IYardManSpdLim;
	UINT8                Reserved1;
	UINT16               ITachFreq1;
	UINT16               ITachFreq2;
	UINT8                ICouplingSpdLim;
	UINT8                Reserved2;
	INT16                ILineVoltage;
	INT16                ILineCurr;
	INT16                IDiffCurr;
	INT16                IDynBrkResCurr;
	INT16                IDcLinkVoltage;
	INT16                IDcLinkCurr;
	INT16                IHeatSinkTemp;
	INT16                IAmbientTemp;
	UINT8                Reserved3;
	usint8_array_3       Reserved4;
	INT32                CTractEffortReq;
	INT32                ITractEffortDeli;
	UINT16               IBrkResTemp;
	UINT16               IChargResTemp;
	UINT16               ITruckSpeed;
	UINT16               ICarSpeed;
	UINT16               IWheelDiameter1;
	UINT16               IWheelDiameter2;
	UINT32               ITruckLoadWeight;
	INT16                IVcuDbfb;
	INT16                IDcu2Dbfb;
	UINT32               IEnergyConsume;
	UINT32               IEnergyRegen;
	UINT32               ICarWeight;
	UINT16               IMCUHandlePos;
	UINT8                IJogRevTimer;
	AR_UINT8_17          Reserved5;
} DS_1020;
#define MWT_DS_1020 DS_1020

typedef UINT8 AR_UINT8_28[28];
#define MWT_AR_UINT8_28 AR_UINT8_28

typedef struct
{
	UINT32               IOdometer;
	AR_UINT8_28          Reserved1;
} DS_1021;
#define MWT_DS_1021 DS_1021

typedef UINT8 AR_UINT8_9[9];
#define MWT_AR_UINT8_9 AR_UINT8_9

typedef UINT8 AR_UINT8_4[4];
#define MWT_AR_UINT8_4 AR_UINT8_4

typedef struct
{
	UINT8                ITelegrVldtBit;
	UINT8                Reserved1;
	UINT16               ILifeSignInd;
	AR_UINT8_4           Reserved2;
} DS_1023;
#define MWT_DS_1023 DS_1023

typedef UINT8 usint8_array_2[2];
#define MWT_usint8_array_2 usint8_array_2

typedef UINT8 AR_UINT8_18[18];
#define MWT_AR_UINT8_18 AR_UINT8_18

typedef struct
{
	UINT8                ITach1OpSts;
	UINT8                ITach2OpSts;
	UINT8                ITach1Flt;
	UINT8                ITach2Flt;
	UINT8                IMvbVld;
	UINT8                ITrkMode;
	UINT8                IAllWhlSlide;
	usint8_array_2       Reserved1;
	UINT8                IHscbCmd;
	UINT8                IHscbFdbck;
	UINT8                ITruckId;
	UINT8                ILockOutRstPTU;
	UINT8                IRDSelfTestReq;
	UINT8                IPropCutout;
	UINT8                RailGapDet_Health;
	usint8_array_16      Reserved2;
} DS_1024;
#define MWT_DS_1024 DS_1024

typedef UINT8 AR_UINT8_26[26];
#define MWT_AR_UINT8_26 AR_UINT8_26

typedef struct
{
	UINT32               ITachRpm1;
	UINT32               ITachRpm2;
	UINT32               IRefWheelDia;
	UINT16               Reserved1;
	usint8_array_2       Reserved2;
	UINT32               ILoadWeigh;
	INT16                ILineVoltage;
	AR_UINT8_26          Reserved3;
} DS_1025;
#define MWT_DS_1025 DS_1025

typedef struct
{
	UINT8                ITelegrVldtBit;
	UINT8                Reserved1;
	UINT16               ILifeSignInd;
	AR_UINT8_4           Reserved2;
} DS_1026;
#define MWT_DS_1026 DS_1026

typedef UINT8 AR_UINT8_30[30];
#define MWT_AR_UINT8_30 AR_UINT8_30

typedef struct
{
	UINT8                ITelegrVldBit;
	UINT8                ILifeSignInd;
	usint8_array_6       Reserved1;
} DS_1005;
#define MWT_DS_1005 DS_1005

typedef struct
{
	UINT16               ITrainSpeed;
	usint8_array_6       Reserved1;
} DS_1006;
#define MWT_DS_1006 DS_1006

typedef struct
{
	UINT32               ISystemUtcTime;
	UINT8                IDaylightTime;
	UINT8                ITimeZone;
	usint8_array_2       Reserved1;
	UINT32               ISystemTime;
	AR_UINT8_4           Reserved2;
} DS_1007;
#define MWT_DS_1007 DS_1007

typedef struct
{
	UINT8                IActiveCab;
	UINT8                ILocalActiveCab;
	UINT8                ICouplerStateX;
	UINT8                ICouplerStateY;
	UINT8                ICarOpMode;
	usint8_array_3       Reserved1;
} DS_1008;
#define MWT_DS_1008 DS_1008

typedef UINT8 AR_UINT8_20[20];
#define MWT_AR_UINT8_20 AR_UINT8_20

typedef UINT8 AR_UINT8_10[10];
#define MWT_AR_UINT8_10 AR_UINT8_10

typedef MT_CHAR8 AR_CHAR8_256[256];
#define MWT_AR_CHAR8_256 AR_CHAR8_256

typedef MT_CHAR8 AR_CHAR8_64[64];
#define MWT_AR_CHAR8_64 AR_CHAR8_64

typedef UINT32 AR_UINT32_4[4];
#define MWT_AR_UINT32_4 AR_UINT32_4

typedef MT_CHAR8 AR_CHAR8_512[512];
#define MWT_AR_CHAR8_512 AR_CHAR8_512

typedef UINT8 AR_UINT8_7[7];
#define MWT_AR_UINT8_7 AR_UINT8_7

typedef MT_CHAR8 AR_CHAR8_12[12];
#define MWT_AR_CHAR8_12 AR_CHAR8_12

typedef MT_CHAR8 AR_CHAR8_4[4];
#define MWT_AR_CHAR8_4 AR_CHAR8_4

typedef MT_CHAR8 AR_CHAR8_16[16];
#define MWT_AR_CHAR8_16 AR_CHAR8_16

typedef MT_CHAR8 AR_CHAR8_128[128];
#define MWT_AR_CHAR8_128 AR_CHAR8_128

typedef UINT8 AR_UINT8_126[126];
#define MWT_AR_UINT8_126 AR_UINT8_126

typedef MT_CHAR8 AR_CHAR8_10[10];
#define MWT_AR_CHAR8_10 AR_CHAR8_10

typedef UINT8 AR_UINT8_161[161];
#define MWT_AR_UINT8_161 AR_UINT8_161

typedef UINT8 AR_UINT8_15[15];
#define MWT_AR_UINT8_15 AR_UINT8_15

typedef struct
{
	UINT32               appVersion;
	INT32                appResultCode;
} DS_1004;
#define MWT_DS_1004 DS_1004

typedef struct
{
	UINT32               version;
	INT32                resultCode;
	UINT32               timeMsgSent;
	UINT32               timeMsgValidity;
	UINT32               dataID;
	UINT32               waysideRef;
	AR_UINT32_4          Reserved1;
} DS_1003;
#define MWT_DS_1003 DS_1003

typedef struct
{
	UINT32               appVersion;
	UINT32               transactionID;
	AR_CHAR8_64          dataLocation;
	UINT16               noOfChars;
	AR_CHAR8_512         specificParameters;
} DS_1002;
#define MWT_DS_1002 DS_1002

typedef struct
{
	UINT32               version;
	INT32                resultCode;
	UINT32               timeMsgSent;
	UINT32               timeMsgValidity;
	UINT32               dataID;
	UINT32               waysideRef;
	AR_UINT32_4          Reserved1;
} DS_1001;
#define MWT_DS_1001 DS_1001

typedef struct
{
	UINT8                IRailGapDetect;
	UINT8                ILeadingTruck;
	UINT8                ILeadingConsistNo;
	AR_UINT8_29          Reserved1;
} DS_1027;
#define MWT_DS_1027 DS_1027

typedef struct
{
	usint8_array_8       Reserved1;
} DS_1016;
#define MWT_DS_1016 DS_1016

typedef struct
{
	AR_CHAR8_16          Reserved1;
	AR_CHAR8_128         Reserved2;
	usint8_array_6       Reserved3;
	AR_UINT8_126         Reserved4;
} DS_1015;
#define MWT_DS_1015 DS_1015

typedef struct
{
	UINT8                Reserved1;
	UINT8                VConfigStatus;
	usint8_array_6       Reserved2;
} DS_1014;
#define MWT_DS_1014 DS_1014

typedef struct
{
	UINT8                Reserved1;
	AR_CHAR8_4           Reserved2;
	UINT16               Reserved3;
	UINT16               Reserved4;
	UINT8                Reserved5;
	usint8_array_6       Reserved6;
} DS_1013;
#define MWT_DS_1013 DS_1013

typedef struct
{
	AR_CHAR8_12          VSysSwId;
	UINT8                VSysSwVersion;
	UINT8                VSysSwRelease;
	UINT8                VSysSwUpdate;
	UINT8                VSysSwEvolve;
	UINT32               VSysSwDate;
	UINT32               VCrc;
	AR_UINT8_10          Reserved1;
} DS_1012;
#define MWT_DS_1012 DS_1012

typedef struct
{
	UINT8                ISlfTestOnDemQty;
	AR_UINT8_10          CSlfTestOnDemReq;
	AR_UINT8_4           Reserved1;
} DS_1011;
#define MWT_DS_1011 DS_1011

typedef struct
{
	DS_1003              MsgHeadEDSRepPrepData;
	DS_1004              MsgDataEDSRepPrepData;
} DS_847;
#define MWT_DS_847 DS_847

typedef struct
{
	UINT8                CIdSystemReq;
	AR_UINT8_7           Reserved1;
} DS_1010;
#define MWT_DS_1010 DS_1010

typedef struct
{
	DS_1001              MsgHeadEDSReqPrepData;
	DS_1002              MsgDataEDSReqPrepData;
} DS_846;
#define MWT_DS_846 DS_846

typedef struct
{
	UINT32               appVersion;
	INT32                appResultCode;
	UINT32               transactionID;
	UINT32               statusData;
} DS_844;
#define MWT_DS_844 DS_844

typedef struct
{
	UINT32               appVersion;
	INT32                appResultCode;
} DS_843;
#define MWT_DS_843 DS_843

typedef struct
{
	UINT32               appVersion;
	UINT32               TransactionID;
	AR_CHAR8_256         filename;
	AR_CHAR8_64          fileServerHostname;
	AR_CHAR8_256         fileServerPath;
} DS_842;
#define MWT_DS_842 DS_842

typedef struct
{
	UINT32               appVersion;
	INT32                appResultCode;
	UINT32               transactionID;
} DS_841;
#define MWT_DS_841 DS_841

typedef struct
{
	UINT32               appVersion;
	UINT32               transactionID;
	UINT32               dataID;
	UINT32               waysideRef;
	AR_CHAR8_256         filename;
	UINT32               filesize;
} DS_840;
#define MWT_DS_840 DS_840

typedef struct
{
	DS_1010              Identification;
} DS_8002001;
#define MWT_DS_8002001 DS_8002001

typedef struct
{
	MWT_BOOL             MultipleLcuFlt;
	usint8_array_6       CurrentLcuId;
	usint8_array_6       PreviousLcuId;
	MWT_BOOL             MultipleVatcFlt;
	usint8_array_6       CurrentVatcId;
	usint8_array_6       PreviousVatcId;
	MWT_BOOL             LcuCircCntFlt;
	UINT32               LcuCircCnt;
	MWT_BOOL             VatcCircCntFlt;
	UINT32               VatcCircCnt;
	MWT_BOOL             LcuNoSideInMsgIdFlt;
	UINT8                LcuMsgId;
	MWT_BOOL             VatcMsgIdMismatchFlt;
	UINT8                VatcMsgId;
	MWT_BOOL             LcuMissTelegramFlt;
	MWT_BOOL             VatcMissTelegramFlt;
	MWT_BOOL             LcuInvOpModeFlt;
	UINT8                LcuOpMode;
	MWT_BOOL             VatcInvOpModeFlt;
	UINT8                VatcOpMode;
	MWT_BOOL             LcuInvRebFlt;
	UINT8                LcuReb;
	MWT_BOOL             LcuVatcModeMismatchFlt;
	UINT8                LcuOpMode2;
	UINT8                VatcOpMode2;
} Tcn_Fault_Str;
#define MWT_Tcn_Fault_Str Tcn_Fault_Str

typedef struct
{
	MWT_USINT            msgId;
	usint8_array_6       LcuId;
	MWT_USINT            reserved1;
	MWT_UINT             LcuCircCnt;
	usint8_array_2       reserved2;
	MWT_USINT            operatingMode;
	MWT_USINT            JogRevStatus;
	MWT_USINT            JogRevTimer;
	MWT_USINT            CouplingSpdStatus;
	MWT_USINT            CouplingSpd;
	MWT_USINT            CarWshBut;
	MWT_USINT            CarWshSpd;
	MWT_USINT            EmerBackContact;
	usint8_array_8       reserved3;
	MWT_USINT            Direction;
	MWT_USINT            BRK3net;
	MWT_USINT            FSFBnet;
	MWT_USINT            PowerCmd;
	MWT_USINT            BrakeCmd;
	MWT_USINT            reserved4;
	MWT_UINT             MCUhandlePos;
	MWT_USINT            MCUpowerSwitch;
	MWT_USINT            MCUbrakeSwitch;
	MWT_USINT            MCUFullBrakeSwitch;
	MWT_USINT            EM;
	MWT_USINT            SpeedRestriction;
	MWT_UINT             BO_LockedAxle;
	MWT_USINT            BOBP;
	MWT_USINT            ADCL;
	MWT_USINT            ADCBP;
	MWT_USINT            REB;
	usint8_array_2       reserved5;
	usint8_array_16      reserved6;
} Tcn_LCU_RAW_Str;
#define MWT_Tcn_LCU_RAW_Str Tcn_LCU_RAW_Str

typedef struct
{
	MWT_USINT            msgId;
	usint8_array_6       VatcId;
	MWT_USINT            reserved1;
	MWT_UINT             VatcCircCnt;
	usint8_array_2       reserved2;
	MWT_USINT            operatingMode;
	usint8_array_3       reserved3;
	MWT_INT              Speed;
	MWT_INT              PrevBlckSpeed;
	MWT_INT              OperSpeed;
	usint8_array_6       reserved4;
	MWT_USINT            Direction;
	MWT_USINT            BRK3net;
	MWT_USINT            FSFBnet;
	MWT_USINT            PKOnet;
	MWT_USINT            PowerCmd;
	MWT_USINT            BrakeCmd;
	MWT_USINT            TestMode;
	MWT_USINT            ADCL;
	MWT_USINT            ADCBP;
	usint8_array_11      reserved5;
	usint8_array_16      reserved6;
} Tcn_VATC_RAW_Str;
#define MWT_Tcn_VATC_RAW_Str Tcn_VATC_RAW_Str

typedef struct
{
	UINT8                reserved1;
	usint8_array_2       reserved2;
	UINT8                DynBrakeStatus;
	usint8_array_3       reserved3;
	usint8_array_8       reserved4;
} Tcn_PCU_Str;
#define MWT_Tcn_PCU_Str Tcn_PCU_Str

typedef struct
{
	MWT_REAL             reference_wheel;
	MWT_DINT             odometer;
	wheel_dia_Array      wheelDiameter;
	MWT_REAL             LinePower;
	MWT_REAL             LineEnergyConsTot;
	MWT_REAL             LineEnergyCons;
	MWT_REAL             LineEnergyRegenTot;
	MWT_REAL             LineEnergyRegen;
	MWT_REAL             BrakeEnergyConsTot;
	MWT_REAL             BrakeEnergyCons;
	MWT_DINT             TimeInverterCharged_s;
	MWT_DINT             TimeBRK3DeEnergized_s;
	MWT_DINT             TimeInMotion_s;
	MWT_DINT             TruckStatsOdometer;
	MWT_REAL             AverageSpeed;
	MWT_DINT             TimeInPower_s;
	MWT_DINT             TimeInRegenBraking_s;
	MWT_DINT             TimeInNonRegenBraking_s;
	MWT_DINT             RD_FaultCount;
	MWT_DINT             HSCB_SelfTripCount;
	MWT_UDINT            TruckStatsSessionStartTime;
	MWT_DINT             TotaLoadWeight;
	MWT_DINT             NumberOfLoadWeightSamples;
	MWT_DINT             TruckStatsSessionTime_s;
} NC_bram_Str;
#define MWT_NC_bram_Str NC_bram_Str

typedef struct
{
	UINT8                ITelegrVldtBit;
	UINT8                ILifeSignInd;
	usint8_array_6       Reserved1;
} DS_1017;
#define MWT_DS_1017 DS_1017

typedef struct
{
	UINT8                ISlfTestOnDemNb;
	UINT8                Reserved1;
	AR_UINT8_10          ISlfTstOnDemFb;
	AR_UINT8_10          ISlfTestOnDemRdy;
	usint8_array_2       Reserved2;
} DS_1018;
#define MWT_DS_1018 DS_1018

typedef struct
{
	UINT8                Reserved1;
	UINT8                FPcuLockedOut;
	UINT8                FOverSpdLmtRM;
	UINT8                FOverSpdLmtYM;
	UINT8                FLineVMeasErr;
	UINT8                FLinkVMeasErr;
	UINT8                FPcuHeatSinkHs;
	UINT8                FPcuInvHotAmb;
	UINT8                FTractMotorHot;
	UINT8                FBrkResHot;
	UINT8                FGndHighVolt;
	UINT8                FIGBTGateDrvFb;
	UINT8                FInvPhaseOvrCurr;
	UINT8                FInvPhaseCurrImb;
	UINT8                FFltrCapCharg;
	UINT8                FFltrCapDisCharg;
	UINT8                FFltrCapOverVolt;
	UINT8                FBrkChopper;
	UINT8                FFwdRevCom;
	UINT8                FAccelDecelCom;
	UINT8                FChrgContClose;
	UINT8                FChrgContOpen;
	UINT8                FSeparContClose;
	UINT8                FSeparContOpen;
	UINT8                FRollbackDetect;
	UINT8                FChrgResistHot;
	UINT8                FLinkOvrCurr;
	UINT8                FInvTempMeas;
	UINT8                FPhaseCurrMeas;
	UINT8                FLinkCurrMeas;
	UINT8                FPwrBalance;
	UINT8                FDriveCtrlUnit;
	UINT8                FStallDet;
	UINT8                FFilterCapa;
	UINT8                Reserved2;
	UINT8                Reserved3;
	UINT8                FLineCurrMeas;
	UINT8                FDiffCurrMeas;
	UINT8                FBrkResCurrMeas;
	UINT8                FHSCBContClose;
	UINT8                FHSCBContOpen;
	UINT8                FDirectionMism;
	UINT8                FInvalDirectiReq;
	UINT8                FAmbTempMeas;
	UINT8                FSpeedSensor1;
	UINT8                FSpeedSensor2;
	UINT8                Reserved4;
	UINT8                FHscbTripped;
	UINT8                FCtrlSysMvb;
	UINT8                FCommFbcuMvb;
	UINT8                FMioDxMod;
	UINT8                FAxMod;
	UINT8                FSlideTimeout;
	UINT8                FLoadWeightInval;
	UINT8                FBrkResOverCurr;
	UINT8                Reserved5;
	UINT8                FRunRelayFailed;
	UINT8                FZsrRelay;
	UINT8                FWheelDiameter;
	UINT8                FWheelDiamMism;
	UINT8                FCplgPushBtnEvt;
	UINT8                FDBKnockoutReq;
	UINT8                FEnterSelfTestFl;
	UINT8                FIllegalDirChg;
	UINT8                FRailGapDetected;
	UINT8                FRegenCutout;
	UINT8                FRDTimerMonFlt;
	UINT8                FRDTrSafeFlt;
	UINT8                FZeroSpeedFlt;
	UINT8                FBrkPwrSwMismFlt;
	UINT8                FBrkSwMCUMismFlt;
	UINT8                FLCU_CirCntFlt;
	UINT8                FLCU_MisTeleFlt;
	UINT8                FLCU_MSGIDFlt;
	UINT8                FLCU_PwrBrkCmdMs;
	UINT8                FVATC_CirCntFlt;
	UINT8                FVATC_MisTeleFlt;
	UINT8                FVATC_MSGIDFlt;
	UINT8                FWheelDiamMismSd;
	UINT8                FTCNNoActNet;
	UINT8                FLCUVATCModeFlt;
	UINT8                FCollectShoeFlt;
	UINT8                FDBRakeFlt;
	UINT8                FRGESSFlt;
	UINT8                FLkLnCurrentFlt;
	UINT8                FAutoRmoModeFlt;
	UINT8                FOverSpdLmtMax;
	UINT8                FOpeModeNotAvail;
	UINT8                FMultLCUDetR;
	UINT8                FMultVATCDetR;
	UINT8                FThirdRailCarLoss;
	UINT8                FCommPcuMvb;
	UINT8                FMultLCUDetL;
	UINT8                FMultVATCDetL;
	UINT8                FRunDetTractSafe;
	UINT8                FNoTCNnetActive;
	UINT8                FPKOTlMism;
	AR_UINT8_15          Reserved6;
} DS_1022;
#define MWT_DS_1022 DS_1022

typedef struct
{
	DS_1023              GenInfo802;
	DS_1024              Discrete802;
	DS_1025              Analog802;
} DS_8808001;
#define MWT_DS_8808001 DS_8808001

typedef struct
{
	DS_1026              GenInfo803;
	DS_1027              Discrete803;
} DS_8808002;
#define MWT_DS_8808002 DS_8808002

typedef struct
{
	UINT8                IMaintAllowed;
	UINT8                IPDepTestAllowed;
	AR_UINT8_10          Reserved1;
} DS_1009;
#define MWT_DS_1009 DS_1009

typedef DS_1012 AR_DS_1012_20[20];
#define MWT_AR_DS_1012_20 AR_DS_1012_20

typedef struct
{
	AR_CHAR8_10          Reserved1;
	UINT8                VSysSwNumber;
	AR_DS_1012_20        SoftwareVersion;
	AR_UINT8_161         Reserved2;
	DS_1013              Reserved3;
	DS_1014              EndDeviceMode;
	DS_1015              Reserved4;
	DS_1016              Reserved5;
	AR_UINT8_10          Reserved6;
} DS_8004001;
#define MWT_DS_8004001 DS_8004001

typedef struct
{
	DS_1011              TestOnDemand;
} DS_8003001;
#define MWT_DS_8003001 DS_8003001

typedef struct
{
	DS_1017              GenInfo801;
	DS_1018              TestOnDemand801;
	DS_1019              Discrete801;
	DS_1020              Analog801;
	DS_1021              Counter801;
	DS_1022              Events801;
} DS_8805001;
#define MWT_DS_8805001 DS_8805001

typedef struct
{
	DS_1005              LifeSignTCMS;
	DS_1006              Speed;
	DS_1007              TimeInfo;
	DS_1008              TrainConfig;
	DS_1009              TrainGenInfo;
	AR_UINT8_20          Reserved1;
} DS_8001101;
#define MWT_DS_8001101 DS_8001101



#ifdef __cplusplus
}
#endif

#endif   /* _USERTYPES_H */


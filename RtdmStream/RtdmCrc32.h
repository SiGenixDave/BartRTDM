/******************************************************************************
 * This file is derived from crc32.c from the zlib-1.1.3 distribution
 * by Jean-loup Gailly and Mark Adler.
 * crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 ******************************************************************************
 * HISTORY   :
 *   $Log: crc32.h,v $
 *   Revision 1.4  2007/07/11 12:04:13  mbrotz
 *   Synchronized with actual status of the corresponding DVS source module
 *
 *   Revision 1.3  2006/10/18 14:10:19  pkuenzle
 *   *** empty log message ***
 *
 *  
 *  2     24.08.06 15:24 Pkuenzle
 *  #ifdef __cplusplus   added
 *  
 *  1     06-05-18 17:29 Clundhol
 *  Initial version
 *****************************************************************************/



#ifndef CRC32_H
#define CRC32_H

#ifdef __cplusplus   /* to be compatible with C++ */
extern "C" {
#endif


UINT32 crc32(UINT32 crc, const UINT8 *buf, INT32 len);


#ifdef __cplusplus   /* to be compatible with C++ */
}
#endif


#endif  /* CRC32_H */

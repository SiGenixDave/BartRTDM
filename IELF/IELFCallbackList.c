/*
 * IELFCallbackList.c
 *
 *  Created on: Sep 15, 2016
 *      Author: Dave
 */

/* This file exists because this feature must support the ability to detect the end of an
 * event in case of a power cycle or reset while an event is active.
 */

#if TODO
typedef struct
{
   UINT16 eventCode;
   EventOverCallback callback;

} IELFCallbackMap;

static IELFCallbackMap ielfCallbackMap[] =
{
 {0, x},

};

#endif

/*
 * MySleep.c
 *
 *  Created on: Jun 30, 2016
 *      Author: Dave
 */

#include <windows.h>
#include <stdio.h>

void MySleep (int time)
{
    Sleep (time);
}

extern void FTPDataLog ();
static void UserInterfaceMain ()
{
    int ch;

    printf ("UI Thread\n");
    ch = getchar ();
    if (ch == '0')
    {
        FTPDataLog ();
    }
    fflush (stdin);
}

static DWORD WINAPI myUIThread (void* threadParams)
{
    while (1)
    {
        UserInterfaceMain ();
    }

    return 0;
}

void CreateUIThread (void)
{
    DWORD threadDescriptor;

    CreateThread (
    NULL, /* default security attributes.   */
    0, /* use default stack size.        */
    myUIThread, /* thread function name.          */
    (void*) NULL, /* argument to thread function.   */
    0, /* use default creation flags.    */
    &threadDescriptor); /* returns the thread identifier. */
}

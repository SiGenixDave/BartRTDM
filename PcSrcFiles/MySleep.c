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
    switch (ch)
    {
        case '0':
            FTPDataLog ();
            break;
        case '1':
        case '2':
        case '3':
            LogIELFEvent(ch - '0');
            break;

        case '4':
            ForceSim1EventOver();
            break;

        case '5':
            ForceSim2EventOver();
            break;

        default:
            break;
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

//======================================================================
// GenDriver - Generic stream device driver for Windows CE
//
//======================================================================
#include <windows.h>                 // For all that Windows stuff
#include "MBoxDrv.h"                 // Local includes

//
// Globals
//
HINSTANCE hInst;                     // DLL instance handle

//
// Debug zone support
//
#ifdef DEBUG
// Used as a prefix string for all debug zone messages.
#define DTAG        TEXT ("MBoxDrv: ")

// Debug zone constants
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_FUNC       DEBUGZONE(2)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_DRVCALLS   DEBUGZONE(4)
#define ZONE_EXENTRY  (ZONE_FUNC | ZONE_DRVCALLS)
// Debug zone structure
DBGPARAM dpCurSettings = {
    TEXT("MBoxDrv"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"), TEXT("Init"),
	TEXT("Driver Calls"),TEXT("Undefined"), TEXT("Undefined"),TEXT("Undefined"),
	TEXT("Undefined"), TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"), TEXT("Undefined") },
    0x0003
};
#endif //DEBUG

//======================================================================
// DllMain - DLL initialization entry point
//
BOOL WINAPI DllMain (HANDLE hinstDLL, DWORD dwReason, 
                     LPVOID lpvReserved) {
    hInst = (HINSTANCE)hinstDLL;

    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DEBUGREGISTER(hInst);
            // Improve performance by passing on thread attach calls
            DisableThreadLibraryCalls (hInst);
        break;
    
        case DLL_PROCESS_DETACH:
            DEBUGMSG(ZONE_INIT, (DTAG TEXT("DLL_PROCESS_DETACH\r\n")));
            break;
    }
    return TRUE;
}
//======================================================================
// MBX_Init - Driver initialization function
//
DWORD MBX_Init (DWORD dwContext, LPCVOID lpvBusContext) 
{
    PDRVCONTEXT pDrv;

    DEBUGMSG (ZONE_INIT | ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Init++ dwContex:%x\r\n"), dwContext));

    // Allocate a device instance structure.
    pDrv = (PDRVCONTEXT)LocalAlloc (LPTR, sizeof (DRVCONTEXT));
    if (pDrv) {
       // Initialize structure.
       memset ((PBYTE) pDrv, 0, sizeof (DRVCONTEXT));
       pDrv->dwSize = sizeof (DRVCONTEXT);

       // Read registry to get config data
       GetConfigData (dwContext);
    } else 
        DEBUGMSG (ZONE_INIT | ZONE_ERROR, 
                  (DTAG TEXT("MBX_Init failure. Out of memory\r\n")));

	DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Init-- pDrv: %x\r\n"), pDrv));
    return (DWORD)pDrv;
}
//======================================================================
// MBX_PreDeinit - Driver de-initialization notification function
//
BOOL MBX_PreDeinit (DWORD dwContext) {

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_PreDeinit++ dwContex:%x\r\n"), dwContext));

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_PreDeinit--\r\n")));
    return TRUE;
}
//======================================================================
// MBX_Deinit - Driver de-initialization function
//
BOOL MBX_Deinit (DWORD dwContext) {
    PDRVCONTEXT pDrv = (PDRVCONTEXT) dwContext;

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Deinit++ dwContex:%x\r\n"), dwContext));

    if (pDrv && (pDrv->dwSize == sizeof (DRVCONTEXT))) {

        // Free the driver state buffer.
        LocalFree ((PBYTE)pDrv);
    }
    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Deinit--\r\n")));
    return TRUE;
}
//======================================================================
// MBX_Open - Called when driver opened
//
DWORD MBX_Open (DWORD dwContext, DWORD dwAccess, DWORD dwShare) 
{
    PDRVCONTEXT pDrv = (PDRVCONTEXT) dwContext;
    POPENCONTEXT pOpen;

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Open++ dwContext: %x\r\n"), dwContext));

    // Verify that the context handle is valid.
    if (pDrv && (pDrv->dwSize != sizeof (DRVCONTEXT))) 
	{
        DEBUGMSG (ZONE_ERROR, (DTAG TEXT("MBX_Open failed\r\n")));
        return 0;
    }

    // Allocate an open context structure.
    pOpen = (POPENCONTEXT)LocalAlloc (LPTR, sizeof (OPENCONTEXT));
    if (pOpen) 
	{
       // Initialize structure.
       memset ((PBYTE) pOpen, 0, sizeof (OPENCONTEXT));
       pOpen->dwSize = sizeof (OPENCONTEXT);

	   // Save ptr to drive context
	   pOpen->pDrv = pDrv;

    }
	else 
        DEBUGMSG (ZONE_INIT | ZONE_ERROR, 
                  (DTAG TEXT("MBX_Open failure. Out of memory\r\n")));


    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Open--\r\n")));
    return (DWORD)pOpen;
}
//======================================================================
// MBX_PreClose - Called when the driver is about to be closed
//
BOOL MBX_PreClose (DWORD dwOpen) 
{

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_PreClose++ dwOpen: %x\r\n"), dwOpen));

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_PreClose--\r\n")));
    return TRUE;
}
//======================================================================
// MBX_Close - Called when driver closed
//
BOOL MBX_Close (DWORD dwOpen) 
{
    POPENCONTEXT pOpen = (POPENCONTEXT) dwOpen;
    PDRVCONTEXT pDrv;

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Close++ dwOpen: %x\r\n"), dwOpen));

    if (pOpen && (pOpen->dwSize != sizeof (POPENCONTEXT))) 
	{
        DEBUGMSG (ZONE_FUNC | ZONE_ERROR, 
                  (DTAG TEXT("MBX_Close failed\r\n")));
        return 0;
    }
	pDrv = pOpen->pDrv;

	// Free Open context
	LocalFree (pOpen);

    
    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Close--\r\n")));
    return TRUE;
}
//======================================================================
// MBX_Read - Called when driver read
//
DWORD MBX_Read (DWORD dwOpen, LPVOID pBuffer, DWORD dwCount) {
    DWORD dwBytesRead = 0;
    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Read++ dwOpen: %x\r\n"), dwOpen));

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Read--\r\n")));
    return dwBytesRead;
}
//======================================================================
// MBX_Write - Called when driver written
//
DWORD MBX_Write (DWORD dwOpen, LPVOID pBuffer, DWORD dwCount) {
    DWORD dwBytesWritten = 0;
    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_Write++ dwOpen: %x\r\n"), dwOpen));

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_Write--\r\n")));
    return dwBytesWritten;
}
//======================================================================
// MBX_Seek - Called when SetFilePtr called
//
DWORD MBX_Seek (DWORD dwOpen, long lDelta, WORD wType) {
    DEBUGMSG (ZONE_EXENTRY,(DTAG TEXT("MBX_Seek++ dwOpen:%x %d %d\r\n"), 
              dwOpen, lDelta, wType));

    DEBUGMSG (ZONE_EXENTRY, (DTAG TEXT("MBX_Seek--\r\n")));
    return 0;
}
//======================================================================
// MBX_IOControl - Called when DeviceIOControl called
//
DWORD MBX_IOControl (DWORD dwOpen, DWORD dwCode, PBYTE pIn, DWORD dwIn,
                     PBYTE pOut, DWORD dwOut, DWORD *pdwBytesWritten) {
    pOpenCONTEXT pState;
    DWORD err = ERROR_INVALID_PARAMETER;

    DEBUGMSG (ZONE_EXENTRY, 
              (DTAG TEXT("MBX_IOControl++ dwOpen: %x  dwCode: %x\r\n"),
              dwOpen, dwCode));

    pState = (pOpenCONTEXT) dwOpen;
    switch (dwCode) {
        // Insert IOCTL codes here.

		// This case put in to avoid compiler error.
		case 0:
			break;

        default:
            DEBUGMSG (ZONE_ERROR, 
             (DTAG TEXT("MBX_IOControl: unknown code %x\r\n"), dwCode));
            return FALSE;
    }
    SetLastError (err);
    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("MBX_IOControl--\r\n")));
    return TRUE;
}
//======================================================================
// MBX_PowerDown - Called when system suspends
//
void MBX_PowerDown (DWORD dwContext) 
{
    return;
}
//======================================================================
// MBX_PowerUp - Called when system resumes
//
void MBX_PowerUp (DWORD dwContext) 
{
    return;
}
//----------------------------------------------------------------------
// GetConfigData - Get the configuration data from the registry.
//
DWORD GetConfigData (DWORD dwContext) 
{
    int nLen, rc;
    DWORD dwLen, dwType, dwSize = 0;
    HKEY hKey;
    TCHAR szKeyName[256], szPrefix[8];

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("GetConfigData++\r\n")));
    nLen = 0;
    // If ptr < 65K, it’s a value, not a pointer.  
    if (dwContext < 0x10000) {
        return -1; 
    } else {
        __try {
            nLen = lstrlen ((LPTSTR)dwContext);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            nLen = 0;
        }
    }
    if (!nLen) {
        DEBUGMSG (ZONE_ERROR, (DTAG TEXT("dwContext not a ptr\r\n")));
        return -2;
    }
    // Open the Active key for the driver.
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,(LPTSTR)dwContext,0, 0, &hKey);

    if (rc == ERROR_SUCCESS) 
	{
        // Read the key value.
        dwLen = sizeof(szKeyName);
        rc = RegQueryValueEx (hKey, TEXT("Key"), NULL, &dwType,
                                   (PBYTE)szKeyName, &dwLen);

        RegCloseKey(hKey);
        if (rc == ERROR_SUCCESS)
            rc = RegOpenKeyEx (HKEY_LOCAL_MACHINE, (LPTSTR) 
                                   dwContext, 0, 0, &hKey);
        if (rc == ERROR_SUCCESS) 
		{
            // This driver doesn’t need any data from the key, so as
            // an example, it just reads the Prefix value, which 
            // identifies the three-char prefix (GEN) of this driver.
            dwLen = sizeof (szPrefix);
            rc = RegQueryValueEx (hKey, TEXT("Prefix"), NULL,
                                  &dwType, (PBYTE)szPrefix, &dwLen);
            RegCloseKey(hKey);
        } else 
            DEBUGMSG (ZONE_ERROR, (TEXT("Error opening key\r\n")));
    } else
        DEBUGMSG (ZONE_ERROR, (TEXT("Error opening Active key\r\n")));

    DEBUGMSG (ZONE_FUNC, (DTAG TEXT("GetConfigData--\r\n")));
    return 0;
}

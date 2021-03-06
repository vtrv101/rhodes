#include "stdafx.h"

#include "common/RhoPort.h"
#include "common/StringConverter.h"
#include "ruby/ext/rho/rhoruby.h"
#include "MainWindow.h"

#ifdef OS_WINCE__
#include <tapi.h>
#include <tsp.h>
//#include <sms.h>
#endif

using namespace rho;
using namespace rho::common;

extern "C"
{
#ifdef OS_WINCE__

static const int PHONE_NUMBER_BUFFER_SIZE = 512;

bool getPhoneNumFromSIMCard (String &number) 
{

#define EXIT_ON_NULL(_p) if (_p == NULL){ hr = E_OUTOFMEMORY; goto FuncExit; }
#define EXIT_ON_FALSE(_f) if (!(_f)) { hr = E_FAIL; goto FuncExit; }
#define MAX(i, j)   ((i) > (j) ? (i) : (j))

	const int TAPI_API_LOW_VERSION  = 0x00020000;
	const int TAPI_API_HIGH_VERSION = 0x00020000;
	const int LINE_NUMBER = 1;

    HRESULT  hr = E_FAIL;
    LRESULT  lResult = 0;
    HLINEAPP hLineApp;
    DWORD    dwNumDevs;
    DWORD    dwAPIVersion = TAPI_API_HIGH_VERSION;
    LINEINITIALIZEEXPARAMS liep;

    DWORD dwTAPILineDeviceID;
    const DWORD dwAddressID = LINE_NUMBER - 1;

    liep.dwTotalSize = sizeof(liep);
    liep.dwOptions   = LINEINITIALIZEEXOPTION_USEEVENT;

    if (SUCCEEDED(lineInitializeEx(&hLineApp, 0, 0, TEXT("ExTapi_Lib"), &dwNumDevs, &dwAPIVersion, &liep))) {
        BYTE* pCapBuf = NULL;
        DWORD dwCapBufSize = PHONE_NUMBER_BUFFER_SIZE;
        LINEEXTENSIONID  LineExtensionID;
        LINEDEVCAPS*     pLineDevCaps = NULL;
        LINEADDRESSCAPS* placAddressCaps = NULL;

        pCapBuf = new BYTE[dwCapBufSize];
        EXIT_ON_NULL(pCapBuf);

        pLineDevCaps = (LINEDEVCAPS*)pCapBuf;
        pLineDevCaps->dwTotalSize = dwCapBufSize;

        // Get TSP Line Device ID
        dwTAPILineDeviceID = 0xffffffff;
        for (DWORD dwCurrentDevID = 0 ; dwCurrentDevID < dwNumDevs ; dwCurrentDevID++) {
            if (0 == lineNegotiateAPIVersion(hLineApp, dwCurrentDevID, TAPI_API_LOW_VERSION, TAPI_API_HIGH_VERSION,
                &dwAPIVersion, &LineExtensionID)) {
                lResult = lineGetDevCaps(hLineApp, dwCurrentDevID, dwAPIVersion, 0, pLineDevCaps);

                if (dwCapBufSize < pLineDevCaps->dwNeededSize) {
                    delete[] pCapBuf;
                    dwCapBufSize = pLineDevCaps->dwNeededSize;
                    pCapBuf = new BYTE[dwCapBufSize];
                    EXIT_ON_NULL(pCapBuf);

                    pLineDevCaps = (LINEDEVCAPS*)pCapBuf;
                    pLineDevCaps->dwTotalSize = dwCapBufSize;

                    lResult = lineGetDevCaps(hLineApp, dwCurrentDevID, dwAPIVersion, 0, pLineDevCaps);
                }

                if ((0 == lResult) &&
                    (0 == _tcscmp((TCHAR*)((BYTE*)pLineDevCaps+pLineDevCaps->dwLineNameOffset), CELLTSP_LINENAME_STRING))) {
                    dwTAPILineDeviceID = dwCurrentDevID;
                    break;
                }
            }
        }

        placAddressCaps = (LINEADDRESSCAPS*)pCapBuf;
        placAddressCaps->dwTotalSize = dwCapBufSize;

        lResult = lineGetAddressCaps(hLineApp, dwTAPILineDeviceID, dwAddressID, dwAPIVersion, 0, placAddressCaps);

        if (dwCapBufSize < placAddressCaps->dwNeededSize) {
            delete[] pCapBuf;
            dwCapBufSize = placAddressCaps->dwNeededSize;
            pCapBuf = new BYTE[dwCapBufSize];
            EXIT_ON_NULL(pCapBuf);

            placAddressCaps = (LINEADDRESSCAPS*)pCapBuf;
            placAddressCaps->dwTotalSize = dwCapBufSize;

            lResult = lineGetAddressCaps(hLineApp, dwTAPILineDeviceID, dwAddressID, dwAPIVersion, 0, placAddressCaps);
        }

        if (0 == lResult) {
			EXIT_ON_FALSE(0 != placAddressCaps->dwAddressSize);

			// A non-zero dwAddressSize means a phone number was found
			ASSERT(0 != placAddressCaps->dwAddressOffset);    
			PWCHAR tsAddress = (WCHAR*)(((BYTE*)placAddressCaps)+placAddressCaps->dwAddressOffset);
			number = convertToStringA (tsAddress);

            hr = S_OK;
        }

        delete[] pCapBuf;
    } // End if ()

FuncExit:
    lineShutdown(hLineApp);
	
	if (hr != S_OK) {
		LOG(ERROR) + "failed to get phone number from SIM";
		return false;
	}

    return true;

#undef EXIT_ON_NULL
#undef EXIT_ON_FALSE 
#undef MAX

}
/*
bool getPhoneNumFromSMSBearer (String &number)
{
	SMS_ADDRESS psmsaAddress;
	
	if (SmsGetPhoneNumber (&psmsaAddress) != S_OK) {
		LOG(ERROR) + "failed to get phone number using SMS bearer";
		return false;
	}

	number = convertToStringA(psmsaAddress.ptsAddress);
	return true;
} */

bool getPhoneNumFromOwnerInfo (String &number)
{
	HKEY	hKey;
	DWORD	dwType, dwCount = PHONE_NUMBER_BUFFER_SIZE;
	TCHAR   strValue [PHONE_NUMBER_BUFFER_SIZE];
	LONG    res;
	TCHAR   errMsg[1024];

	if ((res = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("ControlPanel\\Owner"),  NULL, KEY_EXECUTE , &hKey)) == 0) 
	{
		if ((res = RegQueryValueEx (hKey, TEXT("Telephone"), NULL,  &dwType, (LPBYTE )strValue, &dwCount)) == 0) 
		{
			if (dwType != REG_SZ) 
			{
				LOG(ERROR) + "Settings/Owner Information/Telephone has invalid type";
				RegCloseKey(hKey);
				return false;
			}

			if (dwCount > 0) 
			{
				strValue[dwCount + 1] = '\0';

				if (_tcslen((strValue))  == 0) 
				{
					LOG(INFO) + "Settings/Owner Information/Telephone is empty";

					RegCloseKey(hKey);
					return false;
				}

				number = convertToStringA(strValue);

				RegCloseKey(hKey);
				return true;
			}
		}
	}

	RegCloseKey(hKey);
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, errMsg, sizeof(errMsg), NULL);
	LOG(ERROR) + errMsg;

	return false;
}

VALUE phone_number()
{
	String number;

	if (getPhoneNumFromSIMCard(number))
		return rho_ruby_create_string(number.c_str());
	
//	if (getPhoneNumFromSMSBearer(number))
//		return rho_ruby_create_string(number.c_str());

	if (getPhoneNumFromOwnerInfo(number))
		return rho_ruby_create_string(number.c_str());

	return rho_ruby_get_NIL();
}
#else
VALUE phone_number()
{
	return rho_ruby_get_NIL();
}
#endif

static int has_camera()
{
#ifdef OS_WINCE
/*    DEVMGR_DEVICE_INFORMATION devInfo = {0};
    GUID guidCamera = { 0xCB998A05, 0x122C, 0x4166, 0x84, 0x6A, 0x93,
                        0x3E, 0x4D, 0x7E, 0x3C, 0x86 };
    devInfo.dwSize = sizeof(devInfo);

    HANDLE hDevice = FindFirstDevice( DeviceSearchByGuid, &guidCamera, &devInfo);
    if ( hDevice != INVALID_HANDLE_VALUE )
    {
        FindClose(hDevice);
        return 1;
    }

    return 0;*/

    return 1;
#else
    return 0;
#endif
}

VALUE rho_sysimpl_get_property(char* szPropName)
{
	if (strcasecmp("has_camera",szPropName) == 0) 
        return rho_ruby_create_boolean(has_camera());

	if (strcasecmp("phone_number",szPropName) == 0)
		return phone_number();

    return 0;
}

VALUE rho_sys_get_locale()
{
    wchar_t szLang[20];
    int nRes = GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SABBREVLANGNAME , szLang, 20);
    szLang[2] = 0;
    wcslwr(szLang);

    return rho_ruby_create_string(convertToStringA(szLang).c_str());
}

int rho_sys_get_screen_width()
{
#ifdef OS_WINCE
	return GetSystemMetrics(SM_CXSCREEN);
#else
	return CMainWindow::getScreenWidth();
#endif
}

int rho_sys_get_screen_height()
{
#ifdef OS_WINCE
	return GetSystemMetrics(SM_CYSCREEN);
#else
	return CMainWindow::getScreenHeight();
#endif
}

VALUE rho_sys_makephonecall(const char* callname, int nparams, char** param_names, char** param_values) 
{
	return rho_ruby_get_NIL();
}

static int g_rho_has_network = 1;

void rho_sysimpl_sethas_network(int nValue)
{
    g_rho_has_network = nValue;
}

VALUE rho_sys_has_network()
{
	return rho_ruby_create_boolean(g_rho_has_network!=0);
}

}
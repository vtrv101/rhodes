#pragma once

#include "common/RhoStd.h"

namespace rho {
namespace net {

class URI
{
    String m_strUrl;

public:
    URI(const String& strUrl);

    static void urlEncode(const String& fullPath, String& strRes);
    static String urlEncode(const String& fullPath);
    String getPath();
    String getQueryString();
    String getScheme();
    String getPathSpecificPart();
/*
	struct CParsedCookie 
    {
		String strAuth;
		String strSession;
	};*/

    static void parseCookie(const char* szCookie, String& cookie);
	
	static boolean isLocalHost(const String& strUrl);
};

}
}

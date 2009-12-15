#pragma once

#include "ISyncProtocol.h"
#include "common/StringConverter.h"

namespace rho {
namespace sync {

struct CSyncProtocol_1 : public ISyncProtocol
{
    String m_strContentType;

    CSyncProtocol_1()
    {
        m_strContentType = "application/x-www-form-urlencoded";
    }

    const String& getContentType(){ return m_strContentType; }
    virtual int getVersion(){ return 1; }

    String getLoginUrl()
    {
        return RHOCONF().getPath("syncserver") + "client_login";
    }

    String getLoginBody( const String& name, const String& password)
    {
        return "login=" + name + "&password=" + password + "&remember_me=1";
    }

    String getClientCreateUrl()
    {
        return RHOCONF().getPath("syncserver") + "clientcreate?format=json";
    }

    String getClientRegisterUrl()
    {
        return RHOCONF().getPath("syncserver") + "clientregister";
    }

    String getClientRegisterBody( const String& strClientID, const String& strPin, int nPort, const String& strType )
    {
	    return "client_id=" + strClientID +
		    "&device_pin=" + strPin + 
            "&device_port=" + common::convertToStringA(nPort) +
		    "&device_type=" + strType;
    }

    String getClientResetUrl(const String& strClientID)
    {
        return RHOCONF().getPath("syncserver") + "clientreset?client_id=" + strClientID;
    }

    String getClientChangesUrl(const String& strSrcName, const String& strUpdateType, const String& strClientID)
    {
        return RHOCONF().getPath("syncserver") + strSrcName + "/" + strUpdateType + "objects?format=json&client_id=" + strClientID;
    }

    String getServerQueryUrl(const String& strSrcName, const String& strAction, const String& strAskParams)
    {
        String strUrl = RHOCONF().getPath("syncserver") + strSrcName;
        if ( strAction.length() > 0 )
            strUrl += '/' + strAction;
        else if ( strAskParams.length() > 0 )
            strUrl += "/ask";

        return strUrl;
    }

};

}
}

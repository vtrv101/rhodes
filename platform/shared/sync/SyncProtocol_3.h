#pragma once

#include "ISyncProtocol.h"
#include "common/StringConverter.h"

namespace rho {
namespace sync {

struct CSyncProtocol_3 : public ISyncProtocol
{
    String m_strContentType;

    CSyncProtocol_3()
    {
        m_strContentType = "application/json";
    }

    const String& getContentType(){ return m_strContentType; }
    virtual int getVersion(){ return 3; }

    String getLoginUrl()
    {
        return RHOCONF().getPath("syncserver") + "clientlogin";
    }

    String getLoginBody( const String& name, const String& password)
    {
        return "{\"login\":\"" + name + "\",\"password\":\"" + password + "\",\"remember_me\":1}";
    }

    String getClientCreateUrl()
    {
        return RHOCONF().getPath("syncserver") + "clientcreate";
    }

    String getClientRegisterUrl()
    {
        return RHOCONF().getPath("syncserver") + "clientregister";
    }

    String getClientRegisterBody( const String& strClientID, const String& strPin, int nPort, const String& strType )
    {
        return "{\"client_id\":\"" + strClientID +
            "\",\"device_pin\":\"" + strPin + 
            "\",\"device_port\":\"" + common::convertToStringA(nPort) +
            "\",\"device_type\":\"" + strType + "\"}";
    }

    String getClientResetUrl(const String& strClientID)
    {
        return RHOCONF().getPath("syncserver") + "clientreset?client_id=" + strClientID;
    }

    String getClientChangesUrl(const String& /*strSrcName*/, const String& /*strUpdateType*/, const String& /*strClientID*/)
    {
        return RHOCONF().getPath("syncserver");
    }

    String getServerQueryUrl(const String& strSrcName, const String& strAction, const String& strAskParams)
    {
        String strUrl = RHOCONF().getPath("syncserver");
        if ( strAction.length() > 0 )
            strUrl += strAction;
        else if ( strAskParams.length() > 0 )
            strUrl += "ask";
        else
            strUrl = strUrl.substr(0,strUrl.length()-1);

        return strUrl;
    }

};

}
}

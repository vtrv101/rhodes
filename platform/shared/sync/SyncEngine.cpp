#include "SyncEngine.h"
#include "SyncSource.h"
#include "SyncThread.h"

#include "json/JSONIterator.h"
#include "common/RhoConf.h"
#include "common/StringConverter.h"
#include "sync/ClientRegister.h"
#include "net/URI.h"
#include "statistic/RhoProfiler.h"
#include "ruby/ext/rho/rhoruby.h"
#include "common/RhoTime.h"
#include "SyncProtocol_3.h"

namespace rho {
const _CRhoRuby& RhoRuby = _CRhoRuby();

/*static*/ String _CRhoRuby::getMessageText(const char* szName)
{
    return rho_ruby_getMessageText(szName);
}

/*static*/ String _CRhoRuby::getErrorText(int nError)
{
    return rho_ruby_getErrorText(nError);
}

namespace sync {
IMPLEMENT_LOGCLASS(CSyncEngine,"Sync");

using namespace rho::net;
using namespace rho::common;
using namespace rho::json;

CSyncEngine::CSyncEngine(db::CDBAdapter& dbUser, db::CDBAdapter& dbApp): m_dbUserAdapter(dbUser), m_dbAppAdapter(dbApp), m_NetRequest(0), m_syncState(esNone), m_oSyncNotify(*this)
{
    m_bStopByUser = false;
    m_nSyncPageSize = 2000;

    initProtocol();
}

void CSyncEngine::initProtocol()
{
    m_SyncProtocol = new CSyncProtocol_3();
}

void CSyncEngine::prepareSync(ESyncState eState)
{
    setState(eState);
    m_bStopByUser = false;
    loadAllSources();

    m_strSession = loadSession();
    if ( isSessionExist()  )
    {
        m_clientID = loadClientID();
        getNotify().cleanLastSyncObjectCount();
    }
    else
    {
        if ( m_sources.size() > 0 )
        {
            CSyncSource& src = *m_sources.elementAt(getStartSource());
    	    //src.m_strError = "Client is not logged in. No sync will be performed.";
            src.m_nErrCode = RhoRuby.ERR_CLIENTISNOTLOGGEDIN;

            getNotify().fireSyncNotification(&src, true, src.m_nErrCode, "");
        }else
            getNotify().fireSyncNotification(null, true, RhoRuby.ERR_CLIENTISNOTLOGGEDIN, "");

        stopSync();
    }
}

void CSyncEngine::doSyncAllSources()
{
    prepareSync(esSyncAllSources);

    if ( isContinueSync() )
    {
	    PROF_CREATE_COUNTER("Net");	    
	    PROF_CREATE_COUNTER("Parse");
	    PROF_CREATE_COUNTER("DB");
	    PROF_CREATE_COUNTER("Data");
	    PROF_CREATE_COUNTER("Data1");
	    PROF_CREATE_COUNTER("Pull");
	    PROF_START("Sync");

        syncAllSources();

	    PROF_DESTROY_COUNTER("Net");	    
	    PROF_DESTROY_COUNTER("Parse");
	    PROF_DESTROY_COUNTER("DB");
	    PROF_DESTROY_COUNTER("Data");
	    PROF_DESTROY_COUNTER("Data1");
	    PROF_DESTROY_COUNTER("Pull");
	    PROF_STOP("Sync");

    }

    getNotify().cleanCreateObjectErrors();

    if ( getState() != esExit )
        setState(esNone);
}

void CSyncEngine::doSearch(rho::Vector<rho::String>& arSources, String strParams, String strAction, boolean bSearchSyncChanges, int nProgressStep)
{
    prepareSync(esSearch);
    if ( !isContinueSync() )
    {
        if ( getState() != esExit )
            setState(esNone);

        return;
    }

    CTimeInterval startTime = CTimeInterval::getCurrentTime();

    if ( bSearchSyncChanges )
    {
        for ( int i = 0; i < (int)arSources.size(); i++ )
        {
            CSyncSource* pSrc = findSourceByName(arSources.elementAt(i));
            if ( pSrc != null )
                pSrc->syncClientChanges();
        }
    }

    int nErrCode = 0;
    while( isContinueSync() )
    {
        int nSearchCount = 0;
        String strUrl = getProtocol().getServerQueryUrl(strAction);
        String strQuery = getProtocol().getServerQueryBody("", getClientID(), getSyncPageSize());

        if ( strParams.length() > 0 )
            strQuery += strParams;

        for ( int i = 0; i < (int)arSources.size(); i++ )
        {
            CSyncSource* pSrc = findSourceByName(arSources.elementAt(i));
            if ( pSrc != null )
            {
                strQuery += "&sources[][name]=" + pSrc->getName();

                if ( !pSrc->isTokenFromDB() && pSrc->getToken() > 1 )
                    strQuery += "&sources[][token]=" + convertToStringA(pSrc->getToken());
            }
        }

		LOG(INFO) + "Call search on server. Url: " + (strUrl+strQuery);
        NetResponse(resp,getNet().pullData(strUrl+strQuery, this));

        if ( !resp.isOK() )
        {
            stopSync();
			if (resp.isResponseRecieved())
				nErrCode = RhoRuby.ERR_REMOTESERVER;
			else
				nErrCode = RhoRuby.ERR_NETWORK;
            continue;
        }

        const char* szData = resp.getCharData();

        CJSONArrayIterator oJsonArr(szData);

        for( ; !oJsonArr.isEnd() && isContinueSync(); oJsonArr.next() )
        {
            CJSONArrayIterator oSrcArr(oJsonArr.getCurItem());

            int nVersion = 0;
            if ( !oSrcArr.isEnd() && oSrcArr.getCurItem().hasName("version") )
            {
                nVersion = oSrcArr.getCurItem().getInt("version");
                oJsonArr.next();
            }

            if ( nVersion != getProtocol().getVersion() )
            {
                LOG(ERROR) + "Sync server send search data with incompatible version. Client version: " + convertToStringA(getProtocol().getVersion()) +
                    "; Server response version: " + convertToStringA(nVersion);
                stopSync();
                nErrCode = RhoRuby.ERR_UNEXPECTEDSERVERRESPONSE;
                continue;
            }

            if ( !oSrcArr.getCurItem().hasName("source") )
            {
                LOG(ERROR) + "Sync server send search data without source name.";
                stopSync();
                nErrCode = RhoRuby.ERR_UNEXPECTEDSERVERRESPONSE;
                continue;
            }

            String strSrcName = oSrcArr.getCurItem().getString("source");
            CSyncSource* pSrc = findSourceByName(strSrcName);
            if ( pSrc == null )
            {
                LOG(ERROR) + "Sync server send search data for unknown source name:" + strSrcName;
                stopSync();
                nErrCode = RhoRuby.ERR_UNEXPECTEDSERVERRESPONSE;
                continue;
            }

            oSrcArr.reset(0);
            pSrc->m_bIsSearch = true;
            pSrc->setProgressStep(nProgressStep);
            pSrc->processServerResponse_ver3(oSrcArr);

            nSearchCount += pSrc->getCurPageCount();
        }

        if ( nSearchCount == 0 )
            break;
    }  

    if ( isContinueSync() )
    	getNotify().fireSyncNotification(null, true, RhoRuby.ERR_NONE, RhoRuby.getMessageText("sync_completed"));
    else if ( nErrCode != 0 )
    {
        CSyncSource& src = *m_sources.elementAt(getStartSource());
        src.m_nErrCode = nErrCode;
        src.m_bIsSearch = true;
        getNotify().fireSyncNotification(&src, true, src.m_nErrCode, "");
    }

    //update db info
    CTimeInterval endTime = CTimeInterval::getCurrentTime();
    unsigned long timeUpdated = CLocalTime().toULong();
    for ( int i = 0; i < (int)arSources.size(); i++ )
    {
        CSyncSource* pSrc = findSourceByName(arSources.elementAt(i));
        if ( pSrc == null )
            continue;
        CSyncSource& oSrc = *pSrc;
        oSrc.getDB().executeSQL("UPDATE sources set last_updated=?,last_inserted_size=?,last_deleted_size=?, \
						 last_sync_duration=?,last_sync_success=?, backend_refresh_time=? WHERE source_id=?", 
                         timeUpdated, oSrc.getInsertedCount(), oSrc.getDeletedCount(), 
                         (endTime-startTime).toULong(), oSrc.getGetAtLeastOnePage(), oSrc.getRefreshTime(),
                         oSrc.getID() );
    }
    //

    getNotify().cleanCreateObjectErrors();
    if ( getState() != esExit )
        setState(esNone);
}

void CSyncEngine::doSyncSource(const CSourceID& oSrcID)
{
    prepareSync(esSyncSource);

    if ( isContinueSync() )
    {
        CSyncSource* pSrc = findSource(oSrcID);
        if ( pSrc != null )
        {
            CSyncSource& src = *pSrc;
            LOG(INFO) +"Started synchronization of the data source: " + src.getName();

            src.sync();

            getNotify().fireSyncNotification(&src, true, src.m_nErrCode, src.m_nErrCode == RhoRuby.ERR_NONE ? RhoRuby.getMessageText("sync_completed") : "");
        }else
        {
            LOG(ERROR) + "Sync one source : Unknown Source " + oSrcID.toString();

            CSyncSource src(*this, getDB() );
    	    //src.m_strError = "Unknown sync source.";
            src.m_nErrCode = RhoRuby.ERR_RUNTIME;

            getNotify().fireSyncNotification(&src, true, src.m_nErrCode, "");
        }
    }

    getNotify().cleanCreateObjectErrors();

    if ( getState() != esExit )
        setState(esNone);
}

CSyncSource* CSyncEngine::findSource(const CSourceID& oSrcID)
{
    for( int i = 0; i < (int)m_sources.size(); i++ )
    {
        CSyncSource& src = *m_sources.elementAt(i);
        if ( oSrcID.isEqual(src) )
            return &src;
    }
    
    return null;
}

CSyncSource* CSyncEngine::findSourceByName(const String& strSrcName)
{
    return findSource(CSourceID(strSrcName));
}

void CSyncEngine::loadAllSources()
{
    m_sources.clear();
    m_bHasUserPartition = false;
    m_bHasAppPartition = false;

    DBResult( res, getDB().executeSQL("SELECT source_id,sync_type,token,name, partition from sources ORDER BY priority") );
    for ( ; !res.isEnd(); res.next() )
    { 
        String strShouldSync = res.getStringByIdx(1);
        if ( strShouldSync.compare("none") == 0 || strShouldSync.compare("bulk_sync_only") == 0 )
            continue;

        String strName = res.getStringByIdx(3);
        String strPartition = res.getStringByIdx(4);
        m_bHasUserPartition = m_bHasUserPartition || strPartition.compare("user") == 0;
        m_bHasAppPartition = m_bHasAppPartition || strPartition.compare("app") == 0;

        m_sources.addElement( new CSyncSource( res.getIntByIdx(0), strName, res.getUInt64ByIdx(2), 
            (strPartition.compare("user") == 0 ? getDB() : getAppDB()), *this) );
    }
}

String CSyncEngine::loadClientID()
{
    String clientID = "";
    synchronized(m_mxLoadClientID)
    {
        boolean bResetClient = false;
        int nBulkSyncState = 0;
        {
            DBResult( res, getDB().executeSQL("SELECT client_id,reset,bulksync_state from client_info limit 1") );
            if ( !res.isEnd() )
            {
                clientID = res.getStringByIdx(0);
                bResetClient = res.getIntByIdx(1) > 0;
                nBulkSyncState = res.getIntByIdx(2);
            }
        }

        if ( clientID.length() == 0 )
        {
            clientID = requestClientIDByNet();

            DBResult( res , getDB().executeSQL("SELECT * FROM client_info") );
            if ( !res.isEnd() )
                getDB().executeSQL("UPDATE client_info SET client_id=?", clientID);
            else
                getDB().executeSQL("INSERT INTO client_info (client_id) values (?)", clientID);

        }else if ( bResetClient )
        {
    	    if ( !resetClientIDByNet(clientID) )
    		    stopSync();
    	    else
    		    getDB().executeSQL("UPDATE client_info SET reset=? where client_id=?", 0, clientID );	    	
        }

       	doBulkSync(clientID, nBulkSyncState);
    }
    return clientID;
}

boolean CSyncEngine::resetClientIDByNet(const String& strClientID)//throws Exception
{
    NetResponse( resp, getNet().pullData(getProtocol().getClientResetUrl(strClientID), this) );
    return resp.isOK();
}

String CSyncEngine::requestClientIDByNet()
{
    NetResponse(resp,getNet().pullData(getProtocol().getClientCreateUrl(), this));
    if ( resp.isOK() && resp.getCharData() != null )
    {
        const char* szData = resp.getCharData();
        CJSONEntry oJsonEntry(szData);

        CJSONEntry oJsonObject = oJsonEntry.getEntry("client");
        if ( !oJsonObject.isEmpty() )
            return oJsonObject.getString("client_id");
    }

    return "";
}

void CSyncEngine::doBulkSync(String strClientID, int nBulkSyncState)//throws Exception
{
    //TODO:doBulkSync
    if ( nBulkSyncState >= 2 || !isContinueSync() )
        return;

	LOG(INFO) + "Bulk sync: start";
	getNotify().fireBulkSyncNotification(false, RhoRuby.ERR_NONE);

    if ( nBulkSyncState == 0 && m_bHasUserPartition )
    {
        loadBulkPartition(getDB(), "user", strClientID);

        if ( !isContinueSync() )
            return;

	    getDB().executeSQL("UPDATE client_info SET bulksync_state=1 where client_id=?", strClientID );	    	
    }

    if ( m_bHasAppPartition )
        loadBulkPartition(getAppDB(), "app", strClientID);

    if ( !isContinueSync() )
        return;

    getDB().executeSQL("UPDATE client_info SET bulksync_state=2 where client_id=?", strClientID );

    getNotify().fireBulkSyncNotification(true, RhoRuby.ERR_NONE);        
}

static String getHostFromUrl( const String& strUrl );
void CSyncEngine::loadBulkPartition(db::CDBAdapter& dbPartition, const String& strPartition, const String& strClientID )
{
    String serverUrl = RHOCONF().getPath("syncserver");
    String strUrl = serverUrl + "bulk_data";
    String strQuery = "?client_id=" + strClientID + "&partition=" + strPartition;
    String strDataUrl = "", strCmd = "";

    while(strCmd.length() == 0)
    {	    
        NetResponse( resp, getNet().pullData(strUrl+strQuery, this) );
        if ( !resp.isOK() || resp.getCharData() == null )
        {
    	    LOG(ERROR) + "Bulk sync failed: server return an error.";
    	    stopSync();
    	    getNotify().fireBulkSyncNotification(true, RhoRuby.ERR_REMOTESERVER);
    	    return;
        }

	    LOG(INFO) + "Bulk sync: got response from server: " + resp.getCharData();
    	
        const char* szData = resp.getCharData();
        CJSONEntry oJsonEntry(szData);
        strCmd = oJsonEntry.getString("result");
        if ( oJsonEntry.hasName("url") )
   	        strDataUrl = oJsonEntry.getString("url");
        
        if ( strCmd.compare("wait") == 0)
        {
            int nTimeout = RHOCONF().getInt("bulksync_timeout_sec");
            if ( nTimeout == 0 )
                nTimeout = 5;

            CSyncThread::getInstance()->sleep(nTimeout*1000);
            strCmd = "";
        }
    }

    if ( strCmd.compare("nop") == 0)
    {
	    LOG(INFO) + "Bulk sync return no data.";
	    return;
    }

    String fDataName = dbPartition.getDBPath() + "_bulk";

    LOG(INFO) + "Bulk sync: download data from server: " + strDataUrl;
    strDataUrl = getHostFromUrl(serverUrl) + strDataUrl;

    NetResponse( resp1, getNet().pullFile(strDataUrl, fDataName, this) );
    if ( !resp1.isOK() )
    {
	    LOG(ERROR) + "Bulk sync failed: cannot download database file.";
	    stopSync();
	    getNotify().fireBulkSyncNotification(true, RhoRuby.ERR_REMOTESERVER);
	    return;
    }

	LOG(INFO) + "Bulk sync: change db";
    
    dbPartition.setBulkSyncDB(fDataName);
}

int CSyncEngine::getStartSource()
{
    for( int i = 0; i < (int)m_sources.size(); i++ )
    {
        CSyncSource& src = *m_sources.elementAt(i);
        if ( !src.isEmptyToken() )
            return i;
    }

    return 0;
}

void CSyncEngine::syncAllSources()
{
    //TODO: do not stop on error source
    boolean bError = false;
    for( int i = getStartSource(); i < (int)m_sources.size() && isContinueSync(); i++ )
    {
        CSyncSource& src = *m_sources.elementAt(i);
        if ( isSessionExist() && getState() != esStop )
            src.sync();

        getNotify().onSyncSourceEnd(i, m_sources);
        bError = src.m_nErrCode != RhoRuby.ERR_NONE;
    }

    if ( !bError)
    	getNotify().fireSyncNotification(null, true, RhoRuby.ERR_NONE, RhoRuby.getMessageText("sync_completed"));
}

void CSyncEngine::callLoginCallback(String callback, int nErrCode, String strMessage)
{
	//try{
    String strBody = "error_code=" + convertToStringA(nErrCode);
    strBody += "&error_message=";
    URI::urlEncode(strMessage, strBody);
    strBody += "&rho_callback=1";

    String strUrl = getNet().resolveUrl(callback);
    
	LOG(INFO) + "Login callback: " + callback + ". Body: "+ strBody;

    NetResponse( resp, getNet().pushData( strUrl, strBody, null ) );
    if ( !resp.isOK() )
        LOG(ERROR) + "Call Login callback failed. Code: " + resp.getRespCode() + "; Error body: " + resp.getCharData();
	//}catch(Exception exc)
	//{
	//	LOG.ERROR("Call Login callback failed.", exc);
	//}
}

void CSyncEngine::login(String name, String password, String callback)
{
    PROF_START("Login");
	//try {

    NetResponse( resp, getNet().pullCookies( getProtocol().getLoginUrl(), getProtocol().getLoginBody(name, password), this ) );
    
    if ( !resp.isResponseRecieved())
    {
        callLoginCallback(callback, RhoRuby.ERR_NETWORK, resp.getCharData());
        return;
    }

    if ( resp.isUnathorized() )
    {
        callLoginCallback(callback, RhoRuby.ERR_UNATHORIZED, resp.getCharData());
    	return;
    }

    if ( !resp.isOK() )
    {
        callLoginCallback(callback, RhoRuby.ERR_REMOTESERVER, resp.getCharData());
    	return;
    }

    String strSession = resp.getCharData();
    if ( strSession.length() == 0 )
    {
    	LOG(ERROR) + "Return empty session.";
    	callLoginCallback(callback, RhoRuby.ERR_UNEXPECTEDSERVERRESPONSE, "" );
        return;
    }

    DBResult( res , getDB().executeSQL("SELECT * FROM client_info") );
    if ( !res.isEnd() )
        getDB().executeSQL( "UPDATE client_info SET session=?", strSession );
    else
        getDB().executeSQL("INSERT INTO client_info (session) values (?)", strSession);


    if ( CClientRegister::getInstance() != null )
        CClientRegister::getInstance()->stopWait();
    
    callLoginCallback(callback, RhoRuby.ERR_NONE, "" );
	
    PROF_STOP("Login");
	//}catch(Exception exc)
	//{
	//	LOG.ERROR("Login failed.", exc);
    //	callLoginCallback(callback, RhoRuby.ERR_RUNTIME, exc.getMessage() );
	//}
}

boolean CSyncEngine::isLoggedIn()
 {
    int nCount = 0;
    DBResult( res , getDB().executeSQL("SELECT count(session) FROM client_info WHERE session IS NOT NULL") );
    if ( !res.isEnd() )
        nCount = res.getIntByIdx(0);

    return nCount > 0;
}

String CSyncEngine::loadSession()
{
    String strRes = "";
    DBResult( res , getDB().executeSQL("SELECT session FROM client_info WHERE session IS NOT NULL") );
    
    if ( !res.isEnd() )
    	strRes = res.getStringByIdx(0);
    
    return strRes;
}

void CSyncEngine::logout()
{
    getDB().executeSQL( "UPDATE client_info SET session=NULL" );
    m_strSession = "";
    getNet().deleteCookie("");

    loadAllSources();
    //TODO: remove deleteCookie
    getNet().deleteCookie(RHOCONF().getPath("syncserver"));
}
	
void CSyncEngine::setSyncServer(char* syncserver)
{
	rho_conf_setString("syncserver", syncserver);
	rho_conf_save();

    getDB().executeSQL("DELETE FROM client_info");

	logout();
}

static String getHostFromUrl( const String& strUrl )
{
    const char* url = strUrl.c_str();
    const char* pStartSrv, *pEndSrv;
    int nSrvLen;
    const char* pHttp = strstr(url,"://");
    if ( !pHttp )
        pHttp = strstr(url,":\\\\");

    if ( pHttp )
        pStartSrv = pHttp+3;
    else
        pStartSrv = url;

    pEndSrv = strchr( pStartSrv, '/');
    if ( !pEndSrv )
        pEndSrv = strchr( pStartSrv, '\\');

    nSrvLen = pEndSrv ? (pEndSrv+1 - url) : strlen(url);
    return String(url, nSrvLen);
}

String CSyncEngine::CSourceID::toString()const
{
    if ( m_strName.length() > 0 )
        return "name : " + m_strName;

    return "# : " + convertToStringA(m_nID);
}

boolean CSyncEngine::CSourceID::isEqual(CSyncSource& src)const
{
    if ( m_strName.length() > 0 )
        return src.getName().compare(m_strName)==0;

    return m_nID == src.getID();
}
}
}

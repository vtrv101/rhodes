/*
 *  rhodes
 *
 *  Copyright (C) 2008 Rhomobile, Inc. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
package com.rho.sync;

import com.rho.RhoClassFactory;
import com.rho.RhoEmptyLogger;
import com.rho.RhoEmptyProfiler;
import com.rho.RhoLogger;
import com.rho.RhoProfiler;
import com.rho.net.*;
import com.rho.db.*;
import java.util.Vector;

import org.json.me.JSONException;

import com.rho.FilePath;
import com.rho.TimeInterval;
import com.rho.RhoRuby;

class SyncSource
{
	private static final RhoLogger LOG = RhoLogger.RHO_STRIP_LOG ? new RhoEmptyLogger() : 
		new RhoLogger("Sync");
	
	private static final RhoProfiler PROF = RhoProfiler.RHO_STRIP_PROFILER ? new RhoEmptyProfiler() : 
		new RhoProfiler();

	public static final int edpNone = 0, edpDeleteObjects = 2;
	
	static class SyncBlob
	{
	   String m_strBody;
	   String m_strFilePath;

	   SyncBlob(String body, String filePath ){
	      m_strBody = body; m_strFilePath = filePath;
	   }

	   String getBody(){ return m_strBody; }
	   String getFilePath(){ return m_strFilePath; }
	};
	
    SyncEngine m_syncEngine;

    Integer  m_nID;
    String m_strName;
    String m_strUrl;
    long m_token = 0;
    boolean m_bTokenFromDB; 
    
    int m_nCurPageCount, m_nInserted, m_nDeleted, m_nTotalCount, m_nAttribCounter=0;
    boolean m_bGetAtLeastOnePage = false;
    int m_eSyncServerDataPass = edpNone;
    int m_nErrCode = RhoRuby.ERR_NONE;
    String m_strError = "";
    String m_strParams = "";
    String m_strAction = "";
    boolean m_bSearchSyncChanges = false;
    int     m_nProgressStep = -1;
    String m_strUrlParams = "";
    
	String m_strPushBody = "";
    Vector/*Ptr<CSyncBlob*>*/ m_arSyncBlobs = new Vector();
    
    String m_strAskParams = "";
    int m_nRefreshTime = 0;
    
    String getUrl() { return m_strUrl; }
    Integer getID() { return m_nID; }
    String getName() { return m_strName; }
    int getServerObjectsCount(){ return m_nInserted+m_nDeleted; }
    boolean isSearch(){ return m_strParams.length() > 0;}
    
    long getToken(){ return m_token; }
    void setToken(long token){ m_token = token; m_bTokenFromDB = false; }
    boolean isEmptyToken()
    {
    	return m_token == 0;
    }
    void setAskParams(String ask){ m_strAskParams = ask;}
    String getAskParams(){ return m_strAskParams;}
    int getInsertedCount() { return m_nInserted; }
    int getDeletedCount() { return m_nDeleted; }
    void setCurPageCount(int nCurPageCount){m_nCurPageCount = nCurPageCount;}
    void setTotalCount(int nTotalCount){m_nTotalCount = nTotalCount;}
    int  getCurPageCount(){return m_nCurPageCount;}
    int  getTotalCount(){return m_nTotalCount;}
    int  getProgressStep(){ return m_nProgressStep; }

    void setSyncServerDataPass(int ePass){m_eSyncServerDataPass = ePass;}
    boolean isDeleteObjectsPass(){ return m_eSyncServerDataPass == edpDeleteObjects; }
    
    SyncEngine getSync(){ return m_syncEngine; }
    SyncNotify getNotify(){ return getSync().getNotify(); }
	DBAdapter getDB(){ return getSync().getDB(); }
	NetRequest getNet(){ return getSync().getNet(); }
	void setRefreshTime( int nRefreshTime ){ m_nRefreshTime = nRefreshTime;}
	void setUrlParams(String strParams){m_strUrlParams=strParams!=null?strParams:"";}
	void setUrl(String strUrl){m_strUrl=strUrl;}
	
    SyncSource(int id, String strUrl, String name, long token, SyncEngine syncEngine )
    {
    	m_syncEngine = syncEngine;
        m_nID = new Integer(id);
        m_strUrl = strUrl;
        m_strName = name;
        m_token = token;
        m_bTokenFromDB = true;
        
        m_nCurPageCount = 0;
        m_nInserted = 0;
        m_nDeleted = 0;
    }
	
    SyncSource(SyncEngine syncEngine)
    {
    	m_syncEngine = syncEngine;
        m_nID = new Integer(0);
        m_strUrl = "";
        m_strName = "";
        m_bTokenFromDB = true;
        
        m_nCurPageCount = 0;
        m_nInserted = 0;
        m_nDeleted = 0;
    }
    
	void sync() throws Exception
	{
    	getNotify().fireSyncNotification(null, false, RhoRuby.ERR_NONE, RhoRuby.getMessageText("syncronizing") + getName() + "...");
		
	    TimeInterval startTime = TimeInterval.getCurrentTime();
	    
	    try{
	    	PROF.START("Pull");
	        if ( isEmptyToken() )
	            processToken(1);
	    	
	        boolean bSyncedServer = false;
	        if ( m_strParams.length() == 0 || m_bSearchSyncChanges )
	        {
	            if ( isPendingClientChanges() )
	            {
	            	LOG.INFO("Client has unconfirmed items in database. Sync server first.");
	                syncServerChanges();
	                bSyncedServer = true;
	            }

	            if ( bSyncedServer && isPendingClientChanges() )
	            {
	            	LOG.ERROR("Client still has unconfirmed items in database. Sync is stopped.");
	                getSync().setState(SyncEngine.esStop);
	            }
	            else
	            {
	                boolean bSyncClient = false;
	                {
	                    IDBResult res = getDB().executeSQL("SELECT object FROM changed_values WHERE source_id=? and sent<=1 LIMIT 1 OFFSET 0", getID());
	                    bSyncClient = !res.isEnd();
	                }
	                if ( bSyncClient )
	                {
	                    syncClientChanges();
	                    getAndremoveAsk();
	                    bSyncedServer = false;
	                }
	            }
	        }
	        PROF.STOP("Pull");

	        if ( !bSyncedServer )
	        	syncServerChanges();
	        
	    }catch(Exception exc)
	    {
	    	getSync().stopSync();
	    	throw exc;
	    }finally{
		   TimeInterval endTime = TimeInterval.getCurrentTime();
		    getDB().executeSQL("UPDATE sources set last_updated=?,last_inserted_size=?,last_deleted_size=?, "+
								 "last_sync_duration=?,last_sync_success=?, backend_refresh_time=? WHERE source_id=?", 
		                         new Long(endTime.toULong()/1000), new Integer(getInsertedCount()), new Integer(getDeletedCount()), new Long((endTime.minus(startTime)).toULong()), 
		                         new Integer(m_bGetAtLeastOnePage?1:0), new Integer(m_nRefreshTime), getID() );
	    }
	}

	boolean isPendingClientChanges()throws DBException
	{
	    IDBResult res = getDB().executeSQL("SELECT object FROM changed_values WHERE source_id=? and update_type='create' and sent>1  LIMIT 1 OFFSET 0", getID());
	    return !res.isEnd();
	}
	
	void syncClientBlobs(String strBaseQuery)throws Exception
	{
	    String strQuery;
	    for( int i = 0; i < m_arSyncBlobs.size(); i ++)
	    {
	        SyncBlob blob = (SyncBlob)m_arSyncBlobs.elementAt(i);
	
	        String strFilePath = RhoClassFactory.createFile().getDirPath("");
	        String strBlobPath = blob.getFilePath(); 
	        if ( strBlobPath.startsWith("/") )
	        	strBlobPath = strBlobPath.substring(1);
	        
	        strFilePath += strBlobPath;
	        
	        strQuery = strBaseQuery + "&" + blob.getBody();
	        try{
		        NetResponse resp = getNet().pushFile(strQuery, strFilePath, getSync(), null );
		        if ( !resp.isOK() )
		        {
		            getSync().setState(SyncEngine.esStop);
		        	m_nErrCode = RhoRuby.ERR_REMOTESERVER;
		        	//m_strError = resp.getCharData();
		            return;
		        }
		    }catch(Exception exc)
		    {
		    	m_nErrCode = RhoRuby.getNetErrorCode(exc);
		    	throw exc;
		    }

		    getDB().startTransaction();
	        getDB().executeSQL("DELETE FROM changed_values WHERE source_id=? and attrib_type=? and value=?", getID(), "blob.file", blob.getFilePath() );
	        getDB().executeSQL("DELETE FROM object_values WHERE source_id=? and attrib_type=? and value=?", getID(), "blob.file", blob.getFilePath() );
	        getDB().endTransaction();
	    }
	
	    m_arSyncBlobs.removeAllElements();
	}

	void syncClientChanges()throws Exception
	{
	    String[] arUpdateTypes = {"create", "update", "delete"};
	    for( int i = 0; i < 3 && getSync().isContinueSync(); i++ )
	    {
	        String strUrl = getUrl() + "/" + arUpdateTypes[i];
	        strUrl += "objects";
	        String strQuery = SyncEngine.SYNC_SOURCE_FORMAT() + "&client_id=" + getSync().getClientID();

	        m_arSyncBlobs.removeAllElements();
	        String strBody = makePushBody(arUpdateTypes[i]);
	        if ( strBody.length() > 0 )
	        {
			    LOG.INFO( "Push client changes to server. Source: " + getName() + "Size :" + strBody.length() );
			    LOG.TRACE("Push body: " + strBody);		
	 
	            NetResponse resp = getNet().pushData(strUrl+strQuery,strBody, getSync());
	            if ( !resp.isOK() )
	            {
	                getSync().setState(SyncEngine.esStop);
	                m_nErrCode = RhoRuby.ERR_REMOTESERVER;
	                continue;
	            }
	        }
	 
	        if ( m_arSyncBlobs.size()>0 )
	        {
			    LOG.INFO( "Push blobs to server. Source: " + getName() + "Count :" + m_arSyncBlobs.size() );
	            //oo conflicts
	            if ( i < 1 ) //create
	                getDB().executeSQL("UPDATE changed_values SET sent=2 WHERE source_id=? and update_type=? and (attrib_type IS NULL or attrib_type!=?) and sent=1", 
	                    getID(), arUpdateTypes[i], "blob.file" );
	            else
	            //
	            	getDB().executeSQL("DELETE FROM changed_values WHERE source_id=? and update_type=? and (attrib_type IS NULL or attrib_type!=?) and sent=1", 
	            		getID(), arUpdateTypes[i], "blob.file" );
	            
	            syncClientBlobs(strUrl+strQuery);
	        }else if ( strBody.length() > 0 )
	        {
	            //oo conflicts
	            if ( i < 1 ) //create
	                getDB().executeSQL("UPDATE changed_values SET sent=2 WHERE source_id=? and update_type=? and sent=1", getID(), arUpdateTypes[i] );
	            else
	            //
	            	getDB().executeSQL("DELETE FROM changed_values WHERE source_id=? and update_type=? and sent=1", getID(), arUpdateTypes[i] );
	        }
	    }
	}
	
	/*
	 * Construct the body of the request by filtering 
	 * the attr_filter string. The body format should
	 * look like the following:
	 * create: attrvals[][attrib]=<name|industry>&attrvals[][object]=<locallygeneratedid>&attrvals[][value]=<some value>
	 * update: attrvals[][attrib]=<name|industry>&attrvals[][object]=<remoteid>&attrvals[][value]=<some new value>
	 * delete: attrvals[][attrib]=<name|industry>&attrvals[][object]=<remoteid>
	 */
	
	String makePushBody( String szUpdateType)throws DBException
	{
		String strBody = "";
        getDB().Lock();
		
	    IDBResult res = getDB().executeSQL("SELECT attrib, object, value, attrib_type, main_id "+
						 "FROM changed_values where source_id=? and update_type =? and sent<=1 ORDER BY sent DESC", getID(), szUpdateType );
	    if ( res.isEnd() )
	    {
	    	getDB().Unlock();
	    	return strBody;
	    }
	    
	    for( ; !res.isEnd(); res.next() )
	    {
	        String strSrcBody = "attrvals[][attrib]=" + res.getStringByIdx(0);
	
	        if ( res.getStringByIdx(1).length() > 0 ) 
	            strSrcBody += "&attrvals[][object]=" + res.getStringByIdx(1);

	        long main_id = res.getLongByIdx(4);
	        if ( main_id != 0 )
	            strSrcBody += "&attrvals[][id]=" + main_id;
	        
	        String value = res.getStringByIdx(2);
	        String attribType = res.getStringByIdx(3);
	
	
	        //if ( value.length() > 0 )
	        {
	            if ( attribType.equals("blob.file") )
	            {
	                FilePath oBlobPath = new FilePath(value);
	                strSrcBody += "&attrvals[][value]=";
	                strSrcBody += oBlobPath.getBaseName();
	                strSrcBody += "&attrvals[][attrib_type]=blob";
	
	                if ( value.length() > 0 )
	                	m_arSyncBlobs.addElement(new SyncBlob(strSrcBody,value));
	                continue;
	            }else
	                strSrcBody += "&attrvals[][value]=" + value;
	        }
	
	        if ( strBody.length() > 0 )
	            strBody += "&";
	
	        strBody += strSrcBody;
	    }
	    
	    getDB().executeSQL("UPDATE changed_values SET sent=1 WHERE source_id=? and update_type=? and sent=0", getID(), szUpdateType );	    
	    getDB().Unlock();
	    
	    return strBody;
	}

	void getAndremoveAsk()throws DBException
	{
	    String askParams = "";
	    {
	        IDBResult res = getDB().executeSQL("SELECT object, attrib, value "+
				     "FROM changed_values WHERE source_id=? and update_type =?", getID(), "ask" );
	        if ( !res.isEnd() )
	        {
	            askParams = res.getStringByIdx(2);

	            getDB().executeSQL("DELETE FROM object_values WHERE object=? and attrib=? and source_id=?", 
	                res.getStringByIdx(0), res.getStringByIdx(1), getID() );
	        }
	        
		    getDB().executeSQL("DELETE FROM changed_values WHERE source_id=? and update_type=?", getID(), "ask" );
	    }

	    setAskParams(askParams);
	}
	
	void syncServerChanges()throws Exception
	{
		LOG.INFO("Sync server changes source ID :" + getID() );
		
	    while( getSync().isContinueSync() )
	    {
	        setCurPageCount(0);
	        String strUrl = getUrl();
	        if ( m_strAction.length() > 0 )
	            strUrl = FilePath.join(strUrl, m_strAction);
	        
			String strQuery = SyncEngine.SYNC_SOURCE_FORMAT() + "&client_id=" + getSync().getClientID() + 
	                "&p_size=" + getSync().SYNC_PAGE_SIZE() + "&version=" + getSync().SYNC_VERSION();
	        if ( m_strParams.length() > 0 )
	            strQuery += m_strParams;
	
	        if( m_strUrlParams.length() > 0 )
	            strQuery += "&" + m_strUrlParams;
	        	
	        if ( getAskParams().length() > 0 )
	        {
	            getSync();
				strUrl +=  SyncEngine.SYNC_ASK_ACTION();
	            strQuery += "&question=" + getAskParams();
	        }
	
	        if ( !m_bTokenFromDB && getToken() > 1 )
	            strQuery += "&ack_token=" + getToken();
	
			LOG.INFO( "Pull changes from server. Url: " + (strUrl+strQuery) );
			
			NetResponse resp = null;
			try{
			    PROF.START("Net");	    
				resp = getNet().pullData(strUrl+strQuery, getSync());
				PROF.STOP("Net");
				
		        if ( !resp.isOK() )
		        {
		            getSync().stopSync();
		        	m_nErrCode = RhoRuby.ERR_REMOTESERVER;
		        	//m_strError = resp.getCharData();
		            
		            continue;
		        }
		    }catch(Exception exc)
		    {
		    	m_nErrCode = RhoRuby.getNetErrorCode(exc);
		    	throw exc;
		    }
	
	        processServerData(resp.getCharData());
	        
			//String strData =
			//	"[{count:16},{version:2},{rt:0},{total_count:16},{token:\"31272969717999\"},{s:\"Years\",ol:[{o:\"7\",av:[{a:\"year\",i:847548358725629473,v:\"2000\"}]},{o:\"11\",av:[{a:\"year\",i:412366391662703410,v:\"1996\"}]},{o:\"20\",av:[{a:\"year\",i:137205700852855225,v:\"2010\"}]},{o:\"8\",av:[{a:\"year\",i:679868441309756807,v:\"1999\"}]},{o:\"12\",av:[{a:\"year\",i:456800387229102986,v:\"1995\"}]},{o:\"3\",av:[{a:\"year\",i:734226969968776876,v:\"2004\"}]},{o:\"9\",av:[{a:\"year\",i:56010014622863357,v:\"1998\"}]},{o:\"17\",av:[{a:\"year\",i:822839088903421583,v:\"2007\"}]},{o:\"4\",av:[{a:\"year\",i:694980267678628747,v:\"2003\"}]},{o:\"18\",av:[{a:\"year\",i:454618145604696729,v:\"2008\"}]},{o:\"5\",av:[{a:\"year\",i:161829862235046918,v:\"2002\"}]},{o:\"1\",av:[{a:\"year\",i:429713603438265228,v:\"2006\"}]},{o:\"19\",av:[{a:\"year\",i:53905967515604512,v:\"2009\"}]},{o:\"6\",av:[{a:\"year\",i:623979215240715853,v:\"2001\"}]},{o:\"10\",av:[{a:\"year\",i:906257189782972492,v:\"1997\"}]},{o:\"2\",av:[{a:\"year\",i:659684360823905556,v:\"2005\"}]}]}]";				
	        //"[{count:10},{version:1},{total_count: 5425},{token: 123},{s:\"RhoDeleteSource\",ol:[{o:\"rho_del_obj\",av:[{i:55550425},{i:75665819},{i:338165272},{i:402396629},{i:521753981},{i:664143530},{i:678116186},{i:831092394},{i:956041217},{i:970452458}]}]}]";
			/*"[{count: 124},{version: 1},{total_count: 5425},{token: 123},"
	        "{s:\"Product\",ol:["
			"{oo:\"123\",o:\"2ed2e0c7-8c4c-99c6-1b37-498d250bb8e7\",av:["
			"{a:\"first_name\",i:47354289,v:\"Lars. \n\n Burgess\", t:\"blob\"},"
	        "{a:\"second_name\",i:55555,v:\"Burgess\"}]},"
	        "{oo:\"456\", e:\"Something went wrong creating this record on the backend: code 7\"}"
	        "]}]"; */
			/*"[{count: 1},{version: 1},{total_count: 1},{token: 123},"
	        "{s:\"Product\",ol:["
	        "{oo:\"94\", e:\"Something went wrong creating this record on the backend: code 7\"}"
	        "]}]";*/
			/*"[{count: 1},{version: 1},{total_count: 1},{token: 123},"
	        "{s:\"Product\",ol:["
	        "{o:\"94\", av:["
	        "{a:\"TEST\",i:55555,v:\"Geny\"}]},"
	        "]}]";

			//u:\"query\",  
			 */
			//processServerData(strData);

	        if ( getAskParams().length() > 0 || getCurPageCount() == 0 )
	            break;
	    }
	}

	void processServerData(String szData)throws Exception
	{
		LOG.TRACE("Server data: " + szData);
		PROF.START("Parse");		
	    JSONArrayIterator oJsonArr = new JSONArrayIterator(szData);
	    PROF.STOP("Parse");
	    
	    PROF.START("Data1");
	    if ( !oJsonArr.isEnd() && oJsonArr.getCurItem().hasName("error") )
	    {
	        m_strError = oJsonArr.getCurItem().getString("error");
	        m_nErrCode = RhoRuby.ERR_CUSTOMSYNCSERVER;
	        getSync().stopSync();
	        return;
	    }
	    
	    if ( !oJsonArr.isEnd() )
	    {
	        setCurPageCount(oJsonArr.getCurItem().getInt("count"));
	        oJsonArr.next();
	    }
	    int nVersion = 0;
	    if ( !oJsonArr.isEnd() && oJsonArr.getCurItem().hasName("version") )
	    {
	        nVersion = oJsonArr.getCurItem().getInt("version");
	        oJsonArr.next();
	    }
	    
	    if ( nVersion != getSync().SYNC_VERSION() )
	    {
	        LOG.ERROR("Sync server send data with incompatible version. Client version: " + getSync().SYNC_VERSION() +
	            "; Server response version: " + nVersion + ". Source name: " + getName() );
	        getSync().stopSync();
	        m_nErrCode = RhoRuby.ERR_SYNCVERSION;
	        return;
	    }
	    
	    if ( !oJsonArr.isEnd() && oJsonArr.getCurItem().hasName("rt") )
	    {
	        setRefreshTime(oJsonArr.getCurItem().getInt("rt"));
	        oJsonArr.next();
	    }
	    
	    if ( !oJsonArr.isEnd() && oJsonArr.getCurItem().hasName("total_count") )
	    {
	        setTotalCount(oJsonArr.getCurItem().getInt("total_count"));
	        oJsonArr.next();
	    }
	    
//	    if ( getServerObjectsCount() == 0 )
//	    	getNotify().fireSyncNotification(this, false, RhoRuby.ERR_NONE, "");
	    
	    if ( !oJsonArr.isEnd() )
	    {
	        processToken(oJsonArr.getCurItem().getUInt64("token"));
	        oJsonArr.next();
	    }else if ( getCurPageCount() == 0 )
	    {
	    	//oo conflicts
	    	getDB().executeSQL("DELETE FROM changed_values where source_id=? and sent>=3", getID() );
	    	//
	        processToken(0);
	    }
	    
		LOG.INFO( "Got " + getCurPageCount() + "(Processed: " +  getServerObjectsCount() + ") records of " + getTotalCount() + " from server. Source: " + getName()
				+ ". Version: " + nVersion );
		
		PROF.STOP("Data1");
		if ( !oJsonArr.isEnd() && getSync().isContinueSync() )
		{
			PROF.START("Data");
		    getDB().startTransaction();

		    try{
	            int nSavedPos = oJsonArr.getCurPos();
	            setSyncServerDataPass(edpNone);
	            processServerData_Ver1(oJsonArr);

	            setSyncServerDataPass(edpDeleteObjects);
	            oJsonArr.reset(nSavedPos);
	            processServerData_Ver1(oJsonArr);
		        
			    PROF.STOP("Data");		    
		    	
		    	PROF.START("DB");
				getDB().endTransaction();
			    PROF.STOP("DB");
			    
			    getNotify().fireObjectsNotification();
		        
		    }catch(Exception exc){
		    	getDB().rollback();
		    	throw exc;
			}
		    
		}
		
		PROF.START("Data1");
		if ( getCurPageCount() > 0 )
	    	getNotify().fireSyncNotification(this, false, RhoRuby.ERR_NONE, "");
		PROF.STOP("Data1");
	}

	boolean processSyncObject_ver1(JSONEntry oJsonObject, Integer nSrcID)throws Exception
	{
		String strOldObject = oJsonObject.getString("oo");
	    if ( isDeleteObjectsPass() != (nSrcID.intValue() < 0) )
	    	return true;
	    
	    if ( oJsonObject.hasName("e") )
	    {
	    	String strError = oJsonObject.getString("e");
	        getNotify().addCreateObjectError(nSrcID,strOldObject,strError);
	        return true;
	    }

		String strObject = oJsonObject.getString("o");
		JSONArrayIterator oJsonArr = new JSONArrayIterator(oJsonObject, "av");
	    //oo conflicts
	    boolean bUpdatedOO = false;
	    //
	    for( ; !oJsonArr.isEnd() && getSync().isContinueSync(); m_nAttribCounter++, oJsonArr.next() )
		{
			JSONEntry oJsonEntry = oJsonArr.getCurItem();
	        if ( oJsonEntry.isEmpty() )
	        	continue;
	        
		    if ( nSrcID.intValue() >= 0 ) //insert
		    {
		    	CValue value = new CValue(oJsonEntry,1);
		    	if ( !downloadBlob(value) )
		    		return false;
		    	
		    	String strAttrib = oJsonEntry.getString("a");
	            //oo conflicts
	            if ( strOldObject != null && !bUpdatedOO )
	            {
                    getDB().executeSQL("UPDATE object_values SET object=? where object=? and source_id=?", strObject, strOldObject, nSrcID );
                    getDB().executeSQL("UPDATE changed_values SET object=? where object=? and source_id=?", strObject, strOldObject, nSrcID );

                    getNotify().onObjectChanged(nSrcID,strOldObject, SyncNotify.enCreate);

                    bUpdatedOO = true;
	            }
	            
                IDBResult resInsert = getDB().executeSQLReportNonUnique("INSERT INTO object_values "+
                    "(id, attrib, source_id, object, value, attrib_type) VALUES(?,?,?,?,?,?)", 
                    value.m_nID, strAttrib, nSrcID, strObject,
                    value.m_strValue, value.m_strAttrType );
                if ( resInsert.isNonUnique() )
                {
                    getDB().executeSQL("UPDATE object_values "+
                        "SET id=?, value=?, attrib_type=? WHERE object=? and attrib=? and source_id=?", 
                        value.m_nID, value.m_strValue, value.m_strAttrType,
                        strObject, strAttrib, nSrcID );

                    // oo conflicts
                    getDB().executeSQL("UPDATE changed_values SET main_id=? where object=? and attrib=? and source_id=? and sent<=1", value.m_nID, strObject, strAttrib, nSrcID );
                    getDB().executeSQL("UPDATE changed_values SET sent=4 where object=? and attrib=? and source_id=? and sent>1", strObject, strAttrib, nSrcID );
                    //
                }

                getNotify().onObjectChanged(nSrcID,strObject, SyncNotify.enUpdate);
		    	
		        m_nInserted++;
		    }else
		    {
		    	long id = oJsonEntry.getLong("i");
		    	
	            IDBResult res = getDB().executeSQL("SELECT source_id, object FROM object_values where id=?", id );
	            if ( !res.isEnd() )
	            {
	            	LOG.TRACE("Delete: " + id);
	                Integer nDelSrcID = new Integer(res.getIntByIdx(0));
	                String strDelObject = res.getStringByIdx(1);
	                getDB().executeSQL("DELETE FROM object_values where id=?", id );
	                getNotify().onObjectChanged(nDelSrcID, strDelObject, SyncNotify.enDelete);
	            }
                // oo conflicts
                getDB().executeSQL("UPDATE changed_values SET sent=3 where main_id=?", id );
                //
	            
		        m_nDeleted++;
		    }
		}
		
		return true;
	}
	
	void processServerData_Ver1(JSONArrayIterator oJsonArr)throws Exception
	{
	    for( ; !oJsonArr.isEnd() && getSync().isContinueSync(); oJsonArr.next() )
	    {
	        JSONEntry oJsonSource = oJsonArr.getCurItem();
	        String strSrcName = oJsonSource.getString("s");
	        Integer nSrcID = getID();
	        if ( strSrcName.compareTo("RhoDeleteSource") == 0 )
	            nSrcID = new Integer(-1);
	        else if ( strSrcName.compareTo(getName()) != 0 )
	        {
	            SyncSource pSrc = getSync().findSourceByName(strSrcName);
	            if ( pSrc == null )
	            {
	                LOG.ERROR("Sync server send data for unknown source name:" + strSrcName);
	                getSync().stopSync();
	                m_nErrCode = RhoRuby.ERR_UNEXPECTEDSERVERRESPONSE;
	                break;
	            }
	            nSrcID = pSrc.getID();
	        }

	        JSONArrayIterator oJsonObjList = new JSONArrayIterator(oJsonSource, "ol");
	        m_nAttribCounter = 0;
	        for( ; !oJsonObjList.isEnd() && getSync().isContinueSync(); oJsonObjList.next() )
	        {
		        if ( getDB().isUnlockDB() )//|| m_nAttribCounter >= 200 )
		        {
		        	if ( getDB().isUnlockDB() )
		        		LOG.INFO( "Commit transaction because of UI request." );
		        	
		            getDB().endTransaction();
		            getDB().startTransaction();
		            
		            m_nAttribCounter = 0;
		        }
		
		        JSONEntry oJsonObject = oJsonObjList.getCurItem();
		        if( !processSyncObject_ver1(oJsonObject,nSrcID))
	            {
		            getSync().stopSync();
		            break;
	            }
		        
		        if ( !isDeleteObjectsPass() && nSrcID.intValue() >=0 )
		        {
	                int nSyncObjectCount  = getNotify().incLastSyncObjectCount(nSrcID);

	                if ( getProgressStep() > 0 && (nSyncObjectCount%getProgressStep() == 0) )
	                    getNotify().fireSyncNotification(this, false, RhoRuby.ERR_NONE, "");
		        }
		        
		        m_bGetAtLeastOnePage = true;
	        }
	    }
	}
	
	class CValue
	{
	    String m_strValue;
		String m_strAttrType;
		Long   m_nID;
		
		CValue(JSONEntry oJsonEntry)throws JSONException
		{
			m_strValue = oJsonEntry.getString("value");
			m_strAttrType = oJsonEntry.getString("attrib_type");
			m_nID = new Long(oJsonEntry.getLong("id"));
		}
		CValue(JSONEntry oJsonEntry, int nVer)throws JSONException
		{
			if ( nVer == 1 )
			{
				m_strValue = oJsonEntry.getString("v");
				m_strAttrType = oJsonEntry.getString("t");
				m_nID = new Long(oJsonEntry.getLong("i"));
			}
		}
		
	}
	
	private String makeFileName(CValue value)throws Exception
	{
		String fName = DBAdapter.makeBlobFolderName();
		
		String strExt = ".bin";
		URI uri = new URI(value.m_strValue);
		int nDot = uri.getPath().lastIndexOf('.');
		if ( nDot >= 0 )
			strExt = uri.getPath().substring(nDot);
		else{
			int nExt = uri.getQueryString().indexOf("extension=");
			if ( nExt >= 0 ){
				int nExtEnd = uri.getQueryString().indexOf("&", nExt);
				if (nExtEnd < 0 )
					nExtEnd = uri.getQueryString().length();
				
				strExt = uri.getQueryString().substring(nExt+10, nExtEnd);
			}
		}
		
		//TODO: add join method to Rho::FilePath
		fName += "id_" + value.m_nID.toString() + strExt;
		
		return fName;
	}
	
	boolean downloadBlob(CValue value)throws Exception
	{
		if ( value.m_strAttrType == null || !value.m_strAttrType.equals("blob.url")  )
			return true;
		
		String fName = makeFileName( value );
		String url = value.m_strValue;
		int nQuest = url.lastIndexOf('?');
		if ( nQuest > 0 )
			url += "&";
		else
			url += "?";
		url += "client_id=" + getSync().getClientID();
		
		try{
			NetResponse resp = getNet().pullFile(url, fName, getSync(), null);
	        if ( !resp.isOK() )
	        {
	        	m_nErrCode = RhoRuby.ERR_REMOTESERVER;
	        	//m_strError = resp.getCharData();
	        	return false;
	        }
	    }catch(Exception exc)
	    {
	    	m_nErrCode = RhoRuby.getNetErrorCode(exc);
	    	throw exc;
	    }
        
        value.m_strAttrType = "blob.file";
        
    	String root = RhoClassFactory.createFile().getDirPath("");
        
        value.m_strValue = "/" + fName.substring(root.length());
        
        return true;
	}
	
	void processToken(long token)throws DBException
	{
		if ( token > 1 && getToken() == token ){
			//Delete non-confirmed records
	    	
	        setToken( token ); //For m_bTokenFromDB = false;
	        //getDB().executeSQL("DELETE FROM object_values where source_id=? and token=?", getID(), token );
	        //TODO: add special table for id,token
		}else
	    {
	        setToken( token );
	        getDB().executeSQL("UPDATE sources SET token=? where source_id=?", new Long(token), getID() );
		}
	
	}
}

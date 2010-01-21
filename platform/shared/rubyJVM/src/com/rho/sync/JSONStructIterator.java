package com.rho.sync;

import org.json.me.JSONObject;
class JSONStructIterator
{
	//TODO: JSONStructIterator
    JSONStructIterator(String szData)
    {
/*        m_struct = 0;
        m_curEntry = 0;
        m_rootObject = json_tokener_parse(const_cast<char*>(szData));

	    if ( !m_rootObject || is_error(m_rootObject) ) 
            m_rootObject = 0;
        else
        {
            m_struct = json_object_get_object((struct json_object *)m_rootObject);
            m_curEntry = m_struct->head;
        }*/
    }

    JSONStructIterator(JSONEntry oEntry, String strName)
    {
/*        m_struct = 0;
        m_curEntry = 0;
        m_rootObject = 0;

        CJSONEntry oItem = oEntry.getEntry(strName);
        if ( !oItem.isEmpty() )
        {
            m_struct = json_object_get_object( oItem.getObject() );
            m_curEntry = m_struct->head;
        }
*/        
    }

    JSONStructIterator(JSONEntry oEntry)
    {
/*        m_rootObject = 0;

        m_struct = json_object_get_object( oEntry.getObject() );
        m_curEntry = m_struct->head;
*/        
    }

    boolean isEnd()
    {
        return true;//m_curEntry == 0;
    }

    void  next()
    {
        //m_curEntry = m_curEntry->next;
    }

    void reset()
    {
        //m_curEntry = m_struct->head;
    }

    String getCurKey()
    {
        return isEnd() ? new String() : "";//String((char*)m_curEntry->k);
    }

    JSONEntry getCurValue()
    {
        return new JSONEntry( (JSONObject)null ); 
        		//isEnd() ? null :
        	    //( struct json_object *) m_curEntry->v );
    }

}

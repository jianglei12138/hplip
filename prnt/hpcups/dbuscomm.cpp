/*****************************************************************************\
  dbuscomm.cpp : Interface for DBusCommunicator class

  Copyright (c) 1996 - 2011, HP Co.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  Author: Amarnath Chitumalla
\*****************************************************************************/

#include "dbuscomm.h"

DBusCommunicator::DBusCommunicator()
{
	m_strPrinterURI = "";
	m_strPrinterName = "";
#ifdef HAVE_DBUS
	m_DbusConnPtr = NULL; 
#endif
}
/*
*
*
*/


bool DBusCommunicator::initDBusComm(string strDbusPath/*=""*/,string strInterfaceName/*=""*/,  
				string strPrinterURI/*=""*/, string strPrinterName/*= ""*/, int iJobId/*=0*/, string strUser/*=""*/)
{
#ifdef HAVE_DBUS
	DBusError objError;
	DBusError * pDBusError=&objError;
		
	m_strPrinterURI = strPrinterURI;
	m_strPrinterName = strPrinterName;
	m_strDbusInterface = strInterfaceName;
	m_strDbusPath = strDbusPath;
	m_strUser = strUser;
	m_iJobId = iJobId;
		
	dbus_error_init(pDBusError);
	m_DbusConnPtr = dbus_bus_get(DBUS_BUS_SYSTEM, pDBusError);
	if(dbus_error_is_set(pDBusError))
	{
		dbglog("Error: dBus Connection Error (%s)!\n", pDBusError->message);
		dbus_error_free(pDBusError);
	}
	
	if(m_DbusConnPtr == NULL)
	{
		dbglog("Error: dBus Connection Error (%s)!\n", pDBusError->message);
		return false;
	}
#endif	
	return true;
}

bool DBusCommunicator::sendEvent(string strDbusPath,string strInterfaceName, string strDeviceURI, string strPrinterName, int iEvent, 
			string strTitle/*=""*/, int iJobId/*=0*/, string strUser/*=""*/)
{
#ifdef HAVE_DBUS

	if(NULL == m_DbusConnPtr )
	{
		dbglog("Error: dBus connection ptr is NULL.\n");
		return false;
	}
	if(true == strDbusPath.empty() || true == strInterfaceName.empty() || 0 == iEvent)
	{
		dbglog("Error: dBus service Name can't be empty. DBus Path(%s) DBus Interface (%s) Event(%d)!\n", 
				strDbusPath.c_str(), strInterfaceName.c_str(), iEvent);
		return false;
	}
	DBusMessage * msg = dbus_message_new_signal(strDbusPath.c_str(), strInterfaceName.c_str(), "Event");
	if (NULL == msg)
	{
		dbglog("Error: dBus dbus_message_new_signal returned error. DBus Interface (%s) Event(%d)!\n", strInterfaceName.c_str(), iEvent);
		return false;
	}
	
	if (NULL == msg)
	{
		dbglog("dbus message is NULL!\n");
		return false;
	}
	const char * stURI=strDeviceURI.c_str();
	const char * stPRNTNM=strPrinterName.c_str();
	const char * stUSR=strUser.c_str();
	const char * stTtl=strTitle.c_str();
 
	dbus_message_append_args(msg, 
		DBUS_TYPE_STRING, &stURI,
		DBUS_TYPE_STRING, &stPRNTNM,
		DBUS_TYPE_UINT32, &iEvent, 
		DBUS_TYPE_STRING, &stUSR, 
		DBUS_TYPE_UINT32, &iJobId,
		DBUS_TYPE_STRING, &stTtl, 
		DBUS_TYPE_INVALID);

	if (!dbus_connection_send(m_DbusConnPtr , msg, NULL))
	{
		dbglog("dbus message send failed!\n");
		return false;
	}

	dbus_connection_flush(m_DbusConnPtr );
	dbus_message_unref(msg);

#endif
	return true;

}

bool DBusCommunicator::sendEvent(int iEvent, string strTitle/*=""*/, int iJobId/*=0*/,string strUser/*=""*/)
{
	return sendEvent(m_strDbusPath, m_strDbusInterface, m_strPrinterURI, m_strPrinterName, iEvent, strTitle, iJobId,strUser);
}

DBusCommunicator::~DBusCommunicator()
{

}

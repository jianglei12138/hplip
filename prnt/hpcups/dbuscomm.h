/*****************************************************************************\
  dbuscomm.h : Interface for DBusCommunicator class

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
\*****************************************************************************/

#ifndef __DBUSCOMM_H__
#define __DBUSCOMM_H__

# include <string>
#include "CommonDefinitions.h"
#include "hpmud.h"
#include <pwd.h>

#ifdef HAVE_DBUS
	#include <dbus/dbus.h>
	#define DBUS_INTERFACE "com.hplip.StatusService"
	#define DBUS_PATH "/"
#else
#define DBUS_INTERFACE ""
#define DBUS_PATH ""
#endif


using namespace std;

/*			//DBusBusType
	DBUS_BUS_SESSION		//The login session bus. 
	DBUS_BUS_SYSTEM		//The systemwide bus. 
	DBUS_BUS_STARTER		//The bus that started us, if any. 
*/

class DBusCommunicator
{
	private:
		string m_strPrinterURI;
		string m_strPrinterName;
#ifdef HAVE_DBUS
		DBusConnection *m_DbusConnPtr;
#endif
		string m_strDbusInterface;
		string m_strDbusPath;
		string m_strUser;
		int m_iJobId;
		
		
	public:
		DBusCommunicator();
		bool initDBusComm(string strDbusPath="",string strInterfaceName="",  string strPrinterURI="", string strPrinterName = "",int iJobId=0, string strUser="");
		bool sendEvent(int iEvent, string strTitle="", int iJobId=0, string strUser="");
		bool sendEvent(string strDbusPath,string strInterfaceName, string strPrinterURI, string strPrinterName, int event, string strTitle="",int iJobId=0, string strUser="");
		~DBusCommunicator();
};


#endif	// __DBUSCOMM_H__


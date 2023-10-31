#include <Arduino.h>
#include "settings.h"

#include "Ftp.h"

#include "Log.h"
#include "MemX.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"

#include <WiFi.h>

#ifdef FTP_ENABLE
	#include "ESP-FTP-Server-Lib.h"
#endif

// FTP
String Ftp_User = "esp32"; // FTP-user (default; can be changed later via GUI)
String Ftp_Password = "esp32"; // FTP-password (default; can be changed later via GUI)

// FTP
#ifdef FTP_ENABLE
FTPServer *ftpSrv; // Heap-alloction takes place later (when needed)
bool ftpEnableLastStatus = false;
bool ftpEnableCurrentStatus = false;
#endif

void ftpManager(void);

void Ftp_Init(void) {
	String info;
	// Get FTP-user from NVS
	String nvsFtpUser = gPrefsSettings.getString("ftpuser", "-1");
	if (!nvsFtpUser.compareTo("-1")) {
		gPrefsSettings.putString("ftpuser", (String) Ftp_User);
		Log_Println(wroteFtpUserToNvs, LOGLEVEL_ERROR);
	} else {
		Ftp_User = nvsFtpUser;
		Log_Printf(LOGLEVEL_INFO, restoredFtpUserFromNvs, nvsFtpUser.c_str());
	}

	// Get FTP-password from NVS
	String nvsFtpPassword = gPrefsSettings.getString("ftppassword", "-1");
	if (!nvsFtpPassword.compareTo("-1")) {
		gPrefsSettings.putString("ftppassword", (String) Ftp_Password);
		Log_Println(wroteFtpPwdToNvs, LOGLEVEL_ERROR);
	} else {
		Ftp_Password = nvsFtpPassword;
		Log_Printf(LOGLEVEL_INFO, restoredFtpPwdFromNvs, nvsFtpPassword.c_str());
	}
}

void Ftp_Cyclic(void) {
#ifdef FTP_ENABLE
	ftpManager();

	if (WL_CONNECTED == WiFi.status()) {
		if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
			ftpSrv->handle();
		}
	}

	if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
		if (ftpSrv->countConnections() > 0) {
			System_UpdateActivityTimer(); // Re-adjust timer while client is connected to avoid ESP falling asleep
		}
	}
#endif
}

void Ftp_EnableServer(void) {
#ifdef FTP_ENABLE
	if (Wlan_IsConnected() && !ftpEnableLastStatus && !ftpEnableCurrentStatus) {
		ftpEnableLastStatus = true;
#else
	if (Wlan_IsConnected()) {
#endif

		System_IndicateOk();
	} else {
		Log_Println(unableToStartFtpServer, LOGLEVEL_ERROR);
		System_IndicateError();
	}
}

// Creates FTP-instance only when requested
void ftpManager(void) {
#ifdef FTP_ENABLE
	if (ftpEnableLastStatus && !ftpEnableCurrentStatus) {
		Log_Printf(LOGLEVEL_DEBUG, freeHeapWithoutFtp, ESP.getFreeHeap());
		ftpEnableCurrentStatus = true;
		ftpSrv = new FTPServer();
		ftpSrv->addUser(Ftp_User, Ftp_Password);
		ftpSrv->addFilesystem("SD-Card", &gFSystem);
		ftpSrv->begin();
		Log_Printf(LOGLEVEL_DEBUG, freeHeapWithFtp, ESP.getFreeHeap());
		Log_Println(ftpServerStarted, LOGLEVEL_NOTICE);
	}
#endif
}

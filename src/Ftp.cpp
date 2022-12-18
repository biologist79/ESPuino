#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "Ftp.h"
#include "Log.h"
#include "MemX.h"
#include "SdCard.h"
#include "System.h"
#include "Wlan.h"

#ifdef FTP_ENABLE
	#include "ESP32FtpServer.h"
#endif

// FTP
String Ftp_User = "esp32";      // FTP-user (default; can be changed later via GUI)
String Ftp_Password = "esp32";  // FTP-password (default; can be changed later via GUI)

// FTP
#ifdef FTP_ENABLE
FtpServer *ftpSrv; // Heap-alloction takes place later (when needed)
bool ftpEnableLastStatus = false;
bool ftpEnableCurrentStatus = false;
#endif

void ftpManager(void);

void Ftp_Init(void) {
	String info;
	// Get FTP-user from NVS
	String nvsFtpUser = gPrefsSettings.getString("ftpuser", "-1");
	if (!nvsFtpUser.compareTo("-1")) {
		gPrefsSettings.putString("ftpuser", (String)Ftp_User);
		Log_Println((char *) FPSTR(wroteFtpUserToNvs), LOGLEVEL_ERROR);
	} else {
		Ftp_User = nvsFtpUser;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredFtpUserFromNvs), nvsFtpUser.c_str());
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	}

	// Get FTP-password from NVS
	String nvsFtpPassword = gPrefsSettings.getString("ftppassword", "-1");
	if (!nvsFtpPassword.compareTo("-1")) {
		gPrefsSettings.putString("ftppassword", (String)Ftp_Password);
		Log_Println((char *) FPSTR(wroteFtpPwdToNvs), LOGLEVEL_ERROR);
	} else {
		Ftp_Password = nvsFtpPassword;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredFtpPwdFromNvs), nvsFtpPassword.c_str());
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	}
}

void Ftp_Cyclic(void) {
	#ifdef FTP_ENABLE
		ftpManager();

		if (WL_CONNECTED == WiFi.status()) {
			if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
				ftpSrv->handleFTP();
			}
		}

		if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
			if (ftpSrv->isConnected()) {
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
		Log_Println((char *) FPSTR(unableToStartFtpServer), LOGLEVEL_ERROR);
		System_IndicateError();
	}
}

// Creates FTP-instance only when requested
void ftpManager(void) {
	#ifdef FTP_ENABLE
		if (ftpEnableLastStatus && !ftpEnableCurrentStatus) {
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeHeapWithoutFtp), ESP.getFreeHeap());
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
			ftpEnableCurrentStatus = true;
			ftpSrv = new FtpServer();
			ftpSrv->begin(gFSystem, Ftp_User, Ftp_Password);
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeHeapWithFtp), ESP.getFreeHeap());
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		#if (LANGUAGE == DE)
				Log_Println((char *) F("FTP-Server gestartet"), LOGLEVEL_NOTICE);
		#else
				Log_Println((char *) F("FTP-Server started"), LOGLEVEL_NOTICE);
		#endif
		}
	#endif
}

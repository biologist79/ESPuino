#pragma once

constexpr uint8_t ftpUserLength = 10u; // Length will be published n-1 as maxlength to GUI
constexpr uint8_t ftpPasswordLength = 15u; // Length will be published n-1 as maxlength to GUI

void Ftp_Init(void);
void Ftp_Cyclic(void);
void Ftp_EnableServer(void);

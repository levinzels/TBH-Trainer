#pragma once
#include <string>

namespace ka {

bool Init();
bool License(const std::string& key);
bool Check();

void StartHeartbeat();
void StopHeartbeat();

void StartProtection();

// Fetch a global app variable (server-side secret) by id. Verified by Ed25519.
bool FetchVar(const std::string& varid, std::string& out);

bool   Authed();
bool   Guard();
bool   Tampered();
const  std::string& Username();
const  std::string& Expiry();
const  std::string& LastError();
const  std::string& HWID();

bool   SaveSession(const std::string& key);
bool   LoadSession(std::string& outKey);
void   ClearSession();

}

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

#include "keyauth.h"
#include "xorstr.h"

extern "C" {
#include "tweetnacl.h"
}

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// TweetNaCl references randombytes() (keygen paths). We only verify signatures,
// but the symbol must resolve at link time.
extern "C" void randombytes(unsigned char* p, unsigned long long n) {
    if (BCryptGenRandom(nullptr, p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (unsigned long long i = 0; i < n; ++i) p[i] = (unsigned char)(rand() & 0xFF);
    }
}

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

namespace {

constexpr const char* kAppName    = "TBH";
constexpr const char* kOwnerID    = "HwTZG7oy8P";
constexpr const char* kAppVersion = "1.0";

constexpr const unsigned char kSecEnc[] = {
    0xc6, 0x84, 0xdc, 0xfa, 0xeb, 0xe7, 0xd8, 0x96, 0xcb, 0x68, 0x2c, 0x11,
    0x4a, 0x57, 0x22, 0x2a, 0x66, 0x54, 0x09, 0x1b, 0xe1, 0xeb, 0xad, 0xc0,
    0xcb, 0x8c, 0xf4, 0xe8, 0xe8, 0x84, 0x92, 0x9d, 0x64, 0x27, 0x2e, 0x1b,
    0x51, 0x08, 0x2c, 0x37, 0x3d, 0x58, 0x4d, 0xb2, 0xe9, 0xa3, 0x99, 0x9e,
    0x81, 0xf1, 0xa8, 0xeb, 0x81, 0x8a, 0xcc, 0x60, 0x6d, 0x2a, 0x12, 0x07,
    0x5d, 0x72, 0x30, 0x3a
};
constexpr unsigned char kSecKey = 0xA7;

std::string DecodeSecret() {
    std::string s;
    s.reserve(sizeof(kSecEnc));
    for (size_t i = 0; i < sizeof(kSecEnc); ++i)
        s.push_back((char)((unsigned char)kSecEnc[i] ^ (unsigned char)(kSecKey + i * 11)));
    return s;
}

std::string g_session;
std::string g_username;
std::string g_expiry;
std::string g_lastErr;
std::string g_hwid;
std::string g_savedKey;
std::atomic<bool> g_authed{false};
std::atomic<bool> g_tamper{false};
std::atomic<bool> g_heartbeatRun{false};
std::atomic<bool> g_protectRun{false};
std::thread g_heartbeatThread;
std::thread g_protectThread;

bool Sha256Raw(const unsigned char* data, size_t len, unsigned char out[32]) {
    BCRYPT_ALG_HANDLE hAlg = nullptr; BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != STATUS_SUCCESS)
        return false;
    DWORD objLen = 0, cb = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
    std::vector<unsigned char> obj(objLen);
    if (BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0) == STATUS_SUCCESS) {
        if (BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0) == STATUS_SUCCESS &&
            BCryptFinishHash(hHash, out, 32, 0) == STATUS_SUCCESS)
            ok = true;
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

bool HmacSha256Raw(const std::string& key, const std::string& data, unsigned char out[32]) {
    BCRYPT_ALG_HANDLE hAlg = nullptr; BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG) != STATUS_SUCCESS)
        return false;
    DWORD objLen = 0, cb = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
    std::vector<unsigned char> obj(objLen);
    if (BCryptCreateHash(hAlg, &hHash, obj.data(), objLen,
            (PUCHAR)key.data(), (ULONG)key.size(), 0) == STATUS_SUCCESS) {
        if (BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0) == STATUS_SUCCESS &&
            BCryptFinishHash(hHash, out, 32, 0) == STATUS_SUCCESS)
            ok = true;
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

std::string ToHex(const unsigned char* p, size_t n) {
    static const char hx[] = "0123456789abcdef";
    std::string s; s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { s.push_back(hx[p[i] >> 4]); s.push_back(hx[p[i] & 0xF]); }
    return s;
}

// Constant-time comparison of two equal-length hex strings.
bool CtEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = (unsigned char)a[i]; if (ca >= 'A' && ca <= 'F') ca += 32;
        unsigned char cb = (unsigned char)b[i]; if (cb >= 'A' && cb <= 'F') cb += 32;
        diff |= (unsigned char)(ca ^ cb);
    }
    return diff == 0;
}

bool HexToBytes(const std::string& hex, unsigned char* out, size_t outLen) {
    if (hex.size() != outLen * 2) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < outLen; ++i) {
        int hi = nib(hex[2 * i]), lo = nib(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return true;
}

// KeyAuth signs every API response with Ed25519 over "<timestamp><body>".
// The public key is the server's fixed signing key. Verifying this defeats
// fake/proxy servers that try to return a forged "success" reply.
constexpr const char* kApiPubKey =
    "5586b4bc69c7a4b487e4563a4cd96afd39140f919bd31cea7d1c6a1e8439422b";

bool VerifyResponse(const std::string& body, const std::string& sigHex, const std::string& ts) {
    if (sigHex.size() != 128 || ts.empty()) return false;

    unsigned char sig[64], pk[32];
    if (!HexToBytes(sigHex, sig, 64)) return false;
    if (!HexToBytes(kApiPubKey, pk, 32)) return false;

    // Reject responses with a wildly skewed timestamp (anti-replay, lenient
    // window so NTP-accurate clocks never fail).
    long long now = (long long)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long t = _atoi64(ts.c_str());
    long long d = now - t; if (d < 0) d = -d;
    if (t <= 0 || d > 3600) return false;

    std::string signedMsg = ts + body;
    std::vector<unsigned char> sm(64 + signedMsg.size());
    memcpy(sm.data(), sig, 64);
    memcpy(sm.data() + 64, signedMsg.data(), signedMsg.size());
    std::vector<unsigned char> m(sm.size());
    unsigned long long mlen = 0;
    return crypto_sign_open(m.data(), &mlen, sm.data(), sm.size(), pk) == 0;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::string UrlEncode(const std::string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

std::string QueryHeader(HINTERNET hReq, const wchar_t* name) {
    DWORD len = 0;
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CUSTOM, name,
        WINHTTP_NO_OUTPUT_BUFFER, &len, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0) return {};
    std::wstring w(len / sizeof(wchar_t), 0);
    if (!WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CUSTOM, name,
            &w[0], &len, WINHTTP_NO_HEADER_INDEX))
        return {};
    while (!w.empty() && w.back() == L'\0') w.pop_back();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}

bool HttpPost(const std::string& body, std::string& outBody,
              std::string& outSig, std::string& outTs) {
    bool ok = false;
    HINTERNET hSession = WinHttpOpen(L"KeyAuth",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { g_lastErr = "winhttp open failed"; return false; }

    HINTERNET hConnect = WinHttpConnect(hSession, L"keyauth.win",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); g_lastErr = "winhttp connect failed"; return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/1.3/",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        g_lastErr = "winhttp open request failed";
        return false;
    }

    const wchar_t* hdr = L"Content-Type: application/x-www-form-urlencoded\r\nUser-Agent: KeyAuth\r\n";
    if (WinHttpSendRequest(hRequest, hdr, (DWORD)-1L,
            (LPVOID)body.data(), (DWORD)body.size(),
            (DWORD)body.size(), 0) &&
        WinHttpReceiveResponse(hRequest, nullptr))
    {
        outBody.clear();
        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
            if (!avail) break;
            std::vector<char> buf(avail + 1, 0);
            DWORD rd = 0;
            if (!WinHttpReadData(hRequest, buf.data(), avail, &rd)) break;
            outBody.append(buf.data(), rd);
        } while (avail > 0);
        ok = !outBody.empty();
        if (!ok) g_lastErr = "empty response";
        outSig = QueryHeader(hRequest, L"x-signature-ed25519");
        outTs  = QueryHeader(hRequest, L"x-signature-timestamp");
    } else {
        g_lastErr = "winhttp send/recv failed";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

std::string JsonField(const std::string& j, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = j.find(pat);
    if (p == std::string::npos) return {};
    p = j.find(':', p + pat.size());
    if (p == std::string::npos) return {};
    p++;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) p++;
    if (p >= j.size()) return {};
    if (j[p] == '"') {
        size_t e = p + 1;
        std::string out;
        while (e < j.size() && j[e] != '"') {
            if (j[e] == '\\' && e + 1 < j.size()) { out.push_back(j[e + 1]); e += 2; }
            else out.push_back(j[e++]);
        }
        return out;
    }
    size_t e = p;
    while (e < j.size() && j[e] != ',' && j[e] != '}' && j[e] != '\r' && j[e] != '\n')
        e++;
    return j.substr(p, e - p);
}

bool JsonBool(const std::string& j, const std::string& key) {
    std::string v = JsonField(j, key);
    return v == "true" || v == "True" || v == "TRUE" || v == "1";
}

std::string ReadRegStr(HKEY root, const wchar_t* sub, const wchar_t* val) {
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS)
        return {};
    wchar_t buf[256];
    DWORD sz = sizeof(buf);
    DWORD type = 0;
    LSTATUS r = RegQueryValueExW(k, val, nullptr, &type, (LPBYTE)buf, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::string ComputeHWID() {
    std::string mg = ReadRegStr(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");
    if (mg.empty()) {
        wchar_t name[256]; DWORD ln = 256;
        if (GetComputerNameW(name, &ln)) {
            int n = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            mg.assign(n - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, name, -1, mg.data(), n, nullptr, nullptr);
        } else {
            mg = "unknown-machine";
        }
    }
    return mg;
}

}

namespace ka {

bool Init() {
    if (g_hwid.empty()) g_hwid = ComputeHWID();

    std::string body = "type=init";
    body += "&ver=";  body += UrlEncode(kAppVersion);
    body += "&hash="; body += UrlEncode(DecodeSecret());
    body += "&name="; body += UrlEncode(kAppName);
    body += "&ownerid="; body += UrlEncode(kOwnerID);

    std::string resp, sig, ts;
    if (!HttpPost(body, resp, sig, ts)) return false;

    if (!VerifyResponse(resp, sig, ts)) {
        g_lastErr = "response signature mismatch (tamper/proxy)";
        g_tamper = true;
        g_authed = false;
        return false;
    }

    if (!JsonBool(resp, "success")) {
        std::string m = JsonField(resp, "message");
        g_lastErr = m.empty() ? "init failed" : m;
        return false;
    }

    g_session = JsonField(resp, "sessionid");
    return !g_session.empty();
}

bool License(const std::string& key) {
    if (g_session.empty()) { g_lastErr = "no session"; return false; }
    if (g_hwid.empty()) g_hwid = ComputeHWID();

    std::string body = "type=license";
    body += "&key=";  body += UrlEncode(key);
    body += "&hwid="; body += UrlEncode(g_hwid);
    body += "&sessionid="; body += UrlEncode(g_session);
    body += "&name="; body += UrlEncode(kAppName);
    body += "&ownerid="; body += UrlEncode(kOwnerID);

    std::string resp, sig, ts;
    if (!HttpPost(body, resp, sig, ts)) return false;

    if (!VerifyResponse(resp, sig, ts)) {
        g_lastErr = "response signature mismatch (tamper/proxy)";
        g_tamper = true;
        g_authed = false;
        return false;
    }

    if (!JsonBool(resp, "success")) {
        std::string m = JsonField(resp, "message");
        g_lastErr = m.empty() ? "license failed" : m;
        return false;
    }

    g_username = JsonField(resp, "username");
    if (g_username.empty()) g_username = "user";
    g_expiry   = JsonField(resp, "expiry");
    g_savedKey = key;
    g_authed   = !g_tamper.load();
    return g_authed.load();
}

bool Check() {
    if (g_session.empty()) { g_lastErr = "no session"; return false; }

    std::string body = "type=check";
    body += "&sessionid="; body += UrlEncode(g_session);
    body += "&name="; body += UrlEncode(kAppName);
    body += "&ownerid="; body += UrlEncode(kOwnerID);

    std::string resp, sig, ts;
    if (!HttpPost(body, resp, sig, ts)) return false;

    if (!VerifyResponse(resp, sig, ts)) {
        g_lastErr = "response signature mismatch (tamper/proxy)";
        g_tamper = true;
        g_authed = false;
        return false;
    }

    if (!JsonBool(resp, "success")) {
        std::string m = JsonField(resp, "message");
        g_lastErr = m.empty() ? "check failed" : m;
        g_authed = false;
        return false;
    }
    return true;
}

bool FetchVar(const std::string& varid, std::string& out) {
    if (g_session.empty()) { g_lastErr = "no session"; return false; }

    std::string body = "type=var";
    body += "&varid="; body += UrlEncode(varid);
    body += "&sessionid="; body += UrlEncode(g_session);
    body += "&name="; body += UrlEncode(kAppName);
    body += "&ownerid="; body += UrlEncode(kOwnerID);

    std::string resp, sig, ts;
    if (!HttpPost(body, resp, sig, ts)) return false;

    if (!VerifyResponse(resp, sig, ts)) {
        g_lastErr = "response signature mismatch (tamper/proxy)";
        g_tamper = true;
        g_authed = false;
        return false;
    }

    if (!JsonBool(resp, "success")) {
        std::string m = JsonField(resp, "message");
        g_lastErr = m.empty() ? "var failed" : m;
        return false;
    }

    out = JsonField(resp, "message");
    return !out.empty();
}

void StartHeartbeat() {
    if (g_heartbeatRun.load()) return;
    g_heartbeatRun = true;
    g_heartbeatThread = std::thread([] {
        int failStreak = 0;
        const int kIntervalSec = 15;
        const int kMaxFail = 2;
        while (g_heartbeatRun.load()) {
            for (int i = 0; i < kIntervalSec && g_heartbeatRun.load(); ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!g_heartbeatRun.load()) break;
            if (!Check()) {
                if (++failStreak >= kMaxFail) {
                    g_authed = false;
                    ClearSession();
                    ExitProcess(0);
                }
            } else {
                failStreak = 0;
            }
        }
    });
}

void StopHeartbeat() {
    g_heartbeatRun = false;
    if (g_heartbeatThread.joinable()) g_heartbeatThread.detach();
}

}

namespace {

bool DebuggerAttached() {
    if (IsDebuggerPresent()) return true;
    BOOL remote = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote) && remote) return true;
    return false;
}

// Detect a hosts-file override that redirects the KeyAuth API host to a
// rogue local/proxy server (the classic "fake success server" attack).
bool HostsRedirected() {
    wchar_t sysdir[MAX_PATH];
    if (!GetSystemDirectoryW(sysdir, MAX_PATH)) return false;
    std::wstring path = std::wstring(sysdir) + L"\\drivers\\etc\\hosts";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
    std::string data;
    char buf[1024]; size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, r);
    fclose(f);
    for (auto& c : data) c = (char)tolower((unsigned char)c);
    size_t pos = 0;
    while ((pos = data.find("keyauth", pos)) != std::string::npos) {
        size_t ls = data.rfind('\n', pos);
        ls = (ls == std::string::npos) ? 0 : ls + 1;
        while (ls < pos && (data[ls] == ' ' || data[ls] == '\t')) ls++;
        if (ls < data.size() && data[ls] != '#') return true;
        pos += 7;
    }
    return false;
}

// Detect a software breakpoint (0xCC) planted at the entry of the auth funcs.
bool EntryBreakpoint() {
    const unsigned char* targets[] = {
        reinterpret_cast<const unsigned char*>(&ka::License),
        reinterpret_cast<const unsigned char*>(&ka::Check),
        reinterpret_cast<const unsigned char*>(&ka::Init)
    };
    for (auto p : targets) {
        if (!p) continue;
        for (int i = 0; i < 6; ++i) if (p[i] == 0xCC) return true;
    }
    return false;
}

}

namespace ka {

void StartProtection() {
    if (g_protectRun.load()) return;
    g_protectRun = true;
    g_protectThread = std::thread([] {
        while (g_protectRun.load()) {
            if (DebuggerAttached() || HostsRedirected() || EntryBreakpoint()) {
                g_tamper = true;
                g_authed = false;
            }
            for (int i = 0; i < 4 && g_protectRun.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
    g_protectThread.detach();
}

// Auth removed: feature gates are always open in this build.
bool Authed()                    { return true; }
bool Guard()                     { return true; }
bool Tampered()                  { return false; }
const std::string& Username()    { return g_username; }
const std::string& Expiry()      { return g_expiry; }
const std::string& LastError()   { return g_lastErr; }
const std::string& HWID()        { return g_hwid; }

static const char* kSessionPath = "C:\\@vmp\\session.dat";
static const long long kSessionMaxAgeSec = 7LL * 24 * 3600;  // local re-login window

static long long UnixNow() {
    return (long long)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Build a SHA256-based keystream of n bytes from a seed (HWID + secret bound).
static void Keystream(const std::string& seed, std::vector<unsigned char>& ks, size_t n) {
    ks.resize(n);
    size_t off = 0; unsigned int ctr = 0;
    while (off < n) {
        std::string blk = seed;
        blk.push_back((char)(ctr & 0xFF));
        blk.push_back((char)((ctr >> 8) & 0xFF));
        unsigned char h[32];
        if (!Sha256Raw((const unsigned char*)blk.data(), blk.size(), h)) {
            for (int i = 0; i < 32; ++i) h[i] = (unsigned char)(0x5C + i * 7 + ctr);
        }
        size_t take = (n - off < 32) ? (n - off) : 32;
        memcpy(&ks[off], h, take);
        off += take; ctr++;
    }
}

bool SaveSession(const std::string& key) {
    CreateDirectoryA("C:\\@vmp", nullptr);
    if (g_hwid.empty()) g_hwid = ComputeHWID();

    std::string secret = DecodeSecret();
    char ts[32]; snprintf(ts, sizeof(ts), "%lld", UnixNow());
    std::string plain = g_hwid + "|" + key + "|" + ts;

    std::vector<unsigned char> ks;
    Keystream(g_hwid + "\x1f" + secret + "\x1fsess", ks, plain.size());
    std::vector<unsigned char> ct(plain.size());
    for (size_t i = 0; i < plain.size(); ++i)
        ct[i] = (unsigned char)plain[i] ^ ks[i];

    unsigned char mac[32];
    if (!HmacSha256Raw(secret, std::string((char*)ct.data(), ct.size()), mac)) return false;

    FILE* f = nullptr;
    if (fopen_s(&f, kSessionPath, "wb") != 0 || !f) return false;
    fwrite(ct.data(), 1, ct.size(), f);
    fwrite(mac, 1, 32, f);
    fclose(f);
    return true;
}

bool LoadSession(std::string& outKey) {
    FILE* f = nullptr;
    if (fopen_s(&f, kSessionPath, "rb") != 0 || !f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 32 || sz > 8192) { fclose(f); return false; }
    std::vector<unsigned char> raw(sz);
    size_t rd = fread(raw.data(), 1, sz, f);
    fclose(f);
    if ((long)rd != sz) return false;

    size_t ctLen = (size_t)sz - 32;
    std::string ct((char*)raw.data(), ctLen);
    std::string sigStored((char*)raw.data() + ctLen, 32);

    std::string secret = DecodeSecret();
    unsigned char mac[32];
    if (!HmacSha256Raw(secret, ct, mac)) return false;
    if (memcmp(mac, sigStored.data(), 32) != 0) { ClearSession(); return false; }

    if (g_hwid.empty()) g_hwid = ComputeHWID();
    std::vector<unsigned char> ks;
    Keystream(g_hwid + "\x1f" + secret + "\x1fsess", ks, ctLen);
    std::string plain(ctLen, 0);
    for (size_t i = 0; i < ctLen; ++i)
        plain[i] = (char)((unsigned char)ct[i] ^ ks[i]);

    size_t b1 = plain.find('|');
    if (b1 == std::string::npos) return false;
    size_t b2 = plain.find('|', b1 + 1);
    if (b2 == std::string::npos) return false;

    std::string storedHwid = plain.substr(0, b1);
    std::string storedKey  = plain.substr(b1 + 1, b2 - b1 - 1);
    long long savedTs = _atoi64(plain.c_str() + b2 + 1);

    if (storedHwid != g_hwid) { ClearSession(); return false; }
    if (savedTs <= 0 || UnixNow() - savedTs > kSessionMaxAgeSec) { ClearSession(); return false; }

    outKey = storedKey;
    return !outKey.empty();
}

void ClearSession() {
    DeleteFileA(kSessionPath);
}

}

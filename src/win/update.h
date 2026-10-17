#pragma once
#include "platform.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <winhttp.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <bcrypt.h>

#ifndef ATYPOI_UPDATE_FEED
#define ATYPOI_UPDATE_FEED L""
#endif
namespace atypoi::win::update {
struct Manifest {std::wstring version,url;std::string sha256;};
inline bool https(const std::wstring& url){return url.starts_with(L"https://") && url.size()<4096 && url.find_first_of(L"\r\n\t ")==std::wstring::npos;}
inline Manifest parse(std::string_view input){
    Manifest result;std::istringstream stream{std::string(input)};std::string line;bool seenVersion=false,seenUrl=false,seenHash=false;
    while(std::getline(stream,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
        auto equal=line.find('=');if(equal==std::string::npos)throw std::runtime_error("Invalid update manifest");
        auto key=line.substr(0,equal),value=line.substr(equal+1);
        if(key=="version"&&!seenVersion){result.version=fromUtf8(value);seenVersion=true;}
        else if(key=="url"&&!seenUrl){result.url=fromUtf8(value);seenUrl=true;}
        else if(key=="sha256"&&!seenHash){result.sha256=value;seenHash=true;}
        else throw std::runtime_error("Unknown or duplicate update field");
    }
    if(!seenVersion||!seenUrl||!seenHash||!https(result.url)||result.sha256.size()!=64||!std::all_of(result.sha256.begin(),result.sha256.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))throw std::runtime_error("Invalid update manifest fields");
    return result;
}
inline std::array<unsigned,3> versionParts(const std::wstring& text){
    std::array<unsigned,3> result{};std::wistringstream stream(text);wchar_t dot=0;
    if(!(stream>>result[0]>>dot)||dot!=L'.'||!(stream>>result[1]>>dot)||dot!=L'.'||!(stream>>result[2]))throw std::runtime_error("Invalid version");
    if(stream.peek()!=WEOF&&stream.peek()!=L'-')throw std::runtime_error("Invalid version suffix");return result;
}
struct Internet {HINTERNET handle=nullptr;explicit Internet(HINTERNET h):handle(h){}~Internet(){if(handle)WinHttpCloseHandle(handle);}Internet(const Internet&)=delete;};
inline std::string get(const std::wstring& url,size_t maxBytes,const std::atomic_bool& cancel){
    if(!https(url))throw std::runtime_error("HTTPS required");
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof parts;parts.dwHostNameLength=static_cast<DWORD>(-1);parts.dwUrlPathLength=static_cast<DWORD>(-1);parts.dwExtraInfoLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&parts))throw std::runtime_error("Invalid update URL");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    Internet session(WinHttpOpen(L"Atypoi/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));if(!session.handle)throw std::runtime_error("HTTP initialization failed");
    WinHttpSetTimeouts(session.handle,3000,3000,5000,5000);
    Internet connection(WinHttpConnect(session.handle,host.c_str(),parts.nPort,0));if(!connection.handle)throw std::runtime_error("Connection failed");
    Internet request(WinHttpOpenRequest(connection.handle,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));if(!request.handle)throw std::runtime_error("Request failed");
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof redirect);
    if(cancel||!WinHttpSendRequest(request.handle,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.handle,nullptr))throw std::runtime_error("Update request failed");
    DWORD status=0,length=sizeof status;if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&length,nullptr)||status!=200)throw std::runtime_error("Update server returned an error");
    std::string bytes;char buffer[16384];DWORD count=0;auto deadline=GetTickCount64()+60000;
    do {if(cancel||GetTickCount64()>deadline)throw std::runtime_error("Cancelled or download timeout");if(!WinHttpReadData(request.handle,buffer,sizeof buffer,&count))throw std::runtime_error("Download failed");if(bytes.size()+count>maxBytes)throw std::runtime_error("Update exceeds size limit");bytes.append(buffer,count);}while(count);
    return bytes;
}
inline std::string sha256(std::string_view bytes){
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD size=0,written=0;BYTE digest[32]{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Hash unavailable");
    BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<BYTE*>(&size),sizeof size,&written,0);std::vector<BYTE> object(size);
    auto status=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0);if(status>=0)status=BCryptHashData(hash,reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0);if(status>=0)status=BCryptFinishHash(hash,digest,sizeof digest,0);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);if(status<0)throw std::runtime_error("Hash failed");
    std::string text;for(auto b:digest){text+="0123456789abcdef"[b>>4];text+="0123456789abcdef"[b&15];}return text;
}
inline std::string signer(const std::wstring& path){
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;WINTRUST_FILE_INFO file{};file.cbStruct=sizeof file;file.pcwszFilePath=path.c_str();
    WINTRUST_DATA data{};data.cbStruct=sizeof data;data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_WHOLECHAIN;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    LONG valid=WinVerifyTrust(nullptr,&action,&data);std::string result;
    if(valid==ERROR_SUCCESS){auto provider=WTHelperProvDataFromStateData(data.hWVTStateData);auto signature=provider?WTHelperGetProvSignerFromChain(provider,0,FALSE,0):nullptr;if(signature&&signature->csCertChain){BYTE hash[32];DWORD length=sizeof hash;if(CertGetCertificateContextProperty(signature->pasCertChain[0].pCert,CERT_SHA256_HASH_PROP_ID,hash,&length)){for(DWORD i=0;i<length;++i){result+="0123456789abcdef"[hash[i]>>4];result+="0123456789abcdef"[hash[i]&15];}}}}
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);return result;
}
struct Result {std::wstring message,path,version;};
inline Result check(const std::atomic_bool& cancel){
    if(std::wstring(ATYPOI_UPDATE_FEED).empty())return {L"공개 업데이트 주소가 아직 설정되지 않았습니다. 새 개발용 설치 파일로 업데이트할 수 있습니다.",{},{}};
    auto ownSigner=signer(modulePath());if(ownSigner.empty())throw std::runtime_error("Installed application has no trusted publisher signature");
    auto manifest=parse(get(ATYPOI_UPDATE_FEED,16384,cancel));
    if(versionParts(manifest.version)<=versionParts(atypoi::win::version))return {L"현재 최신 버전을 사용하고 있습니다.",{},{}};
    auto bytes=get(manifest.url,64*1024*1024,cancel);if(sha256(bytes)!=manifest.sha256)throw std::runtime_error("Update SHA256 mismatch");
    wchar_t temp[32768];if(!GetTempPathW(32768,temp))throw std::runtime_error("Temporary folder unavailable");GUID unique;CoCreateGuid(&unique);
    auto path=std::filesystem::path(temp)/(L"Atypoi.Update."+guidString(unique)+L".exe");
    try{
        std::ofstream out(path,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();if(!out)throw std::runtime_error("Cannot save update");
        if(cancel||signer(path.wstring())!=ownSigner)throw std::runtime_error("Update publisher signature mismatch");
    }catch(...){std::error_code ignored;std::filesystem::remove(path,ignored);throw;}
    return {L"새 버전의 파일과 게시자 서명을 확인했습니다. 설치 프로그램을 열까요?",path.wstring(),manifest.version};
}
}

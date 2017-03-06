// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.
//
// This file is modified from Rescle written by yoshio.okumura@gmail.com:
// http://code.google.com/p/rescle/

#include "rescle.h"

#include <assert.h>
#include <atlstr.h>
#include <sstream> // wstringstream
#include <iomanip> // setw, setfill
#include <iostream>
#include <string>

namespace rescle {

namespace {

#pragma pack(push,2)
typedef struct _GRPICONENTRY {
  BYTE width;
  BYTE height;
  BYTE colourCount;
  BYTE reserved;
  BYTE planes;
  BYTE bitCount;
  WORD bytesInRes;
  WORD bytesInRes2;
  WORD reserved2;
  WORD id;
} GRPICONENTRY;
#pragma pack(pop)

#pragma pack(push,2)
typedef struct _GRPICONHEADER {
  WORD reserved;
  WORD type;
  WORD count;
  GRPICONENTRY entries[1];
} GRPICONHEADER;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_HEADER {
  WORD wLength;
  WORD wValueLength;
  WORD wType;
} VS_VERSION_HEADER;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_STRING {
  VS_VERSION_HEADER Header;
  WCHAR szKey[1];
} VS_VERSION_STRING;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_ROOT_INFO {
  WCHAR szKey[16];
  WORD  Padding1[1];
  VS_FIXEDFILEINFO Info;
} VS_VERSION_ROOT_INFO;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _VS_VERSION_ROOT {
  VS_VERSION_HEADER Header;
  VS_VERSION_ROOT_INFO Info;
} VS_VERSION_ROOT;
#pragma pack(pop)

unsigned int round(const unsigned int& value,
                     const unsigned int& modula = 4) {
  return value + ((value % modula > 0) ? (modula - value % modula) : 0);
}

class ScopedFile {
 public:
  ScopedFile(const WCHAR* path)
    : hFile(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) {}
  ~ScopedFile() { CloseHandle(hFile); }

  operator HANDLE() { return hFile; }

 private:
  HANDLE hFile;
};

class VersionStampValue {
public:
	WORD wLength = 0; // length in bytes of struct, including children
	WORD wValueLength = 0; // stringfileinfo, stringtable: 0; string: Value size in WORD; var: Value size in bytes
	WORD wType = 0; // 0: binary data; 1: text data
	std::wstring szKey; // stringtable: 8-digit hex stored as UTF-16 (hiword: hi6: sublang, lo10: majorlang; loword: code page); must include zero words to align next member on 32-bit boundary
	std::vector<BYTE> Value; // string: zero-terminated string; var: array of language & code page ID pairs
	std::vector<VersionStampValue> Children;

	size_t GetLength();
	std::vector<BYTE> Serialize();
};

typedef std::pair<const BYTE* const, const size_t> OffsetLengthPair;

}  // namespace

VersionInfo::VersionInfo(const HMODULE& hModule, const WORD& languageId)
{
  HRSRC hRsrc = FindResourceExW(hModule, RT_VERSION, MAKEINTRESOURCEW(1), languageId);

  if (hRsrc == NULL) {
    throw std::system_error(GetLastError(), std::system_category());
  }

  HGLOBAL hGlobal = LoadResource(hModule, hRsrc);
  if (hGlobal == NULL) {
    throw std::system_error(GetLastError(), std::system_category());
  }

  void* p = LockResource(hGlobal);
  if (p == NULL) {
    throw std::system_error(GetLastError(), std::system_category());
  }

  DWORD size = SizeofResource(hModule, hRsrc);
  if (size == 0) {
    throw std::system_error(GetLastError(), std::system_category());
  }

  DeserializeVersionInfo(static_cast<BYTE*>(p), size);
}

bool VersionInfo::HasFixedFileInfo() const
{
  return m_fixedFileInfo.dwSignature == 0xFEEF04BD;
}

VS_FIXEDFILEINFO& VersionInfo::GetFixedFileInfo()
{
  return m_fixedFileInfo;
}

void VersionInfo::SetFixedFileInfo(const VS_FIXEDFILEINFO& value)
{
  m_fixedFileInfo = value;
}

std::vector<BYTE> VersionInfo::Serialize()
{
  VersionStampValue versionInfo;
  versionInfo.szKey = L"VS_VERSION_INFO";
  versionInfo.wType = 0;

  if (HasFixedFileInfo())
  {
    auto size = sizeof(VS_FIXEDFILEINFO);
    versionInfo.wValueLength = size;
    
    auto& dst = versionInfo.Value;
    dst.resize(size);

    memcpy(&dst[0], &GetFixedFileInfo(), size);
  }

  {
    VersionStampValue stringFileInfo;
    stringFileInfo.szKey = L"StringFileInfo";
    stringFileInfo.wType = 1;
    stringFileInfo.wValueLength = 0;

    auto& stringTables = StringTables;
    for (auto iTable = stringTables.begin(); iTable != stringTables.end(); ++iTable)
    {
      VersionStampValue stringTableRaw;
      stringTableRaw.wType = 1;
      stringTableRaw.wValueLength = 0;

      {
        auto& translate = iTable->Encoding;
        std::wstringstream ss;
        ss << std::hex << std::setw(8) << std::setfill(L'0') << (translate.wLanguage << 16 | translate.wCodePage);
        stringTableRaw.szKey = ss.str();
      }

      auto& strings = iTable->Strings;
      for (auto iString = strings.begin(); iString != strings.end(); ++iString)
      {
        auto& stringValue = iString->second;
        auto strLenNullTerminated = stringValue.length() + 1;

        VersionStampValue stringRaw;
        stringRaw.wType = 1;
        stringRaw.szKey = iString->first;
        stringRaw.wValueLength = strLenNullTerminated;

        auto size = strLenNullTerminated * sizeof(WCHAR);
        auto& dst = stringRaw.Value;
        dst.resize(size);

        auto src = stringValue.c_str();
        
        memcpy(&dst[0], src, size);

        stringTableRaw.Children.push_back(std::move(stringRaw));
      }

      stringFileInfo.Children.push_back(std::move(stringTableRaw));
    }

    versionInfo.Children.push_back(std::move(stringFileInfo));
  }
  
  {
    VersionStampValue varFileInfo;
    varFileInfo.szKey = L"VarFileInfo";
    varFileInfo.wType = 1;
    varFileInfo.wValueLength = 0;

    {
      VersionStampValue varRaw;
      varRaw.szKey = L"Translation";
      varRaw.wType = 0;

      {
        auto& src = SupportedTranslations;
        auto newValueSize = sizeof(DWORD);
        auto& dst = varRaw.Value;
        dst.resize(src.size() * newValueSize);

        for (auto iVar = 0; iVar < src.size(); ++iVar)
        {
          auto& translate = src[iVar];
          auto var = DWORD(translate.wCodePage) << 16 | translate.wLanguage;
          memcpy(&dst[iVar * newValueSize], &var, newValueSize);
        }

        varRaw.wValueLength = varRaw.Value.size();
      }

      varFileInfo.Children.push_back(std::move(varRaw));
    }

    versionInfo.Children.push_back(std::move(varFileInfo));
  }

  versionInfo.wLength = versionInfo.GetLength();
  return move(versionInfo.Serialize());
}

VersionStringTable DeserializeVersionStringTable(const _VS_VERSION_STRING* const stringTable);
void DeserializeVersionStringFileInfo(const BYTE* offset, size_t length, std::vector<VersionStringTable>& stringTables);
void DeserializeVarFileInfo(const unsigned char* offset, std::vector<Translate>& translations);
OffsetLengthPair GetChildrenData(const VS_VERSION_STRING* const entry);

void VersionInfo::DeserializeVersionInfo(const BYTE* const pData, size_t size)
{
  const auto pVersionInfo = reinterpret_cast<const VS_VERSION_ROOT* const>(pData);
  const auto& fixedFileInfoSize = pVersionInfo->Header.wValueLength;

  if (fixedFileInfoSize > 0)
    SetFixedFileInfo(pVersionInfo->Info.Info);

  const auto fixedFileInfoEndOffset = reinterpret_cast<const BYTE* const>(&pVersionInfo->Info.szKey) + (wcslen(pVersionInfo->Info.szKey) + 1) * sizeof(WCHAR) + fixedFileInfoSize;
  const auto pVersionInfoChildren = reinterpret_cast<const BYTE* const>(round(reinterpret_cast<unsigned int>(fixedFileInfoEndOffset)));
  const auto versionInfoChildrenOffset = pVersionInfoChildren - pData;
  const auto versionInfoChildrenSize = pVersionInfo->Header.wLength - versionInfoChildrenOffset;

  const auto childrenEndOffset = pVersionInfoChildren + versionInfoChildrenSize;
  const auto resourceEndOffset = pData + size;
  for (auto p = pVersionInfoChildren; p < childrenEndOffset && p < resourceEndOffset;)
  {
    const auto pVersionInfoChild = reinterpret_cast<const VS_VERSION_STRING*>(p);

    const auto& pKey = pVersionInfoChild->szKey;
    const auto versionInfoChildData = GetChildrenData(pVersionInfoChild);
    if (wcscmp(pKey, L"StringFileInfo") == 0)
    {
      DeserializeVersionStringFileInfo(versionInfoChildData.first, versionInfoChildData.second, StringTables);
    }
    else if (wcscmp(pKey, L"VarFileInfo") == 0)
    {
      DeserializeVarFileInfo(versionInfoChildData.first, SupportedTranslations);
    }

    p += round(pVersionInfoChild->Header.wLength);
  }
}

VersionStringTable DeserializeVersionStringTable(const _VS_VERSION_STRING* const stringTable)
{
  const auto strings = GetChildrenData(stringTable);

  auto end_ptr = const_cast<WCHAR*>(stringTable->szKey + (8 * sizeof(WCHAR)));
  auto langIdCodePagePair = static_cast<DWORD>(wcstol(stringTable->szKey, &end_ptr, 16));

  VersionStringTable tableEntry;

  // unicode string of 8 hex digits
  tableEntry.Encoding.wLanguage = langIdCodePagePair >> 16;
  tableEntry.Encoding.wCodePage = langIdCodePagePair;

  for (auto posStrings = 0U; posStrings < strings.second;)
  {
    const auto stringEntry = reinterpret_cast<const VS_VERSION_STRING* const>(strings.first + posStrings);
    const auto stringData = GetChildrenData(stringEntry);
    tableEntry.Strings.push_back(std::pair<std::wstring, std::wstring>(stringEntry->szKey, std::wstring(reinterpret_cast<const WCHAR* const>(stringData.first), stringEntry->Header.wValueLength)));

    posStrings += round(stringEntry->Header.wLength);
  }

  return tableEntry;
}

void DeserializeVersionStringFileInfo(const BYTE* offset, size_t length, std::vector<VersionStringTable>& stringTables)
{
  for (auto posStringTables = 0U; posStringTables < length;)
  {
    const auto stringTable = reinterpret_cast<const VS_VERSION_STRING*>(offset + posStringTables);

    auto stringTableEntry = DeserializeVersionStringTable(stringTable);
    stringTables.push_back(stringTableEntry);

    posStringTables += round(stringTable->Header.wLength);
  }
}

void DeserializeVarFileInfo(const unsigned char* offset, std::vector<Translate>& translations)
{
  const auto varObject = reinterpret_cast<const VS_VERSION_STRING* const>(offset);
  const auto translatePairs = GetChildrenData(varObject);

  const auto top = reinterpret_cast<const DWORD* const>(translatePairs.first);
  for (auto pTranslatePair = top; pTranslatePair < top + translatePairs.second; pTranslatePair += sizeof(DWORD))
  {
    auto codePageLangIdPair = *pTranslatePair;
    Translate translate;
    translate.wLanguage = codePageLangIdPair;
    translate.wCodePage = codePageLangIdPair >> 16;
    translations.push_back(translate);
  }
}

OffsetLengthPair GetChildrenData(const VS_VERSION_STRING* const entry)
{
  auto headerOffset = reinterpret_cast<const BYTE* const>(entry);
  auto headerSize = sizeof(VS_VERSION_HEADER);
  auto keySize = (wcslen(entry->szKey) + 1) * sizeof(WCHAR);
  auto childrenOffset = round(headerSize + keySize);

  auto pChildren = headerOffset + childrenOffset;
  auto childrenSize = entry->Header.wLength - childrenOffset;
  return OffsetLengthPair(pChildren, childrenSize);
}

size_t VersionStampValue::GetLength()
{
  if (wLength > 0)
    return wLength;

  size_t bytes = sizeof(VS_VERSION_HEADER);
  bytes += static_cast<size_t>(szKey.length() + 1) * sizeof(WCHAR);

  if (!Value.empty())
  {
    bytes = round(bytes) + Value.size();
  }

  if (!Children.empty())
  {
    for (auto i = Children.begin(); i != Children.end(); ++i)
    {
      bytes = round(bytes) + static_cast<size_t>(i->GetLength());
    }
  }

  wLength = bytes;

  return bytes;
}

std::vector<BYTE> VersionStampValue::Serialize()
{
  std::vector<BYTE> data = std::vector<BYTE>(GetLength());
  memset(&data[0], NULL, data.size());

  size_t bytes = 0;

  size_t headerSize = sizeof(VS_VERSION_HEADER);
  memcpy(&data[bytes], this, headerSize);
  bytes += headerSize;

  auto keySize = static_cast<size_t>(szKey.length() + 1) * sizeof(WCHAR);
  memcpy(&data[bytes], szKey.c_str(), keySize);
  bytes += keySize;

  if (!Value.empty())
  {
    bytes = round(bytes);
    auto valueSize = Value.size();
    memcpy(&data[bytes], &Value[0], valueSize);
    bytes += valueSize;
  }

  if (!Children.empty())
  {
    for (auto i = Children.begin(); i != Children.end(); ++i)
    {
      bytes = round(bytes);
      auto childLength = i->GetLength();
      auto src = i->Serialize();
      memcpy(&data[bytes], &src[0], childLength);
      bytes += childLength;
    }
  }

  return move(data);
}

ResourceUpdater::ResourceUpdater() : hModule(NULL) {
}

ResourceUpdater::~ResourceUpdater() {
  if (hModule != NULL) {
    FreeLibrary(hModule);
    hModule = NULL;
  }
}

bool ResourceUpdater::Load(const WCHAR* filename) {
  hModule = LoadLibraryExW(filename, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
  if (hModule == NULL) {
    return false;
  }

  this->filename = filename;

  EnumResourceNamesW(hModule, RT_STRING, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
  EnumResourceNamesW(hModule, RT_VERSION, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
  EnumResourceNamesW(hModule, RT_GROUP_ICON, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
  EnumResourceNamesW(hModule, RT_ICON, OnEnumResourceName, reinterpret_cast<LONG_PTR>(this));
  EnumResourceNamesW(hModule, RT_MANIFEST, OnEnumResourceManifest, reinterpret_cast<LONG_PTR>(this));

  return true;
}

bool ResourceUpdater::SetExecutionLevel(const WCHAR* value) {
  std::wstring ws(value);
  executionLevel = std::wstring(ws.begin(), ws.end());
  return true;
}

bool ResourceUpdater::SetVersionString(const WORD& languageId, const WCHAR* name, const WCHAR* value) {
  if (versionStampMap.find(languageId) == versionStampMap.end()) {
    return false;
  }

  std::wstring nameStr(name);
  std::wstring valueStr(value);

  auto& stringTables = versionStampMap[languageId].StringTables;
  for (auto j = stringTables.begin(); j != stringTables.end(); ++j) {
    auto& stringPairs = j->Strings;
    for (auto k = stringPairs.begin(); k != stringPairs.end(); ++k) {
      if (k->first == nameStr) {
        k->second = valueStr;
        return true;
      }
    }

    // Not found, append one for all tables.
    stringPairs.push_back(VersionString(nameStr, valueStr));
  }

  return true;
}

bool ResourceUpdater::SetVersionString(const WCHAR* name, const WCHAR* value) {
  if (versionStampMap.size() < 1) {
    return false;
  } else {
    return SetVersionString(versionStampMap.begin()->first, name, value);
  }
}

bool ResourceUpdater::SetProductVersion(const WORD& languageId, const UINT& id, const unsigned short& v1, const unsigned short& v2, const unsigned short& v3, const unsigned short& v4) {
  if (versionStampMap.find(languageId) == versionStampMap.end()) {
    return false;
  }

  VersionInfo& versionInfo = versionStampMap[languageId];
  if (!versionInfo.HasFixedFileInfo()) {
    return false;
  }

  VS_FIXEDFILEINFO& root = versionInfo.GetFixedFileInfo();

  root.dwProductVersionMS = v1 << 16 | v2;
  root.dwProductVersionLS = v3 << 16 | v4;

  return true;
}

bool ResourceUpdater::SetProductVersion(const unsigned short& v1, const unsigned short& v2, const unsigned short& v3, const unsigned short& v4) {
  if (versionStampMap.size() < 1) {
    return false;
  } else {
    return SetProductVersion(versionStampMap.begin()->first, 1, v1, v2, v3, v4);
  }
}

bool ResourceUpdater::SetFileVersion(const WORD& languageId, const UINT& id, const unsigned short& v1, const unsigned short& v2, const unsigned short& v3, const unsigned short& v4) {
  if (versionStampMap.find(languageId) == versionStampMap.end()) {
    return false;
  }

  VersionInfo& versionInfo = versionStampMap[ languageId ];
  if (!versionInfo.HasFixedFileInfo()) {
    return false;
  }

  VS_FIXEDFILEINFO& root = versionInfo.GetFixedFileInfo();

  root.dwFileVersionMS = v1 << 16 | v2;
  root.dwFileVersionLS = v3 << 16 | v4;
  return true;
}

bool ResourceUpdater::SetFileVersion(const unsigned short& v1, const unsigned short& v2, const unsigned short& v3, const unsigned short& v4) {
  if (versionStampMap.size() < 1) {
    return false;
  } else {
    return SetFileVersion(versionStampMap.begin()->first, 1, v1, v2, v3, v4);
  }
}

bool ResourceUpdater::ChangeString(const WORD& languageId, const UINT& id, const WCHAR* value) {
  if (stringTableMap.find(languageId) == stringTableMap.end()) {
    return false;
  }

  StringTable& table = stringTableMap[ languageId ];

  UINT blockId = id / 16;
  if (table.find(blockId) == table.end()) {
    return false;
  }

  assert(table[ blockId ].size() == 16);
  UINT blockIndex = id % 16;
  table[ blockId ][ blockIndex ] = value;

  return true;
}

bool ResourceUpdater::ChangeString(const UINT& id, const WCHAR* value) {
  if (stringTableMap.size() < 1) {
    return false;
  } else {
    return ChangeString(stringTableMap.begin()->first, id, value);
  }
}

bool ResourceUpdater::SetIcon(const WCHAR* path, const LANGID& langId, const UINT& iconBundle)
{
  auto& pIcon = iconBundleMap[langId].IconBundles[iconBundle];
  if (pIcon == nullptr)
    pIcon = std::make_unique<IconsValue>();

  auto& icon = *pIcon;
  DWORD bytes;

  ScopedFile file(path);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  IconsValue::ICONHEADER& header = icon.header;
  if (!ReadFile(file, &header, 3 * sizeof(WORD), &bytes, NULL))
    return false;

  if (header.reserved != 0 || header.type != 1)
    return false;

  header.entries.resize(header.count);
  if (!ReadFile(file, header.entries.data(), header.count * sizeof(IconsValue::ICONENTRY), &bytes, NULL))
    return false;

  icon.images.resize(header.count);
  for (size_t i = 0; i < header.count; ++i) {
    icon.images[i].resize(header.entries[i].bytesInRes);
    SetFilePointer(file, header.entries[i].imageOffset, NULL, FILE_BEGIN);
    if (!ReadFile(file, icon.images[i].data(), icon.images[i].size(), &bytes, NULL))
      return false;
  }

  icon.grpHeader.resize(3 * sizeof(WORD) + header.count * sizeof(GRPICONENTRY));
  GRPICONHEADER* pGrpHeader = reinterpret_cast<GRPICONHEADER*>(icon.grpHeader.data());
  pGrpHeader->reserved = 0;
  pGrpHeader->type = 1;
  pGrpHeader->count = header.count;
  for (size_t i = 0; i < header.count; ++i) {
    GRPICONENTRY* entry = pGrpHeader->entries + i;
    entry->bitCount    = 0;
    entry->bytesInRes  = header.entries[i].bitCount;
    entry->bytesInRes2 = header.entries[i].bytesInRes;
    entry->colourCount = header.entries[i].colorCount;
    entry->height      = header.entries[i].height;
    entry->id          = i + 1;
    entry->planes      = header.entries[i].planes;
    entry->reserved    = header.entries[i].reserved;
    entry->width       = header.entries[i].width;
    entry->reserved2   = 0;
  }

  return true;
}

bool ResourceUpdater::SetIcon(const WCHAR* path, const LANGID& langId)
{
  auto& iconBundle = iconBundleMap[langId].IconBundles.begin()->first;
  return SetIcon(path, langId, iconBundle);
}

bool ResourceUpdater::SetIcon(const WCHAR* path) {
  auto& langId = iconBundleMap.begin()->first;
  return SetIcon(path, langId);
}

bool ResourceUpdater::Commit() {
  if (hModule == NULL) {
    return false;
  }
  FreeLibrary(hModule);
  hModule = NULL;

  ScopedResourceUpdater ru(filename.c_str(), false);
  if (ru.Get() == NULL) {
    return false;
  }

  // update version info.
  for (VersionStampMap::iterator i = versionStampMap.begin(); i != versionStampMap.end(); i++) {
    auto& langId = i->first;
    auto& versionInfo = i->second;
    auto out = versionInfo.Serialize();

    if (!UpdateResourceW
      (ru.Get()
      , RT_VERSION
      , MAKEINTRESOURCEW(1)
      , langId
      , &out[0], static_cast<DWORD>(out.size()))) {

      return false;
    }
  }

  // update the execution level
  if (!executionLevel.empty())
  {
    // string replace with requested executionLevel
    std::wstring::size_type pos = 0u;
    while ((pos = manifestString.find(original_executionLevel, pos)) != std::string::npos) {
      manifestString.replace(pos, original_executionLevel.length(), executionLevel);
      pos += executionLevel.length();
    }

    //std::wstring wstr(manifestString.begin(), manifestString.end());

    std::vector<char> stringSection;
    stringSection.clear();
    stringSection.push_back(manifestString.size());
    stringSection.insert(stringSection.end(), manifestString.begin(), manifestString.end());
    
    // not sure why I need to add +1 here, but it looks like the first character is the length.
    if (!UpdateResourceW
    (ru.Get()
      , RT_MANIFEST
      , MAKEINTRESOURCEW(1)
      , 1033
      , &stringSection.at(0) + 1, sizeof(char) * manifestString.size())) {

      return false;
    }
  }

  // update string table.
  for (StringTableMap::iterator i = stringTableMap.begin(); i != stringTableMap.end(); i++) {
    for (StringTable::iterator j = i->second.begin(); j != i->second.end(); j++) {
      std::vector<char> stringTableBuffer;
      if (!SerializeStringTable(j->second, j->first, stringTableBuffer)) {
        return false;
      }

      if (!UpdateResourceW
        (ru.Get()
        , RT_STRING
        , MAKEINTRESOURCEW(j->first + 1)
        , i->first
        , &stringTableBuffer[0], static_cast<DWORD>(stringTableBuffer.size()))) {

        return false;
      }
    }
  }

  for (auto iLangIconInfoPair = iconBundleMap.begin(); iLangIconInfoPair != iconBundleMap.end(); ++iLangIconInfoPair)
  {
    auto& langId = iLangIconInfoPair->first;
    auto& maxIconId = iLangIconInfoPair->second.MaxIconId;
    for (auto iNameBundlePair = iLangIconInfoPair->second.IconBundles.begin(); iNameBundlePair != iLangIconInfoPair->second.IconBundles.end(); ++iNameBundlePair)
    {
      auto& bundleId = iNameBundlePair->first;
      auto& pIcon = iNameBundlePair->second;
      if (pIcon == nullptr)
        continue;

      auto& icon = *pIcon;
      // update icon.
      if (icon.grpHeader.size() > 0) {
        if (!UpdateResourceW
          (ru.Get()
            , RT_GROUP_ICON
            , MAKEINTRESOURCEW(bundleId)
            , langId
            , icon.grpHeader.data()
            , icon.grpHeader.size())) {

          return false;
        }

        for (size_t i = 0; i < icon.header.count; ++i) {
          if (!UpdateResourceW
            (ru.Get()
              , RT_ICON
              , MAKEINTRESOURCEW(i + 1)
              , langId
              , icon.images[i].data()
              , icon.images[i].size())) {

            return false;
          }
        }

        for (size_t i = icon.header.count; i < maxIconId; ++i) {
          if (!UpdateResourceW
            (ru.Get()
              , RT_ICON
              , MAKEINTRESOURCEW(i + 1)
              , langId
              , nullptr
              , 0)) {

            return false;
          }
        }
      }
    }
  }

  return ru.Commit();
}

bool ResourceUpdater::GetResourcePointer(const HMODULE& hModule, const WORD& languageId, const int& id, const WCHAR* type, BYTE*& data, size_t& dataSize) {
  if (!IS_INTRESOURCE(id)) {
    return false;
  }

  HRSRC hRsrc = FindResourceExW(hModule, type, MAKEINTRESOURCEW(id), languageId);
  if (hRsrc == NULL) {
    DWORD e = GetLastError();
    return false;
  }

  DWORD size = SizeofResource(hModule, hRsrc);
  if (size == 0) {
    return false;
  }

  HGLOBAL hGlobal = LoadResource(hModule, hRsrc);
  if (hGlobal == NULL) {
    return false;
  }

  void* p = LockResource(hGlobal);
  if (p == NULL) {
    return false;
  }

  dataSize = static_cast<size_t>(size);
  data = static_cast<BYTE*>(p);
  return true;
}

// static
bool ResourceUpdater::UpdateRaw
(const WCHAR* filename
, const WORD& languageId
, const WCHAR* type
, const UINT& id
, const void* data
, const size_t& dataSize
, const bool& deleteOld) {

  ScopedResourceUpdater ru(filename, deleteOld);
  if (ru.Get() == NULL) {
    return false;
  }

  if (UpdateResourceW(ru.Get(), type, MAKEINTRESOURCEW(id), languageId, const_cast<void*>(data), static_cast<DWORD>(dataSize))) {
    return ru.Commit();
  } else {
    return false;
  }
}

bool ResourceUpdater::SerializeStringTable(const StringValues& values, const UINT& blockId, std::vector<char>& out) {
  // calc total size.
  // string table is pascal string list.
  size_t size = 0;
  for (size_t i = 0; i < 16; i++) {
    size += sizeof(WORD);
    size += values[ i ].length() * sizeof(WCHAR);
  }

  out.resize(size);

  // write.
  char* pDst = &out[0];
  for (size_t i = 0; i < 16; i++) {
    WORD length = static_cast<WORD>(values[ i ].length());
    memcpy(pDst, &length, sizeof(length));
    pDst += sizeof(WORD);

    if (length > 0) {
      WORD bytes = length * sizeof(WCHAR);
      memcpy(pDst, values[ i ].c_str(), bytes);
      pDst += bytes;
    }
  }

  return true;
}

// static
BOOL CALLBACK ResourceUpdater::OnEnumResourceLanguage(HANDLE hModule, LPCWSTR lpszType, LPCWSTR lpszName, WORD wIDLanguage, LONG_PTR lParam) {
  ResourceUpdater* instance = reinterpret_cast<ResourceUpdater*>(lParam);
  if (IS_INTRESOURCE(lpszName) && IS_INTRESOURCE(lpszType)) {
    switch (reinterpret_cast<UINT>(lpszType))
    {
    case reinterpret_cast<UINT>(RT_VERSION):
      {
        try
        {
          instance->versionStampMap[wIDLanguage] = VersionInfo(instance->hModule, wIDLanguage);
        }
        catch (const std::system_error& e)
        {
          return false;
        }
      }
      break;
    case reinterpret_cast<UINT>(RT_STRING):
	  {
        UINT id = reinterpret_cast<UINT>(lpszName) - 1;
        auto& vector = instance->stringTableMap[wIDLanguage][id];
        for (size_t k = 0; k < 16; k++) {
          CStringW buf;

          buf.LoadStringW(instance->hModule, id * 16 + k, wIDLanguage);
          vector.push_back(buf.GetBuffer());
        }
	  }
      break;
    case reinterpret_cast<UINT>(RT_ICON):
      {
        auto iconId = reinterpret_cast<UINT>(lpszName);
        auto& maxIconId = instance->iconBundleMap[wIDLanguage].MaxIconId;
        if (iconId > maxIconId)
          maxIconId = iconId;
      }
      break;
    case reinterpret_cast<UINT>(RT_GROUP_ICON):
      instance->iconBundleMap[wIDLanguage].IconBundles[reinterpret_cast<UINT>(lpszName)] = nullptr;
      break;
    default:
      break;
    }
  }
  return TRUE;
}

// static
BOOL CALLBACK ResourceUpdater::OnEnumResourceName(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam) {
  EnumResourceLanguagesW(hModule, lpszType, lpszName, (ENUMRESLANGPROCW) OnEnumResourceLanguage, lParam);
  return TRUE;
}

// courtesy of http://stackoverflow.com/questions/420852/reading-an-applications-manifest-file
BOOL CALLBACK ResourceUpdater::OnEnumResourceManifest(HMODULE hModule, LPCTSTR lpType,
  LPWSTR lpName, LONG_PTR lParam)
{
  ResourceUpdater* instance = reinterpret_cast<ResourceUpdater*>(lParam);
  HRSRC hResInfo = FindResource(hModule, lpName, lpType);
  DWORD cbResource = SizeofResource(hModule, hResInfo);

  HGLOBAL hResData = LoadResource(hModule, hResInfo);
  const BYTE *pResource = (const BYTE *)LockResource(hResData);

  int len = strlen(reinterpret_cast<const char*>(pResource));
  std::wstring manifestStringLocal(pResource, pResource + len);
  size_t found = manifestStringLocal.find(L"requestedExecutionLevel");
  size_t end = manifestStringLocal.find(L"uiAccess");

  instance->original_executionLevel = manifestStringLocal.substr(found +31 , end - found - 33);

  // also store original manifestString
  instance->manifestString = manifestStringLocal;

  UnlockResource(hResData);
  FreeResource(hResData);

  return TRUE;   // Keep going
}

ScopedResourceUpdater::ScopedResourceUpdater(const wchar_t* filename, const bool& deleteOld)
: handle(NULL)
, commited(false) {
  handle = BeginUpdateResourceW(filename, deleteOld ? TRUE : FALSE);
}

ScopedResourceUpdater::~ScopedResourceUpdater() {
  if (!commited) {
    EndUpdate(false);
  }
}

HANDLE ScopedResourceUpdater::Get() const {
  return handle;
}

bool ScopedResourceUpdater::Commit() {
  commited = true;
  return EndUpdate(true);
}

bool ScopedResourceUpdater::EndUpdate(const bool& doesCommit) {
  BOOL fDiscard = doesCommit ? FALSE : TRUE;
  BOOL bResult = EndUpdateResourceW(handle, fDiscard);
  DWORD e = GetLastError();
  return bResult ? true : false;
}

}  // namespace rescle

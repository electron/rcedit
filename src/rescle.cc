// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.
//
// This file is modified from Rescle written by yoshio.okumura@gmail.com:
// http://code.google.com/p/rescle/

#include "rescle.h"

#include <assert.h>
#include <atlstr.h>

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
  WORD wKeyLength;
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

struct Translate {
  WORD wLanguage;
  WORD wCodePage;
};

unsigned short round(const unsigned short& value,
                     const unsigned short& modula = 4) {
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

}  // namespace

unsigned short VersionStampValue::GetLength(const bool& rounding) const {
  unsigned short bytes = sizeof(VS_VERSION_HEADER);
  bytes += static_cast<unsigned short>(szKey.length() + 1) * sizeof(wchar_t);
  bytes += static_cast<unsigned short>(Data.size());
  return rounding ? round(bytes, 4) : bytes;
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

  for (VersionStampMap::iterator i = versionStampMap.begin(); i != versionStampMap.end(); i++) {
    for (VersionStampTable::iterator j = i->second.begin(); j != i->second.end(); j++) {
      BYTE* data = NULL;
      size_t dataSize = 0;
      if (!GetResourcePointer(hModule, i->first, j->first, RT_VERSION, data, dataSize)) {
        return false;
      }
      if (!Deserialize(data, dataSize, j->second)) {
        return false;
      }
    }
  }

  // Load Strings
  for (StringTableMap::iterator i = stringTableMap.begin(); i != stringTableMap.end(); i++) {
    for (StringTable::iterator j = i->second.begin(); j != i->second.end(); j++) {
      for (size_t k = 0; k < 16; k++) {
        CStringW buf;
        buf.LoadStringW(hModule, j->first * 16 + k, i->first);
        j->second.push_back(buf.GetBuffer());
      }
    }
  }

  return true;
}

bool ResourceUpdater::SetVersionString(const WORD& languageId, const WCHAR* name, const WCHAR* value) {
  if (versionStampMap.find(languageId) == versionStampMap.end()) {
    return false;
  }

  VersionStampTable& table = versionStampMap[languageId];

  std::wstring nameStr(name);
  std::wstring valueStr(value);
  size_t sizeWithoutNull = (valueStr.size() + 0) * 2; // not null-terminated.
  size_t sizeWithNull = round((valueStr.size() + 1) * 2); // null-terminated.

  std::vector<char> data;
  data.resize(sizeWithNull);
  memset(&(data[0]), 0, sizeWithNull);
  memcpy(&(data[0]), &valueStr[0], sizeWithoutNull);

  for (VersionStampTable::iterator i = table.begin(); i != table.end(); i++) {
    for (VersionStampValues::iterator j = i->second.begin(); j != i->second.end(); j++) {
      if (j->szKey == nameStr) {
        j->Data = data;
        return true;
      }
    }

    // Not found, append one for all tables.
    if (i->second.size() > 4) {
      VersionStampValue entry = { 0, 0, 0, 0, nameStr };
      entry.Data = data;
      i->second.insert(i->second.end() - 2, entry);
    }
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

  VersionStampTable& table = versionStampMap[ languageId ];
  if (table.find(id) == table.end()) {
    return false;
  }

  VS_FIXEDFILEINFO* root = (VS_FIXEDFILEINFO*) &(table[id][0].Data[0]);

  root->dwProductVersionMS = v1 << 16 | v2;
  root->dwProductVersionLS = v3 << 16 | v4;

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

  VersionStampTable& table = versionStampMap[ languageId ];
  if (table.find(id) == table.end()) {
    return false;
  }

  VS_FIXEDFILEINFO* root = (VS_FIXEDFILEINFO*) &(table[id][0].Data[0]);

  root->dwFileVersionMS = v1 << 16 | v2;
  root->dwFileVersionLS = v3 << 16 | v4;
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
    for (VersionStampTable::iterator j = i->second.begin(); j != i->second.end(); j++) {
      std::vector<char> out;
      WORD lang = i->first;
      UINT id = j->first;
      if (!SerializeVersionInfo(j->second, out)) {
        return false;
      }

      if (!UpdateResourceW
        (ru.Get()
        , RT_VERSION
        , MAKEINTRESOURCEW(j->first)
        , i->first
        , &out[0], static_cast<DWORD>(out.size()))) {

        return false;
      }
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

        for (size_t i = icon.header.count; i < maxIconId - 1; ++i) {
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

bool ResourceUpdater::Deserialize(const BYTE* data, const size_t& dataSize, VersionStampValues& values) {
  unsigned char* pTop = reinterpret_cast<unsigned char*>(const_cast<BYTE*>(data));

  // 4 byte alignment
  for (unsigned short i = 0; i < dataSize;) {
    VS_VERSION_STRING* entry = reinterpret_cast<VS_VERSION_STRING*>(pTop + i);

    unsigned short bytes = 0;
    if (entry->Header.wType == 0) {
      VS_VERSION_ROOT* root = (VS_VERSION_ROOT*) entry;
      bytes = entry->Header.wKeyLength;
    } else if (entry->Header.wKeyLength > 0) {
      bytes = round(entry->Header.wKeyLength * sizeof(wchar_t)); // unicode 16.
    } else {
      bytes = entry->Header.wKeyLength;
    }

    VersionStampValue h;
    i += sizeof(VS_VERSION_HEADER);
    i += static_cast<unsigned short>(wcslen(entry->szKey) + 1) * sizeof(wchar_t); // unicode 16.
    if (bytes > 0) {
      wchar_t* value = (wchar_t*) (pTop + round(i));
      h.Data.resize(bytes);
      memcpy(&h.Data[0], value, bytes);
    }
    i = round(i + bytes);

    h.wOffset = 0;
    h.wLength = 0;
    h.wKeyLength = 0;
    h.wType = 0;
    h.szKey = entry->szKey;
    values.push_back(h);
  }
  return true;
}

bool ResourceUpdater::SerializeVersionInfo(VersionStampValues& values, std::vector<char>& out) {
  // set type.
  for (size_t i = 1; i < values.size() - 1; i++) {
    values[i].wType = 1;
  }

  // calc offset all.
  // calc tree size.
  unsigned short rootSize = 0;
  for (size_t i = 0; i < values.size(); i++) {
    values[i].wOffset = rootSize;
    rootSize += values[i].GetLength();
  }

  unsigned short stringFileInfoSize = values[values.size() - 2].wOffset - values[1].wOffset - 2;
  unsigned short localeSize = values[values.size() - 2].wOffset - values[2].wOffset - 2;
  unsigned short verFileInfoSize = rootSize - values[values.size() - 2].wOffset;

  // set length
  values[0].wLength = rootSize;
  values[1].wLength = stringFileInfoSize;
  values[2].wLength = localeSize;
  for (size_t i = 3; i < values.size() - 2; i++) {
    unsigned short len = static_cast<unsigned short>(values[i].Data.size()) / sizeof(wchar_t) - 1; // exclude null
    values[i].wKeyLength = len;
    values[i].wLength = values[i + 1].wOffset - values[i].wOffset;
    if (values[i].wKeyLength % 2 == 1) {
      values[i].wLength -= sizeof(wchar_t);
    }
  }
  values[values.size() - 2].wLength = verFileInfoSize;
  values[values.size() - 1].wLength = round(values[values.size() - 1].GetLength(false) + values[values.size() - 1].wKeyLength);

  values[0].wKeyLength = static_cast<unsigned short>(values[0].Data.size());
  values[values.size() - 1].wKeyLength = static_cast<unsigned short>(values[values.size() - 1].Data.size());

  out.resize(values[0].wLength);

  unsigned short offset = 0;
  char* pTop = &out[0];
  char* pDst = pTop;
  for (size_t i = 0; i < values.size(); i++) {
    offset = values[i].wOffset;
    pDst = pTop + offset;
    // copy 6 bytes.
    {
      unsigned short bytes = sizeof(VS_VERSION_HEADER);
      char* pSrc = (char*) &(values[i].wLength);
      memcpy(pDst, pSrc, bytes);
      offset += bytes;
      pDst += bytes;
      pSrc += bytes;

      // copy key.
      bytes = static_cast<unsigned short>(values[i].szKey.size() + 1) * sizeof(wchar_t);
      memcpy(pDst, &values[i].szKey[0], bytes);
      offset += bytes;

      // padding
      offset = round(offset);
    }

    pDst = pTop + offset;
    if (values[i].Data.size() > 0) {
      memcpy(pDst, &values[i].Data[0], values[i].Data.size());
    }
  }
  return true;
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
      instance->versionStampMap[ wIDLanguage ][ reinterpret_cast<UINT>(lpszName) ].resize(0);
      break;
    case reinterpret_cast<UINT>(RT_STRING):
      instance->stringTableMap[ wIDLanguage ][ reinterpret_cast<UINT>(lpszName) - 1 ].resize(0);
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

// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include <string.h>

#include "rescle.h"
#include "version.h"

namespace {

void print_help() {
  fprintf(stdout,
"Rcedit " RCEDIT_VERSION ": Edit resources of exe.\n\n"
"Usage: rcedit <filename> [options...]\n\n"
"Options:\n"
"  -h, --help                                 Show this message\n"
"  --set-version-string <key> <value>         Set version string\n"
"  --get-version-string <key>                 Print version string\n"
"  --set-file-version <version>               Set FileVersion\n"
"  --set-product-version <version>            Set ProductVersion\n"
"  --set-icon <path-to-icon>                  Set file icon\n"
"  --set-requested-execution-level <level>    Pass nothing to see usage\n"
"  --application-manifest <path-to-file>      Set manifest file\n"
"  --set-resource-string <key> <value>        Set resource string\n"
"  --get-resource-string <key>                Get resource string\n"
"  --set-rcdata <key> <path-to-file>          Replace RCDATA by integer id\n");
}

bool print_error(const char* message) {
  fprintf(stderr, "Fatal error: %s\n", message);
  return 1;
}

bool print_warning(const char* message) {
  fprintf(stderr, "Warning: %s\n", message);
  return 1;
}

bool parse_version_string(const wchar_t* str, unsigned short *v1, unsigned short *v2, unsigned short *v3, unsigned short *v4) {
  *v1 = *v2 = *v3 = *v4 = 0;
  return (swscanf_s(str, L"%hu.%hu.%hu.%hu", v1, v2, v3, v4) == 4) ||
         (swscanf_s(str, L"%hu.%hu.%hu", v1, v2, v3) == 3) ||
         (swscanf_s(str, L"%hu.%hu", v1, v2) == 2) ||
         (swscanf_s(str, L"%hu", v1) == 1);
}

}  // namespace

int wmain(int argc, const wchar_t* argv[]) {
  bool loaded = false;
  rescle::ResourceUpdater updater;

  if (argc == 1 ||
      (argc == 2 && wcscmp(argv[1], L"-h") == 0) ||
      (argc == 2 && wcscmp(argv[1], L"--help") == 0)) {
    print_help();
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    if (wcscmp(argv[i], L"--set-version-string") == 0 ||
        wcscmp(argv[i], L"-svs") == 0) {
      if (argc - i < 3)
        return print_error("--set-version-string requires 'Key' and 'Value'");

      const wchar_t* key = argv[++i];
      const wchar_t* value = argv[++i];
      if (!updater.SetVersionString(key, value))
        return print_error("Unable to change version string");

    } else if (wcscmp(argv[i], L"--get-version-string") == 0 ||
               wcscmp(argv[i], L"-gvs") == 0) {
      if (argc - i < 2)
        return print_error("--get-version-string requires 'Key'");
      const wchar_t* key = argv[++i];
      const wchar_t* result = updater.GetVersionString(key);
      if (!result)
        return print_error("Unable to get version string");

      fwprintf(stdout, L"%s", result);
      return 0;  // no changes made

    } else if (wcscmp(argv[i], L"--set-file-version") == 0 ||
               wcscmp(argv[i], L"-sfv") == 0) {
      if (argc - i < 2)
        return print_error("--set-file-version requires a version string");

      unsigned short v1, v2, v3, v4;
      if (!parse_version_string(argv[++i], &v1, &v2, &v3, &v4))
        return print_error("Unable to parse version string for FileVersion");

      if (!updater.SetFileVersion(v1, v2, v3, v4))
        return print_error("Unable to change file version");

      if (!updater.SetVersionString(L"FileVersion", argv[i]))
        return print_error("Unable to change FileVersion string");

    } else if (wcscmp(argv[i], L"--set-product-version") == 0 ||
               wcscmp(argv[i], L"-spv") == 0) {
      if (argc - i < 2)
        return print_error("--set-product-version requires a version string");

      unsigned short v1, v2, v3, v4;
      if (!parse_version_string(argv[++i], &v1, &v2, &v3, &v4))
        return print_error("Unable to parse version string for ProductVersion");

      if (!updater.SetProductVersion(v1, v2, v3, v4))
        return print_error("Unable to change product version");

      if (!updater.SetVersionString(L"ProductVersion", argv[i]))
        return print_error("Unable to change ProductVersion string");

    } else if (wcscmp(argv[i], L"--set-icon") == 0 ||
               wcscmp(argv[i], L"-si") == 0) {
      if (argc - i < 2)
        return print_error("--set-icon requires path to the icon");

      if (!updater.SetIcon(argv[++i]))
        return print_error("Unable to set icon");

    } else if (wcscmp(argv[i], L"--set-requested-execution-level") == 0 ||
      wcscmp(argv[i], L"-srel") == 0) {
      if (argc - i < 2)
        return print_error("--set-requested-execution-level requires asInvoker, highestAvailable or requireAdministrator");

      if (updater.IsApplicationManifestSet())
        print_warning("--set-requested-execution-level is ignored if --application-manifest is set");

      if (!updater.SetExecutionLevel(argv[++i]))
        return print_error("Unable to set execution level");

    } else if (wcscmp(argv[i], L"--application-manifest") == 0 ||
      wcscmp(argv[i], L"-am") == 0) {
      if (argc - i < 2)
        return print_error("--application-manifest requires local path");

      if (updater.IsExecutionLevelSet())
        print_warning("--set-requested-execution-level is ignored if --application-manifest is set");

      if (!updater.SetApplicationManifest(argv[++i]))
        return print_error("Unable to set application manifest");

    } else if (wcscmp(argv[i], L"--set-resource-string") == 0 ||
      wcscmp(argv[i], L"--srs") == 0) {
      if (argc - i < 3)
        return print_error("--set-resource-string requires int 'Key' and string 'Value'");

      const wchar_t* key = argv[++i];
      unsigned int key_id = 0;
      if (swscanf_s(key, L"%d", &key_id) != 1)
        return print_error("Unable to parse id");

      const wchar_t* value = argv[++i];
      if (!updater.ChangeString(key_id, value))
        return print_error("Unable to change string");

    } else if (wcscmp(argv[i], L"--set-rcdata") == 0) {
      if (argc - i < 3)
        return print_error("--set-rcdata requires int 'Key' and path to resource 'Value'");

      const wchar_t* key = argv[++i];
      unsigned int key_id = 0;
      if (swscanf_s(key, L"%d", &key_id) != 1)
        return print_error("Unable to parse id");

      const wchar_t* pathToResource = argv[++i];
      if (!updater.ChangeRcData(key_id, pathToResource))
        return print_error("Unable to change RCDATA");
    } else if (wcscmp(argv[i], L"--get-resource-string") == 0 ||
      wcscmp(argv[i], L"-grs") == 0) {
      if (argc - i < 2)
        return print_error("--get-resource-string requires int 'Key'");

      const wchar_t* key = argv[++i];
      unsigned int key_id = 0;
      if (swscanf_s(key, L"%d", &key_id) != 1)
        return print_error("Unable to parse id");

      const wchar_t* result = updater.GetString(key_id);
      if (!result)
        return print_error("Unable to get resource string");

      fwprintf(stdout, L"%s", result);
      return 0;  // no changes made

    } else {
      if (loaded) {
        fprintf(stderr, "Unrecognized argument: \"%ls\"\n", argv[i]);
        return 1;
      }

      loaded = true;
      if (!updater.Load(argv[i])) {
        fprintf(stderr, "Unable to load file: \"%ls\"\n", argv[i]);
        return 1;
      }

    }
  }

  if (!loaded)
    return print_error("You should specify a exe/dll file");

  if (!updater.Commit())
    return print_error("Unable to commit changes");

  return 0;
}

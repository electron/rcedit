// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include <string.h>
#include <stdlib.h>

#include "rescle.h"

bool print_error(const char* message) {
  fprintf(stderr, "Fatal error: %s\n", message);
  return 1;
}

int wmain(int argc, const wchar_t* argv[]) {
  bool loaded = false;
  rescle::ResourceUpdater updater;

  for (int i = 1; i < argc; ++i) {
    if (wcscmp(argv[i], L"--set-version-string") == 0 ||
        wcscmp(argv[i], L"-s") == 0) {
      if (argc - i < 3)
        return print_error("--set-version-string requires 'Key' and 'Value'");

      const wchar_t* key = argv[++i];
      const wchar_t* value = argv[++i];
      if (!updater.ChangeVersionString(key, value))
        return print_error("Unable to change version string");
    } else {
      if (loaded)
        return print_error("Unexpected trailing arguments");

      loaded = true;
      if (!updater.Load(argv[i]))
        return print_error("Unable to load file");
    }
  }

  if (!loaded)
    return print_error("You should specify a exe/dll file");

  if (!updater.Commit())
    return print_error("Unable to commit changes");

  return 0;
}

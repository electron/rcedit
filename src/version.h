// Copyright (c) 2017 GitHub, Inc. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef SRC_VERSION_H_
#define SRC_VERSION_H_

#define RCEDIT_MAJOR_VERSION 0
#define RCEDIT_MINOR_VERSION 2
#define RCEDIT_PATCH_VERSION 0

#ifndef RCEDIT_TAG
# define RCEDIT_TAG ""
#endif

#ifndef RCEDIT_STRINGIFY
#define RCEDIT_STRINGIFY(n) RCEDIT_STRINGIFY_HELPER(n)
#define RCEDIT_STRINGIFY_HELPER(n) #n
#endif

#define RCEDIT_VERSION_STRING  RCEDIT_STRINGIFY(RCEDIT_MAJOR_VERSION) "." \
                               RCEDIT_STRINGIFY(RCEDIT_MINOR_VERSION) "." \
                               RCEDIT_STRINGIFY(RCEDIT_PATCH_VERSION)     \
                               RCEDIT_TAG

#define RCEDIT_VERSION "v" RCEDIT_VERSION_STRING

#endif  // SRC_VERSION_H_

# rcedit [![Build status](https://ci.appveyor.com/api/projects/status/o8d047nebu8j94v3/branch/master?svg=true)](https://ci.appveyor.com/project/electron-bot/rcedit/branch/master)

Command line tool to edit resources of exe file on Windows

Requires `Microsoft Visual C++ Redistributable Packages for Visual Studio 2013`:
http://www.microsoft.com/en-us/download/details.aspx?id=40784

## Building
  * Make sure you have gyp is configured in your system. If not, clone gyp from https://chromium.googlesource.com/external/gyp
  * Clone the repository
  * Run `gyp rcedit.gyp --depth .`
  * Build rcedit.sln with Visual Studio

## Docs

Set version string:

```bash
$ rcedit "path-to-exe-or-dll" --set-version-string "Comments" "This is an exe"
```

Set file version:

```bash
$ rcedit "path-to-exe-or-dll" --set-file-version "10.7"
```

Set product version:

```bash
$ rcedit "path-to-exe-or-dll" --set-product-version "10.7"
```

Set icon:

```bash
$ rcedit "path-to-exe-or-dll" --set-icon "path-to-ico"
```

Set resource string:

```bash
$ rcedit "path-to-exe-or-dll" --set-resource-string id_number "new string value"
```

Set [requested execution level](https://msdn.microsoft.com/en-us/library/6ad1fshk.aspx#Anchor_9) (`asInvoker` | `highestAvailable` | `requireAdministrator`) in the manifest:

```bash
$ rcedit "path-to-exe-or-dll" --set-requested-execution-level "requireAdministrator"
```

Set [application manifest](https://msdn.microsoft.com/en-us/library/windows/desktop/aa374191.aspx):

```bash
$ rcedit "path-to-exe-or-dll" --application-manifest ./path/to/manifest/file
```

And you can change multiple things in one command:

```bash
$ rcedit "path-to-exe-or-dll" --set-icon "path-to-ico" --set-file-version "10.7"
```

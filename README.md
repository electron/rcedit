# rcedit [![Build status](https://ci.appveyor.com/api/projects/status/99eokln2emhidcej?svg=true)](https://ci.appveyor.com/project/zcbenz/rcedit/branch/master)

Command line tool to edit resources of exe file on Windows.

## Executables

Prebuilt binaries can be found in the artifacts of appveyor jobs.

## Building

1. Clone the repository
2. Open `rcedit.sln` with Visual Studio 2015 or above
3. Build

## Generate solution files

If you have modified the gyp files, you should regenerate the solution files:

1. Make sure you have gyp configured on your system. If not, clone gyp from
   https://chromium.googlesource.com/external/gyp
2. Run `gyp rcedit.gyp --depth .`

## Docs

Show help:

```bash
$ rcedit -h
```

Set version string:

```bash
$ rcedit "path-to-exe-or-dll" --set-version-string "Comments" "This is an exe"
```

Use this option to change any supported properties, as described in the MSDN documentation [here](https://msdn.microsoft.com/en-us/library/windows/desktop/aa381058(v=vs.85).aspx)

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

Get version string:

```bash
$ rcedit "path-to-exe-or-dll" --get-version-string "property"
```

Use the same properties as `--set-version-string`. Use `"FileVersion"` to get the results of `--set-file-version` and `"ProductVersion"` to get the results of `--get-product-version`.

Get resource string:

```bash
$ rcedit "path-to-exe-or-dll" --get-resource-string id_number
```

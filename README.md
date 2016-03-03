# rcedit

Command line tool to edit resources of exe file on Windows

Requires `Microsoft Visual C++ Redistributable Packages for Visual Studio 2013`:
http://www.microsoft.com/en-us/download/details.aspx?id=40784

## Building
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

And you can change multiple things in one command:

```bash
$ rcedit "path-to-exe-or-dll" --set-icon "path-to-ico" --set-file-version "10.7"
```

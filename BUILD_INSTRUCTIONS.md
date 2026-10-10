# Building vFPC from Source

vFPC is a C++ plugin for EuroScope that validates IFR departure flight plans against UK route and departure restrictions.

This document explains how to obtain the source code, configure the Windows development environment and prepare the vFPC plugin for building.

For instructions on installing, configuring and using the plugin, refer to the [vFPC User Guide](https://vfpcdata.org/docs).

---

## 1. Obtaining the Source Code

The vFPC 4 source code is maintained in the `vfpc-next` GitHub repository:

https://github.com/VFPC/VFPC-next

### 1.1 Clone the Repository

Cloning the repository is the recommended method for developers because it allows subsequent changes to be retrieved using Git and preserves the repository history.

Install [Git for Windows](https://git-scm.com/downloads/win) if it is not already installed.

Open PowerShell, Command Prompt or Git Bash and navigate to the directory where you want to store the source code. For example:

```powershell
cd C:\Development
```

Clone the repository:

```powershell
git clone https://github.com/VFPC/vfpc-next.git
```

Enter the newly created directory:

```powershell
cd vfpc-next
```

The source code is now available locally.

### 1.2 Download the Source Code

If Git is not required, the repository can also be downloaded as a ZIP archive.

1. Open the `vfpc-next` repository on GitHub.
2. Select **Code**.
3. Select **Download ZIP**.
4. Extract the downloaded archive into a suitable development directory.

Downloading a ZIP does not include the repository's Git history and is less convenient for keeping a local copy up to date.

### 1.3 Selecting a Version

To reproduce a particular release, use the corresponding Git tag or release source archive, where available.

Developers working on new features should use the development branch designated by the project.

The branch or tag should be confirmed before starting a release build.

---

## 2. Development Prerequisites

vFPC is a C++ Windows DLL developed using Microsoft Visual Studio. The plugin is built for the 32-bit (x86) version of EuroScope.

The following tools and components are required to build the plugin from source.

### 2.1 Visual Studio

Install a current version of **Microsoft Visual Studio** with the **Desktop development with C++** workload.

Ensure that the installation includes the C++ build tools and Windows SDK required by the vFPC project.

The project currently uses:

- MSVC v143 C++ build tools.
- A compatible Windows SDK.
- C++20 language support.

Visual Studio Community is suitable for developing and building vFPC, subject to Microsoft's applicable licence terms.

The repository contains `VFPC.sln`, which can be opened directly in Visual Studio.

### 2.2 Git

Git is recommended for obtaining and maintaining a local copy of the source code.

Install [Git for Windows](https://git-scm.com/downloads/win) if you intend to clone the repository.

Git is not required if the source code is downloaded as a ZIP archive.

### 2.3 vcpkg

vFPC uses Microsoft's [vcpkg](https://github.com/microsoft/vcpkg) package manager to obtain its third-party C++ dependencies.

The repository contains a `vcpkg.json` manifest defining the following dependencies:

| Library | Purpose |
| --- | --- |
| `spdlog` | Application logging |
| `fmt` | Text formatting |
| `curl` | HTTP communication |
| `rapidjson` | JSON processing |

The manifest also specifies a vcpkg baseline to provide a consistent dependency configuration.

The Visual Studio project uses vcpkg manifest mode with the following triplet:

```text
x86-windows-static-md
```

This configuration provides the 32-bit dependencies required by the vFPC build.

Third-party licence and copyright information is provided in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

### 2.4 EuroScope SDK

The repository includes the EuroScope plugin SDK header and import library required to build vFPC:

```text
lib/
├── EuroScopePlugIn.h
└── EuroScopePlugInDll.lib
```

These files are included when the repository is cloned or downloaded. No separate EuroScope SDK installation is required for the documented build configuration.

---

## 3. Preparing the Build Environment

Before opening the solution in Visual Studio, install vcpkg and make sure it is available to the development environment.

### 3.1 Check for an Existing vcpkg Installation

If vcpkg may already be installed, open PowerShell and run:

```powershell
where.exe vcpkg
$env:VCPKG_ROOT
```

The first command displays the location of `vcpkg.exe` when it is available on the system path. The second displays the configured `VCPKG_ROOT`, if one has been set.

The vcpkg root directory is the directory containing `vcpkg.exe`.

If an existing installation is correctly configured, it can be used instead of installing another copy.

### 3.2 Install vcpkg

If vcpkg is not already installed, open PowerShell and navigate to a suitable development directory. For example:

```powershell
cd C:\Development
```

Clone the official vcpkg repository:

```powershell
git clone https://github.com/microsoft/vcpkg.git
```

Navigate to the newly created directory and run the bootstrap script:

```powershell
cd vcpkg
.\bootstrap-vcpkg.bat
```

This creates `vcpkg.exe`.

vcpkg only needs to be installed once and can be shared between multiple C++ projects.

### 3.3 Configure Visual Studio Integration

If using a standalone vcpkg installation, Visual Studio integration can be enabled by running the following command from the vcpkg directory:

```powershell
.\vcpkg integrate install
```

This enables user-wide vcpkg integration with Visual Studio.

The vFPC project uses vcpkg manifest mode. The repository's `vcpkg.json` file defines the required libraries, while the Visual Studio project specifies the target triplet.

When manifest integration is enabled, Visual Studio can restore the required dependencies automatically when the project is built.

### 3.4 Optional: Configure `VCPKG_ROOT`

`VCPKG_ROOT` can be used to identify the location of a particular vcpkg installation. It is useful where the development environment or other tooling needs an explicit vcpkg root path, but it is not a requirement imposed directly by the vFPC project.

If you choose to use `VCPKG_ROOT`, set it to the directory containing `vcpkg.exe`.

For example, if vcpkg is installed in `C:\Development\vcpkg`:

```powershell
[Environment]::SetEnvironmentVariable(
    "VCPKG_ROOT",
    "C:\Development\vcpkg",
    "User"
)
```

Restart Visual Studio after changing the environment variable.

The installation directory is not fixed. Substitute the actual location of your own vcpkg installation in the command above.

### 3.5 Restore the Project Dependencies

The `vcpkg.json` manifest specifies the libraries required by vFPC:

- `spdlog`
- `fmt`
- `curl`
- `rapidjson`

The required target triplet is:

```text
x86-windows-static-md
```

With vcpkg manifest integration enabled, Visual Studio can restore the required dependencies automatically when the project is built.

Dependencies can also be installed explicitly from the command line. This can be useful when preparing the development environment in advance or when troubleshooting dependency problems.

From the root of the vFPC repository, run the following command, replacing the example vcpkg path with the location of your own installation:

```powershell
C:\Development\vcpkg\vcpkg.exe install --triplet x86-windows-static-md
```

vcpkg reads `vcpkg.json` and installs the dependencies required by the project.

The initial restore may take some time because vcpkg may need to download and compile the required libraries.

### 3.6 Verify the EuroScope SDK Files

Confirm that the following files are present in the repository:

```text
lib/
├── EuroScopePlugIn.h
└── EuroScopePlugInDll.lib
```

No separate EuroScope SDK download is required.

### 3.7 Check the Development Environment

Before proceeding to the build, confirm that:

- Visual Studio is installed with the **Desktop development with C++** workload.
- The MSVC v143 toolset and a compatible Windows SDK are available.
- vcpkg is installed and available to the development environment.
- `VCPKG_ROOT` is correctly configured if that setup method is being used.
- The required dependencies can be restored for `x86-windows-static-md`.
- `lib/EuroScopePlugIn.h` and `lib/EuroScopePlugInDll.lib` are present in the repository.

The development environment is now ready for the Visual Studio build.

---

## 4. Building vFPC

Once the development environment has been prepared, vFPC can be built using the Visual Studio solution included in the repository.

### 4.1 Open the Solution

Navigate to the root of the local `vfpc-next` repository and open:

```text
VFPC.sln
```

The solution can be opened by double-clicking the file in File Explorer or by selecting **File → Open → Project/Solution** from Visual Studio.

Visual Studio should load the `VFPC` project contained in the solution.

### 4.2 Select the Build Configuration

From the Visual Studio toolbar, select:

```text
Release
x86
```

The solution's `x86` platform maps to the project's `Win32` configuration.

> **Important:** vFPC must be built as a 32-bit (x86) plugin for EuroScope. Do not use the `x64` configuration for the plugin build.

For a normal release build, the selected configuration should therefore appear as:

```text
Release | x86
```

> **Important:** A DLL intended to run in EuroScope must be built using the **Release** configuration. A Debug build will not work correctly with EuroScope.

Debug builds are identified in the DLL properties by the following file description:

```text
VFPC EuroScope Plugin (DEBUG BUILD)
```

A Release build uses:

```text
VFPC EuroScope Plugin
```

### 4.3 Set the Version Information

Before producing a new build, check the version information in:

```text
VersionInfo.h
```

This file is the central source for the plugin version number and the version information embedded in `VFPC.dll`.

The version is defined by:

```cpp
#define VFPC_VERSION_MAJOR 4
#define VFPC_VERSION_MINOR 0
#define VFPC_VERSION_PATCH 0
#define VFPC_VERSION_BUILD 0

#define VFPC_VERSION_PRERELEASE ""
```

Update these values as appropriate before creating a new build.

The four numeric components represent:

```text
MAJOR.MINOR.PATCH.BUILD
```

For example, using the above values gives:

```text
4.0.0.0
```

The prerelease identifier (`VFPC_VERSION_PRERELEASE`) is maintained separately. For a final release it is empty. Development or prerelease versions can use identifiers such as beta3` or `rc1`.

`VersionInfo.h` generates the version strings used by the plugin and by the Windows version resource. This keeps the version reported by vFPC consistent with the version information stored in the DLL.

#### Windows DLL Version Information

The project contains:

```text
Resource.rc
```

This resource script uses the definitions from `VersionInfo.h` to embed version and product information in `VFPC.dll`.

After the DLL has been built, this information can be viewed in Windows by:

1. Right-clicking `VFPC.dll`.
2. Selecting **Properties**.
3. Selecting the **Details** tab.

The displayed information includes the file description, file version, product name, product version, original filename and other product metadata.

> **Important:** Update `VersionInfo.h` before producing a new development or release build. Do not edit the version values independently in `Resource.rc`.

### 4.4 Protect an Existing vFPC Installation

The project is configured to write the completed `VFPC.dll` directly to:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\
```

This is the vFPC plugin directory used by the UK EuroScope installation. It should not be confused with other plugin directories that may exist within the EuroScope installation.

> **Warning:** Building vFPC will overwrite an existing `VFPC.dll` in this directory.

If you want to retain the currently installed version, copy the existing DLL to a backup location before starting the build.

> **Important:** Close EuroScope before building vFPC. While EuroScope is running, the loaded `VFPC.dll` is in use and Visual Studio will be unable to replace it. This will cause the build to fail.

### 4.5 Build the Solution

From the Visual Studio menu, select:

**Build → Build Solution**

Alternatively, use:

```text
Ctrl+Shift+B
```

Visual Studio will compile the vFPC source code, link the required libraries and create `VFPC.dll`.

On a successful build, the DLL is written to:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

The **Output** window can be used to follow the build process.

A successful build should finish without compilation or linker errors and report that the `VFPC` project was built successfully.

### 4.6 Rebuild the Solution

When preparing a release, or when a clean build is required, use:

**Build → Rebuild Solution**

A rebuild removes the existing compiled objects for the selected configuration and recompiles the complete project.

This is recommended when producing a release build because it confirms that the resulting DLL does not depend on compiled objects left over from an earlier build.

### 4.7 Build Errors

If the build fails, review the Visual Studio **Error List** and **Output** windows.

Common causes include:

- EuroScope is still running and has `VFPC.dll` in use;
- vcpkg is not installed or cannot be located;
- the required vcpkg packages have not been installed for `x86-windows-static-md`;
- the wrong build platform has been selected;
- `EuroScopePlugIn.h` or `EuroScopePlugInDll.lib` is missing from the repository;
- the required MSVC toolset or Windows SDK is not installed.

Do not proceed until the solution builds without errors.

---

## 5. Verifying the Build

After Visual Studio reports a successful build, verify the generated DLL before using it for normal EuroScope operation or distributing it as a release.

### 5.1 Confirm the Build Result

Check the Visual Studio **Output** window and confirm that the build completed successfully with no compilation or linker errors.

For a release build, the build configuration should be:

```text
Release | x86
```

The solution's `x86` platform maps to the project's `Win32` configuration.

### 5.2 Confirm the DLL

The completed plugin should be located at:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

In File Explorer, enter the following into the address bar to open the directory directly:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\
```

Confirm that `VFPC.dll` exists and that its **Date modified** corresponds to the build you have just completed.

If the date and time have not changed, do not assume that the new DLL was built or copied successfully. Check the Visual Studio build output for errors or an unexpected output location.

### 5.3 Check the DLL Version

The version information defined in `VersionInfo.h` is embedded into the DLL by the project's Windows resource.

To verify it:

1. Right-click `VFPC.dll`.
2. Select **Properties**.
3. Select the **Details** tab.
4. Check the displayed **File version** and **Product version**.

Confirm that the displayed version corresponds to the version specified in `VersionInfo.h`.

Also check the **File description**. A DLL intended to run in EuroScope should show:

```text
VFPC EuroScope Plugin
```

If the File description is:

```text
VFPC EuroScope Plugin (DEBUG BUILD)
```

the DLL was built using the Debug configuration and should not be used with EuroScope. Return to Visual Studio, select `Release | x86` and rebuild the solution.

The Details tab also provides information such as the product name, original filename and copyright metadata.

If the version is not the version expected for the build, check `VersionInfo.h`, update it if necessary, and rebuild the solution.

### 5.4 Load the Plugin in EuroScope

Start EuroScope and load vFPC in the normal way.

If the plugin was already configured in EuroScope and the DLL has been replaced at the same location, the existing plugin configuration should continue to reference:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

Confirm that the plugin loads without an error.

Detailed instructions for installing, configuring and using vFPC are provided in the [vFPC User Guide](https://vfpcdata.org/docs/).

### 5.5 Confirm the Running Version

When vFPC is loaded, it displays its version information in the EuroScope message area.

The message has the following form:

```text
VFPC: Plugin Version: <version> Loaded. Welcome to the (UK) VATSIM Flight Plan Checker
```

Confirm that the version shown in this message corresponds to the version configured in `VersionInfo.h`.

The `<version>` value reflects the version information generated from `VersionInfo.h`, including the prerelease label and build number where applicable.

This provides a final check that EuroScope has loaded the newly built version of `VFPC.dll`.

If the version displayed in EuroScope does not match the expected version:

1. Check the version information in `VersionInfo.h`.
2. Check the version shown under **Properties → Details** for the built `VFPC.dll`.
3. Confirm that EuroScope is configured to load:

   ```text
   %APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
If necessary, close EuroScope, rebuild the plugin and restart EuroScope.

---

## 6. Troubleshooting the Build

This section covers common problems encountered when building vFPC from source.

For problems encountered when installing, configuring or using vFPC in EuroScope, refer to the [vFPC User Guide](https://vfpcdata.org/docs/).

### 6.1 Build Fails Because vcpkg Cannot Be Found

If Visual Studio cannot locate vcpkg, check the installation from PowerShell:

```powershell
where.exe vcpkg
$env:VCPKG_ROOT
```

Confirm that `VCPKG_ROOT`, where used, points to the directory containing `vcpkg.exe`.

If the environment variable has recently been added or changed, close and restart Visual Studio before attempting another build.

### 6.2 Missing vcpkg Dependencies

The project requires the dependencies defined in `vcpkg.json`:

- `spdlog`
- `fmt`
- `curl`
- `rapidjson`

The required triplet is:

```text
x86-windows-static-md
```

If Visual Studio reports missing headers or libraries, confirm that the dependencies have been installed for this triplet.

From the root of the vFPC repository, run:

```powershell
<path-to-vcpkg>\vcpkg.exe install --triplet x86-windows-static-md
```

Replace `<path-to-vcpkg>` with the location of your vcpkg installation.

### 6.3 Wrong Build Configuration or Platform

A DLL intended to run in EuroScope must be built using the **Release** configuration and as a 32-bit plugin.

Visual Studio should show:

```text
Release | x86
```

The solution's `x86` configuration maps to the project's `Win32` configuration.

If `Debug` is selected, change the configuration to `Release`. If `x64` is selected, change the platform to `x86`, then rebuild the solution.

A Debug DLL can also be identified under **Properties → Details** by the file description:

```text
VFPC EuroScope Plugin (DEBUG BUILD)
```

### 6.4 EuroScope SDK Files Cannot Be Found

The repository includes the EuroScope SDK files required by the project.

Confirm that the following files exist:

```text
lib\EuroScopePlugIn.h
lib\EuroScopePlugInDll.lib
```

If either file is missing, confirm that the repository was cloned or extracted completely before attempting to modify the Visual Studio project configuration.

### 6.5 Linker Errors

Linker errors can occur when the wrong architecture or dependency configuration is being used.

Check that:

- the build platform is `x86`;
- the vcpkg triplet is `x86-windows-static-md`;
- the required vcpkg dependencies are installed;
- `EuroScopePlugInDll.lib` is present in the `lib` directory.

After correcting the problem, use:

**Build → Rebuild Solution**

to perform a complete rebuild.

### 6.6 Build Cannot Replace `VFPC.dll`

If the compilation succeeds but the build fails when attempting to create or replace `VFPC.dll`, check that EuroScope is not running.

EuroScope loads the plugin DLL and keeps the file in use while the application is running. Windows therefore prevents Visual Studio from overwriting the file.

1. Close EuroScope.
2. Confirm that EuroScope has completely exited.
3. In Visual Studio, select **Build → Rebuild Solution**.

The build should then be able to replace:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

If the DLL still cannot be replaced, check that no other running process is using the file.

### 6.7 `VFPC.dll` Is Not Updated

A successful build should update:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

If the DLL has not been updated:

1. Check the Visual Studio **Output** window and confirm that the build completed successfully.
2. Confirm that `Release | x86` is selected.
3. Check the **Date modified** value of `VFPC.dll`.
4. Check that the build output has not been redirected to another location.

Do not assume that the new plugin has been produced simply because an existing `VFPC.dll` is present.

### 6.8 EuroScope Loads an Unexpected Version

If EuroScope starts successfully but the version displayed in the message area is not the expected version, first check the version embedded in the DLL:

1. Right-click `VFPC.dll`.
2. Select **Properties → Details**.
3. Check the displayed version information.

Then confirm that EuroScope is loading:

```text
%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\VFPC.dll
```

The version displayed by EuroScope should correspond to the version configured in `VersionInfo.h`.

If the DLL properties show the expected version but EuroScope reports a different version, close EuroScope and check that it is not configured to load another copy of `VFPC.dll` from a different plugin directory.

### 6.9 Problems Persist After Correcting the Build Configuration

After correcting a build configuration or dependency problem, perform a complete rebuild using:

**Build → Rebuild Solution**

If the problem remains, use the Visual Studio **Output** window rather than only the **Error List** when investigating the failure. The Output window normally provides the compiler or linker command and the detailed error that caused the build to fail.

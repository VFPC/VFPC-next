# Reporting an Issue

If you are unable to resolve a problem using the
[Troubleshooting and Limitations](troubleshooting.md) page, support is
available through the vFPC Discord server.

## Getting Support

### Joining the vFPC Discord Server

Use the link below to join the vFPC Discord server:

[Join the vFPC Discord server](https://discord.gg/zcgkBfayDz)

If you do not already have a Discord account, you will be prompted to create
one or sign in before joining the server.

### Raising a Support Ticket

Once you have joined the server, open a support ticket in the appropriate
support channel.

## Information to Provide

When reporting a problem, provide as much of the following
information as applicable:

- vFPC version.
- EuroScope version.
- Callsign.
- Departure and destination aerodromes.
- Assigned departure runway.
- SID.
- EOBT.
- Aircraft type.
- Requested flight level.
- Filed route.
- The result displayed in the **FPL** column.
- The **SUMMARY** result.
- The **FULL** result, where relevant.
- What result you expected.
- The relevant vFPC log file.
- The startup log entry showing the version and build identification.

## Using the vFPC Log File

vFPC creates a new log file each time the plugin is started. The log contains
diagnostic information about the operation of vFPC and flight plan validation.

The log files are normally located in:

`%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\log`

Log files use the following naming format:

`vfpc-YYYYMMDDHHMMSS.log`

The log file can be useful when investigating an unexpected validation result
or other problem with vFPC.

When reporting a problem, you may be asked to attach the log file covering the
period when the problem occurred.

### Build and Validation Information in Logs

The startup entry identifies the plugin version, numeric DLL revision, build
configuration and source/build identity. Include that entry when reporting a
problem so support can distinguish the build being used.

Logs also record validation results when airport data becomes available, not
only when the flight is first seen. Airport-data load and lifecycle information
helps support investigate loading or shutdown problems. You do not need to
interpret the memory or source-identity fields yourself; provide the relevant
log when requested.

### Detailed Logging

vFPC supports additional diagnostic logging for investigating problems.

Detailed logging should normally only be enabled when requested by vFPC
support. Instructions for enabling it will be provided if required.

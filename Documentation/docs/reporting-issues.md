# Reporting an Issue

If you are unable to resolve a problem using the
[Troubleshooting and Limitations](troubleshooting.md) page, support is
available through the vFPC Discord server.

## Getting Support

### Joining the vFPC Discord Server

Use the link below to join the vFPC Discord server:

[Join the vFPC Discord server](https://discord.gg/ff8fsG3k)

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
- EOBT.
- Aircraft type.
- Requested flight level.
- Filed route.
- The result displayed in the **FPL** column.
- The **SUMMARY** result.
- The **FULL** result, where relevant.
- What result you expected.
- The relevant vFPC log file.

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

### Detailed Logging

vFPC supports additional diagnostic logging for investigating problems.

Detailed logging should normally only be enabled when requested by vFPC
support. Instructions for enabling it will be provided if required.

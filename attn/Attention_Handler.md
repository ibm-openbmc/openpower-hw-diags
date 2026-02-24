# Attention Handler

## Introduction

An attention is a hardware, firmware or software alert mechanism used to request
service from an Attention Handler via an attention signal (e.g. a GPIO). An
attention handler is, in this case, a stateless service that handles attention
requests. The attention handler in combination with a hardware analyzer
constitutes the Openpower Hardware Diagnostics (hardware diags) component.
Hardware diags can operate in daemon mode and in application mode. When
operating in daemon mode openpower hardware diags may be referred to as the
attention handler or attention handler service. When operating in application
mode openpower hardware diags may be referred to as the analyzer. Both modes
executing simultaneously is supported however. There can be at most one instance
of the attention handler service, per attention signal.

## Overview

The main role of the attention handler is a long running process monitoring the
attention interrupt signal and delegating tasks to external components to aid in
handling attention requests. The attention handler is loaded into memory when
the host is started and monitors the attention signal until the host is stopped.
When an attention (signal) is detected by the attention handler (monitor) the
attention handler will query the state of the host to determine the nature of
the active attention(s).

The attention handler services four types of attentions namely, and in order of
priority, Vital Attention (vital), Terminate Immediately (TI), Breakpoint
Attention (BP) and Checkstop Attention (checkstop). TI and BP attentions are
categorically called Special Attentions (special) and are mutually exclusive.
Additionally a TI can be raised by either hostboot (HBTI) or the hypervisor
(PHYPTI).

The general handling of attentions is as follows:

- vital: log an event, request a SBE dump and request a re-ipl of the host.
- PHYPTI: log an event and request a memory preserving IPL (MPIPL) of the host.
- HBTI: log an event, request a hardware dump and request a re-ipl of the host.
- BP: notify the debug agent (e.g. Cronus)
- checkstop: log an event, call the analyzer, request a system dump and request
  a re-ipl of the host.

## Implementation Details

### External Components

The attention handler relies on several external components and BMC services for
the handling of attentions. Among these are systemd, dbus, phosphor-logging and
PDBG.

The general use of these components are as follows:

- systemd: loading and unloading of the attention handler service.
- dbus: querying the state of the BMC and host, requesting dumps and requesting
  host IPL's
- phosphor-logging: debug tracing and logging platform events.
- PDBG: querying host hardware.

### Component Usage

- Platform Event Log (PEL) entry creation and debug tracing: phosphor-logging
  component.
- Host Transition (re-IPL, MPIPL request): dbus interface.
- Attention handler starting and stopping: systemd.

### Starting and Stopping

The attention handler service is started via the BMC systemd infrastructure when
the host is started. When the attention handler is started it will register
itself as the GPIO event handler associated with the attention GPIO and remain
resident in memory waiting for attentions. The attention handler will be stopped
via the BMC systemd infrastructure when the host is stopped.

If the attention handler terminates due to an error condition it will
automatically be restarted by systemd. The attention handler restart policy is
based on the BMC default restart policy (n-number of restarts in n-seconds
maximum).

### Attentions

When the attention signal becomes active the attention handler will begin
handling attentions. Using the PDBG interface the attention handler will query
the host to determine the reason for the active attention handler signal. Each
enabled processor will be queried and a map of active attentions will be
created. The first attention of highest priority will be the one that is
serviced (see Overview for attention type and priority). The Global Interrupt
Status Register in combination with an associated attention mask register is
used to determine active attentions.

#### Vital Attention

A vital attention is handled by generating an event log via the phosphor-logging
interface and then using the dump manager dbus interface to request a hardware
dump. The attention handler will wait, up to one hour, for the dump to complete
and then use the systemd interface to request a re-IPL of the host. This type of
attention indicates that there was a problem with the Self Boot Engine (SBE)
hardware.

#### Special Attentions

Three types of attentions, HBTI, PHYPTI and BP, share a single attention status
flag (special attention) in the global interrupt status register. In order to
determine the specific type of special attention the attention handler relies on
a portion of shared memory called the Shared TI Information Data Area (TI info).
The attention handler gains access to this memory region by submitting a request
for its memory address through the PDBG interface. The request is via a Chip
Operation (chipop) command.

##### HBTI

These attention types are indications from hostboot that an error has occurred
or a shutdown has been requested. When servicing a HBTI the attention handler
will consider two cases namely a TI with SRC and a TI with EID.

For a TI with SRC, using the TI info, the attention handler will create and
submit a PEL entry on behalf of hostboot, request a hostboot dump and upon
completion, or timeout, request a re-IPL of the host using the dbus interface.
Optionally, based upon flags in the TI info, the attention handler may mark the
PEL entry as hidden.

For a TI with EID, the attention handler will, based upon flags in the TI info,
request a hostboot dump using the dbus interface. Upon completion, or timeout,
of the dump the attention handler will request a re-IPL of the host via the dbus
interface. For a TI with EID the attention handler will forgo creating a PEL
entry (hostboot has already done this).

##### PHYPTI

These attentions are indications from the hypervisor that an error has occurred.
When servicing a PHYPTI the attention handler will generate a PEL entry and then
request a host MPIPL using the PDBG interface (chipop).

##### BP (breakpoint)

These attentions are used to signal to the attention handler that it should
notify the debug agent (e.g. Cronus) that a debug breakpoint has been
encountered. If the TI info data is considered valid and neither a HBTI or
PHYPTI is determined to be the cause of the special attention then a BP
attention is assumed. When a BP attention occurs the attention handler will use
the dbus interface to notify the debug agent that a breakpoint condition was
encountered. The attention handler will then go back to listening for
attentions.

##### Recoverables

These attentions are indications that recoverable errors have been encountered.
These attentions do not generate an attention GPIO event however when servicing
a TI the attention handler will note whether or not the host is reporting
recoverable errors. If recoverable errors are present the attention handler will
call the analyzer before requesting a dump or IPL. The check for recoverable
errors is done using the PDBG interface (the Global Interrupt Status Register).

#### Checkstop

These attentions indicate that a hardware error has occurred and further
hardware analyses is required to determine root cause. When servicing a
checkstop attention the attention handler will call the analyzer and then wait
for analyses to complete. Once analyses have completed the attention handler
will use the dbus interface to request a system dump and upon completion, or
timeout, will request a re-IPL of the host.

### Configuration

Some attention handler behavior can be configured by passing parameters to the
service using the service file or using the command line. Currently the
following behaviors are configurable:

- vital handling enable/disable, default enable
- TI handling enable/disable, default enable
- BP handling enable/disable, default enable
- checkstop handling enable/disable, default enable
- special attention default BP/TI, default BP

### Additional Considerations

Regardless of the type of special attention the attention handler will always
create at least one PEL entry containing attention handler specific FFDC. In
some cases this entry may be informational.

Regardless of the type of attention, after servicing the attention the attention
handler will return to listening for attentions. In most cases no more
attentions will be detected unless the currently active attentions are cleared.
Normally active attentions will be cleared by the host re-IPL but any component
could clear the active attentions as is the case for BP attentions and the debug
agent.

If the attention handler encounters any errors while servicing an interrupt it
will generate a PEL entry to indicate the error that was encountered.

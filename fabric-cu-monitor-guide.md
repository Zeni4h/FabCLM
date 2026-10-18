# Microsoft Fabric CU Monitor

**Step-by-Step Guide to Building a Secure C++ CLI for Fabric Capacity Monitoring**

| | |
|---|---|
| **Version** | 1.0 |
| **Last updated** | October 2026 |
| **Language** | C++20 |
| **Target platform** | Windows or Linux |
| **Cloud platform** | Microsoft Fabric and Microsoft Entra ID |

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Understanding Fabric Capacity Metrics](#2-understanding-fabric-capacity-metrics)
3. [Choose the Architecture](#3-choose-the-architecture)
4. [Prerequisites](#4-prerequisites)
5. [Configure Microsoft Fabric](#5-configure-microsoft-fabric)
6. [Register an Application in Microsoft Entra ID](#6-register-an-application-in-microsoft-entra-id)
7. [Configure Permissions and Security](#7-configure-permissions-and-security)
8. [Configure the Eventstream](#8-configure-the-eventstream)
9. [Choose How the C++ CLI Consumes Data](#9-choose-how-the-c-cli-consumes-data)
10. [Set Up the C++ Development Environment](#10-set-up-the-c-development-environment)
11. [Design the CLI Application](#11-design-the-cli-application)
12. [Calculate CU Consumption](#12-calculate-cu-consumption)
13. [Implement Authentication Securely](#13-implement-authentication-securely)
14. [Build and Run the Application](#14-build-and-run-the-application)
15. [Testing and Validation](#15-testing-and-validation)
16. [Security Checklist](#16-security-checklist)
17. [Troubleshooting](#17-troubleshooting)
18. [Recommended Development Roadmap](#18-recommended-development-roadmap)
19. [Official Documentation](#19-official-documentation)
20. [Final Recommendation](#final-recommendation)

---

## 1. Project Overview

The objective is to build a native command-line application that displays Microsoft Fabric capacity consumption without requiring the user to open the Power BI Capacity Metrics app.

The application should eventually support commands such as:

```bash
fabric-cu status
fabric-cu top --limit 10
fabric-cu history --minutes 60
fabric-cu watch
fabric-cu workspace "Data Engineering"
```

> **Note:** The exact command-line interface is a design proposal. These commands do not exist until implemented.

### 1.1 Planned features

The initial version should display:

- Capacity name and identifier.
- Capacity SKU and base capacity units.
- Recent capacity utilization.
- Capacity-level consumption trends.
- Top CU-consuming operations.
- Workspace and item identifiers.
- Operation duration and status.
- Throttling information when available.

Later versions can add:

- Multiple-capacity support.
- CSV and JSON exports.
- Historical data storage.
- Configurable thresholds.
- Desktop notifications.
- Alerts for unusually expensive operations.
- Usage summaries by workspace or item.

### 1.2 Important limitation

The first version will not necessarily reproduce every calculation in Microsoft's Capacity Metrics app.

The application must distinguish between:

- Raw CU consumption reported by events.
- Utilization calculated by the CLI.
- Official capacity utilization metrics.
- Capacity overload and throttling states.

These metrics are related, but they are not interchangeable.

---

## 2. Understanding Fabric Capacity Metrics

Microsoft Fabric capacity provides compute resources measured in Capacity Units (CUs).

For example, an F64 capacity has 64 base CUs.

The official Capacity Metrics app displays consumption across Fabric workloads, including semantic models, warehouses, notebooks, pipelines, and other supported workloads.

However, its internal semantic model is intended for the reports supplied with the application. It should not be treated as a supported general-purpose data API.

For a custom monitoring application, use an officially supported monitoring interface instead.

### 2.1 Capacity overview events

Capacity overview events provide aggregated capacity-level information.

Microsoft documents a summary event emitted every 30 seconds:

```text
Microsoft.Fabric.Capacity.Summary
```

The overview event source can be used to monitor capacity utilization and trends.

A separate capacity state event reports state changes, including relevant overload and throttling conditions.

Use overview events when the primary requirement is:

- Displaying overall capacity consumption.
- Tracking utilization over time.
- Showing capacity health.
- Creating high-level alerts.

**Official documentation:** Fabric capacity overview events

### 2.2 Capacity operation events

Operation events provide more granular information about operations that consume capacity.

The documented event type is:

```text
Microsoft.Fabric.CapacityOperationEvents.Operation
```

Depending on the event schema, these events expose information such as:

- Capacity ID.
- Workspace ID.
- Item ID and type.
- Operation ID.
- Operation duration.
- CU consumption.
- Operation status.
- Throttling delay.

Use operation events when the objective is to identify expensive workloads.

**Official documentation:** Fabric capacity operation events

> **Recommendation:** Start with overview events for the capacity `status` command. Add operation events when implementing the `top` (top-consumers) command.

---

## 3. Choose the Architecture

There are two practical approaches to getting monitoring data into a C++ CLI.

### 3.1 Architecture A: Eventstream and a direct consumer

```text
Microsoft Fabric Capacity
           |
           v
Capacity Overview Events
           |
           v
      Fabric Eventstream
           |
           v
 Supported Consumer Endpoint
           |
           v
       C++ CLI
           |
           v
   Terminal / JSON / CSV
```

The CLI consumes data through a supported endpoint configured for the Eventstream.

This approach is appropriate when you want relatively recent data without building a separate historical database.

However, the exact connection protocol and authentication requirements must be verified for the selected Eventstream endpoint.

> **Warning:** Do not assume that the native capacity event source exposes a directly pollable REST endpoint.

### 3.2 Architecture B: Eventstream, persistent storage, and a CLI

```text
Microsoft Fabric Capacity
           |
           v
Capacity Overview / Operation Events
           |
           v
      Fabric Eventstream
           |
           v
     Lakehouse or Eventhouse
           |
           v
 Supported Query Interface
           |
           v
          C++ CLI
           |
           v
   Terminal / JSON / CSV
```

This approach adds persistent storage.

It makes it easier to implement:

- Historical queries.
- Daily and hourly summaries.
- Top consumers over a time range.
- Comparisons between workspaces.
- Long-term monitoring.
- Usage reporting.

The CLI would query the storage layer through a supported interface rather than trying to maintain its own permanent connection to the live event stream.

### 3.3 Which approach should you choose?

| Requirement | Recommended approach |
|---|---|
| Display recent capacity events | Architecture A |
| Show top-consuming operations | Architecture A or B |
| Query the previous 24 hours | Architecture B |
| Keep long-term history | Architecture B |
| Build reports across multiple capacities | Architecture B |
| Minimize infrastructure | Architecture A |

For a first implementation, I recommend proving that the events can be consumed successfully before adding historical storage.

---

## 4. Prerequisites

Before starting, make sure you have:

- A Microsoft Fabric tenant.
- Access to a Fabric workspace.
- A Fabric capacity for which you have the necessary administrative permissions.
- Permission to create and configure an Eventstream.
- Access to the Microsoft Entra admin center.
- Approval to register an application, if required by your organization.
- A C++20 compiler.
- CMake.
- Git.
- An HTTP library and JSON library for the application.

The capacity operation event source has documented workspace and capacity permission requirements. Confirm that your account satisfies them before configuring the event source.

**Official prerequisites:** Configure Fabric capacity operation events

### 4.1 Recommended C++ dependencies

| Dependency | Purpose |
|---|---|
| C++20 compiler | Build the application |
| CMake | Configure the build |
| libcurl | HTTP communication |
| nlohmann/json | JSON parsing |
| Microsoft Authentication Library or supported OAuth implementation | Authentication |
| librdkafka, if using a compatible Kafka endpoint | Kafka protocol communication |
| SQLite, optionally | Local historical storage |

Not every dependency is needed for every architecture.

For example, if the chosen Eventstream endpoint uses Kafka, you will need a compatible Kafka client and its supported authentication configuration.

If the application queries a storage endpoint using HTTPS, you may not need a Kafka client.

---

## 5. Configure Microsoft Fabric

This section configures the data source.

### Step 1: Sign in to Microsoft Fabric

Open the Microsoft Fabric portal and sign in using an account that has the required access.

Navigate to the workspace where you want to create the monitoring infrastructure.

The workspace must meet the licensing and permission requirements for the selected Eventstream functionality.

### Step 2: Open Real-Time hub

Navigate to **Real-Time hub**.

Find the **Fabric events** section.

Look for the capacity event sources available in your tenant.

Depending on your requirements, select:

- Fabric capacity overview events.
- Fabric capacity operation events.

Availability and capabilities may differ by feature status, region, and tenant configuration.

### Step 3: Create an Eventstream

Select the capacity event source and start creating an Eventstream.

Configure the following:

1. Select the event type.
2. Select the Fabric capacity to monitor.
3. Select the destination workspace.
4. Give the Eventstream a meaningful name.
5. Review the configuration.
6. Create the Eventstream.
7. Publish it.

For example:

```text
fabric-cu-monitoring
```

The operation-event source requires capacity administrator permissions for the selected capacity.

**Official walkthrough:** Create capacity operation event streams

### Step 4: Verify that events are arriving

Open the Eventstream and inspect its live view.

Verify that the source is connected and that events are arriving.

Do not begin implementing the C++ consumer until you have confirmed that the source works.

This isolates Fabric configuration problems from application problems.

### Step 5: Choose a consumer or destination

Determine how the application will retrieve the events.

Possible approaches include:

- A supported custom endpoint.
- A compatible Kafka endpoint, where available and appropriate.
- A Lakehouse destination.
- An Eventhouse destination.

The correct choice depends on whether you want live consumption or historical querying.

Record the connection details, authentication method, endpoint hostname, and relevant permissions.

> **Warning:** Do not put passwords, access tokens, or client secrets in the source repository.

---

## 6. Register an Application in Microsoft Entra ID

An application registration provides an identity for the CLI when the selected authentication architecture requires one.

For unattended operation, a service principal is often appropriate.

For an interactive, single-user CLI, delegated user authentication may be preferable if the selected data interface supports it.

> **Important:** Registering an application does not automatically grant it permission to read Fabric capacity events. The permissions depend on the selected data source and connection mechanism.

### Step 1: Open the Microsoft Entra admin center

Open the **Microsoft Entra admin center**.

Navigate to:

**Identity → Applications → App registrations**

Select **New registration**.

### Step 2: Configure the application

Use a descriptive name, for example:

```text
Fabric CU Monitor CLI
```

Choose the supported account type according to your organization's requirements.

For an organization-specific tool, a single-tenant registration is usually the simplest choice.

A single-tenant registration restricts the application's organizational sign-in audience. It does not, by itself, restrict access to a particular Fabric capacity.

Configure a redirect URI only if required by the chosen authentication flow.

For a client-credentials flow, a redirect URI is generally unnecessary.

Select **Register**.

### Step 3: Record the identifiers

On the application's Overview page, record:

- Application (client) ID.
- Directory (tenant) ID.

These values identify the application and tenant. They are not passwords.

Example configuration:

```bash
FABRIC_TENANT_ID=<tenant-id>
FABRIC_CLIENT_ID=<application-client-id>
```

Do not use the example placeholders as real identifiers.

### Step 4: Choose the authentication method

There are two common approaches.

#### Option A: Interactive user authentication

The user signs in through a supported authentication flow.

**Advantages:**

- No client secret needs to be distributed.
- User access can be revoked through normal account controls.
- Suitable for a developer-operated CLI.

**Disadvantages:**

- Requires user interaction.
- Access depends on the user's permissions.
- Token acquisition and caching must be implemented securely.

#### Option B: Service principal authentication

The application authenticates as itself.

**Advantages:**

- Suitable for unattended monitoring.
- Does not require a user to sign in each time.
- Easier to integrate with scheduled jobs.

**Disadvantages:**

- Requires careful credential management.
- Requires the appropriate tenant settings and resource permissions.
- A client secret is a sensitive credential if that authentication method is used.

For a service principal, configure only the permissions and access actually required by the chosen data interface.

---

## 7. Configure Permissions and Security

This is one of the most important parts of the implementation.

An application registration, an OAuth token, and Fabric resource permissions are separate pieces of the security model.

You need to configure all applicable layers.

### 7.1 Configure tenant settings when required

If your chosen integration uses a service principal to access Fabric APIs or an Entra-authenticated Eventstream endpoint, review the relevant Fabric tenant settings.

In the Fabric portal:

1. Open **Settings**.
2. Open **Admin portal**.
3. Navigate to **Tenant settings**.
4. Find the relevant service-principal setting.
5. Configure the setting according to the official instructions and your organization's policy.

Microsoft documents service-principal access to Fabric APIs for the applicable custom endpoint scenarios.

**Official documentation:** Connect to Eventstream using Microsoft Entra ID authentication

Do not enable broad tenant-wide access merely to make a prototype work.

Where supported, restrict the setting to an approved security group containing the application or identities that require access.

### 7.2 Grant workspace access when required

If the selected endpoint or API requires workspace permissions:

1. Open the target Fabric workspace.
2. Select **Manage access**.
3. Add the application or service principal.
4. Assign the minimum role required for the integration.
5. Verify that the application can access the intended resource.

Microsoft's custom endpoint guidance describes granting workspace access to the application.

However, do not blindly assign Contributor permissions to every service principal. The minimum required role depends on the operation and resource being accessed.

For read-only monitoring, avoid giving the CLI permission to modify workspaces or deploy Fabric items unless that functionality is genuinely required.

### 7.3 Configure API permissions carefully

Open the application registration and inspect API permissions.

Do not add arbitrary Fabric or Power BI permissions just because they sound relevant.

Instead:

1. Identify the actual API or endpoint that the CLI will call.
2. Read its authentication requirements.
3. Determine whether it supports delegated permissions, application permissions, or both.
4. Configure the required permissions.
5. Obtain administrator consent if required.
6. Verify the resulting token audience and claims.

Some event-stream access mechanisms use endpoint-specific authentication and authorization rather than ordinary Fabric REST API permissions.

The OAuth token must be issued for the resource expected by the selected endpoint.

A valid token for the wrong resource will not work.

### 7.4 Prefer a certificate or managed identity when appropriate

If the application runs on an Azure resource that supports managed identities and the selected integration supports that identity type, consider using a managed identity.

Otherwise, consider certificate-based client authentication for an unattended application where supported.

A client secret may be suitable for development, but it should not be embedded in a compiled executable or committed to Git.

### 7.5 Least-privilege design

The application should be able to read the monitoring data it needs and nothing more.

Recommended controls:

- Use a dedicated application identity.
- Restrict the application to the required tenant.
- Limit workspace access.
- Avoid granting administrative roles to the application.
- Do not grant capacity administration privileges to the CLI identity unless the integration explicitly requires them.
- Keep development and production identities separate.
- Review application permissions periodically.
- Revoke unused credentials promptly.

Remember that the identity used to configure the capacity event source may have different requirements from the identity used by the C++ consumer.

---

## 8. Configure the Eventstream

The Eventstream is the bridge between Fabric monitoring events and the consumer.

The exact steps depend on the selected endpoint.

### Option A: Use a supported custom endpoint

A custom endpoint can allow an application to send or fetch data from an Eventstream.

Microsoft documents support for SAS-key and Microsoft Entra ID authentication for applicable custom endpoints.

If using Entra ID authentication:

1. Create the Eventstream.
2. Add the appropriate custom endpoint.
3. Enable the authentication mode supported by that endpoint.
4. Configure the required identity and permissions.
5. Record the endpoint's connection details.
6. Test the connection using Microsoft's documented approach.
7. Only then integrate the endpoint into the C++ application.

**Documentation:** Add a custom endpoint or custom app to an Eventstream

> A custom endpoint is not interchangeable with every other Eventstream source or destination. Verify that the selected endpoint can consume the data in the direction required by your design.

### Option B: Use a compatible Kafka endpoint

Fabric Eventstreams support Kafka endpoints for supported scenarios.

If you choose this approach, confirm:

- The endpoint is available for the Eventstream.
- The endpoint supports consuming the intended stream.
- The required Kafka protocol and authentication mode are supported.
- Your C++ Kafka client supports that protocol and authentication mode.
- The application can obtain the correct Entra ID token or other required credentials.

A typical C++ Kafka client is librdkafka.

**Documentation:** Microsoft Fabric Eventstreams overview

> Do not assume that the Kafka endpoint's broker hostname, OAuth audience, SASL configuration, and topic name can be inferred from the Eventstream name. Obtain them from the actual endpoint configuration and current documentation.

### Option C: Persist events in a Lakehouse or Eventhouse

If you want historical usage, route events to an appropriate destination.

This can separate ingestion from querying:

```text
Capacity Events
      |
      v
  Eventstream
      |
      v
  Persistent Storage
      |
      v
 Supported Query Interface
      |
      v
     C++ CLI
```

The CLI can then query a time range and calculate summaries.

This is generally more convenient than expecting a live stream to provide arbitrary historical queries.

---

## 9. Choose How the C++ CLI Consumes Data

Before writing the consumer, document the interface.

For example:

```text
Data source:
  Fabric Eventstream

Consumption method:
  Supported custom endpoint or Kafka endpoint

Authentication:
  Microsoft Entra ID

Consumer identity:
  Dedicated application identity

Data format:
  JSON, subject to the actual event schema

Persistence:
  None in the initial prototype
```

Do not begin by assuming that a public Fabric REST API returns the same historical CU data as the Capacity Metrics app.

A capacity-management API may provide capacity metadata without providing the detailed usage history needed by the CLI.

The source must be verified against the requirements.

### Recommended implementation order

1. Configure the Eventstream.
2. Confirm that the source emits data.
3. Connect using the documented consumer mechanism.
4. Authenticate successfully.
5. Read one event.
6. Save a sample event locally with sensitive values removed.
7. Implement JSON parsing.
8. Implement the terminal output.
9. Add aggregation.
10. Add error handling and reconnection logic.

This sequence helps isolate configuration problems from code defects.

---

## 10. Set Up the C++ Development Environment

Use C++20 and CMake.

A suggested project layout:

```text
fabric-cu/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── config/
│   └── config.example.json
├── include/
│   ├── auth.hpp
│   ├── consumer.hpp
│   ├── event.hpp
│   ├── metrics.hpp
│   └── cli.hpp
├── src/
│   ├── main.cpp
│   ├── auth.cpp
│   ├── consumer.cpp
│   ├── event.cpp
│   ├── metrics.cpp
│   └── cli.cpp
├── tests/
│   ├── test_event.cpp
│   └── test_metrics.cpp
└── samples/
    └── sample-event.json
```

The files are a proposed project structure. They need to be created as part of the implementation.

### 10.1 Install the compiler

On Windows, you can use Visual Studio with the C++ workload or an appropriate MinGW toolchain.

On Linux, install a supported compiler and CMake using your distribution's package manager.

Verify:

```bash
cmake --version
```

Also verify that your compiler supports C++20.

### 10.2 Install libraries

Use a dependency manager such as vcpkg or Conan, or use your operating system's package manager.

Recommended libraries:

- libcurl for HTTP.
- nlohmann/json for JSON.
- A supported authentication library or OAuth implementation.
- librdkafka if using a compatible Kafka endpoint.

Use a maintained authentication library rather than implementing OAuth token acquisition from scratch.

---

## 11. Design the CLI Application

Separate authentication, transport, event parsing, calculations, and terminal output.

This makes it easier to change the data source without rewriting the entire application.

### 11.1 Proposed modules

#### Authentication module

Responsibilities:

- Acquire access tokens.
- Handle token expiration.
- Use the configured authentication flow.
- Avoid exposing tokens in logs.

#### Consumer module

Responsibilities:

- Connect to the chosen endpoint.
- Read events.
- Handle transient failures.
- Reconnect when appropriate.
- Respect cancellation and shutdown.

#### Event module

Responsibilities:

- Parse incoming JSON.
- Validate required fields.
- Handle missing or unknown fields.
- Convert timestamps and numeric values.

#### Metrics module

Responsibilities:

- Aggregate consumption.
- Calculate defined utilization ratios.
- Group operations by workspace and item.
- Rank consumers.
- Track time windows.

#### CLI module

Responsibilities:

- Parse command-line arguments.
- Format tables.
- Output JSON or CSV.
- Display actionable errors.

### 11.2 Proposed commands

```bash
fabric-cu status
```

Displays recent capacity overview data.

```bash
fabric-cu top --limit 10
```

Displays the highest-consuming operations or items within the configured time window.

```bash
fabric-cu history --minutes 60
```

Displays a summary of locally available or remotely queryable history.

```bash
fabric-cu watch --interval 30
```

Refreshes the display as new data arrives.

```bash
fabric-cu export --format csv
```

Exports available metrics.

These commands should have clearly documented semantics. For example, `watch --interval 30` might refresh the screen every 30 seconds, but that does not mean the underlying event source necessarily emits exactly one usable event every 30 seconds.

### 11.3 Example terminal output

The following is illustrative, not real Fabric data:

```text
Fabric Capacity Monitor
=======================

Capacity: Production-F64
SKU: F64
Base CUs: 64

Recent Capacity Summary
-----------------------
Utilization: Not calculated
Capacity State: Unknown
Last Event: 2026-10-17 22:57:00

Top Consumers
-------------
No operation data available.

Status: Connected
```

The CLI should display `Unknown` or `Not calculated` rather than inventing values when events are missing.

---

## 12. Calculate CU Consumption

Correct calculations are essential.

Do not assume that all event fields use the same units.

### 12.1 Distinguish the relevant quantities

The application should distinguish between:

- Capacity units (CUs).
- CU-seconds.
- CU-milliseconds.
- Capacity utilization percentages.
- Operation duration.
- Throttling delay.

Use the units documented for the actual event schema.

For example, where the operation event schema provides `capacityUnitMs`, this represents CU consumption in milliseconds.

The corresponding CU-seconds are:

```text
CU-seconds = capacityUnitMs / 1000
```

For example:

```text
capacityUnitMs = 125000

CU-seconds = 125000 / 1000

CU-seconds = 125
```

This conversion is valid only when the input field has the documented meaning and units.

### 12.2 Calculate a basic capacity utilization ratio

For a capacity with 64 base CUs over a 30-second interval:

```text
Available CU-seconds = Base CUs × Interval seconds

Available CU-seconds = 64 × 30

Available CU-seconds = 1920
```

If measured consumption for that same interval is 1,440 CU-seconds:

```text
Basic utilization ratio =
    Measured CU-seconds / Available CU-seconds

Basic utilization ratio =
    1440 / 1920

Basic utilization ratio = 75%
```

This is a simple arithmetic ratio, not a guarantee that the result matches the official Capacity Metrics app.

Fabric's official capacity calculations can involve workload-specific rules, smoothing, background and interactive operations, and throttling behavior.

Do not combine unrelated time intervals or compare event totals with different aggregation windows.

### 12.3 Aggregate by workspace and item

To calculate a ranking:

1. Parse the operation event.
2. Validate its CU consumption field.
3. Normalize the units.
4. Identify the operation's workspace and item.
5. Apply the selected time window.
6. Aggregate the relevant CU values.
7. Sort the results in descending order.

Pseudocode:

```cpp
for (const auto& event : events) {
    if (!event.has_valid_consumption()) {
        continue;
    }

    if (!event.is_inside_requested_window()) {
        continue;
    }

    auto key = event.workspace_id() + ":" + event.item_id();

    usage[key] += event.cu_seconds();
}
```

This is illustrative pseudocode. The event structure and accessor methods must be implemented.

Also account for duplicate deliveries. Event delivery systems may provide at-least-once delivery, so a consumer may receive the same event more than once.

Where the schema provides a suitable unique event or operation identifier, use an appropriate deduplication strategy before aggregating consumption.

---

## 13. Implement Authentication Securely

Authentication is dependent on the selected data source.

Do not write a single generic token function and assume the resulting token will work with every Fabric interface.

### 13.1 Token requirements

Before implementing authentication, determine:

- The required identity type.
- The token authority.
- The resource audience or scope.
- Whether delegated or application permissions are supported.
- Whether the endpoint expects OAuth bearer authentication, SAS, or another mechanism.
- How access is granted to the endpoint.

The token audience must match the resource expected by the endpoint.

### 13.2 Interactive authentication

For a user-operated CLI, use a supported interactive authentication flow where available.

The application should:

1. Authenticate the user.
2. Request the necessary access.
3. Acquire a token for the correct resource.
4. Use the token only for the intended connection.
5. Refresh or reacquire tokens through the supported authentication library.

Do not ask the user to paste an access token into the command line.

### 13.3 Service principal authentication

For unattended execution, the application can use a service principal if the chosen endpoint supports it.

For client-credentials authentication, the application needs:

- Tenant ID.
- Client ID.
- A supported credential.
- The correct token audience or scope.
- The required endpoint permissions.

Never hard-code the credential into C++ source.

Avoid storing it in a plain-text configuration file.

For local development, use a protected secret store or environment-based configuration where appropriate.

For production, prefer managed identity or certificate-based authentication where supported.

### 13.4 Example environment variables

The following are placeholders:

```bash
FABRIC_TENANT_ID=<tenant-id>
FABRIC_CLIENT_ID=<client-id>
FABRIC_ENDPOINT=<configured-endpoint>
```

If using a client secret for a supported development scenario:

```bash
FABRIC_CLIENT_SECRET=<secret-value>
```

The secret must be supplied through a secure mechanism.

Do not commit a `.env` file containing real credentials.

Add sensitive local configuration to `.gitignore`.

### 13.5 Secure token handling

The application should:

- Never log access tokens.
- Never log client secrets.
- Avoid placing tokens in command-line arguments.
- Avoid writing tokens to disk unless a supported secure token cache is used.
- Limit token lifetime and scope according to the identity platform.
- Use TLS certificate validation.
- Avoid disabling certificate validation to bypass connection errors.
- Clear sensitive values from memory where practical.

---

## 14. Build and Run the Application

Once the data source and authentication method are confirmed, implement the consumer.

### 14.1 Build configuration

A basic `CMakeLists.txt` could look like this:

```cmake
cmake_minimum_required(VERSION 3.20)

project(fabric-cu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(fabric-cu
    src/main.cpp
    src/auth.cpp
    src/consumer.cpp
    src/event.cpp
    src/metrics.cpp
    src/cli.cpp
)

target_include_directories(fabric-cu PRIVATE include)
```

This file defines a basic CMake target. It does not yet configure or link the external dependencies. Those must be added based on the selected authentication and transport libraries.

### 14.2 Implement in stages

#### Stage 1: Parse a saved event

Use a sanitized sample event to test JSON parsing.

Confirm that the program handles missing and unexpected fields safely.

#### Stage 2: Display a single event

Print a small subset of fields.

Avoid building the entire monitoring interface at once.

#### Stage 3: Connect to the actual source

Configure authentication and the consumer.

Verify that the application can receive a real event.

#### Stage 4: Calculate metrics

Add unit conversions and aggregation.

Validate the calculations against the documented schema.

#### Stage 5: Implement commands

Add `status`, `top`, and `watch`.

#### Stage 6: Add reliability

Handle:

- Network failures.
- Token expiration.
- Reconnection.
- Duplicate events.
- Malformed JSON.
- Missing fields.
- Empty result sets.
- Graceful shutdown.

### 14.3 Example build commands

```bash
cmake -S . -B build
cmake --build build --config Release
```

The exact executable location depends on the operating system and build generator.

---

## 15. Testing and Validation

Test the system at multiple levels.

### 15.1 Fabric configuration tests

Verify:

- The capacity event source exists.
- The selected capacity is correct.
- The Eventstream is published.
- Events are arriving.
- The configured destination or endpoint is reachable.

### 15.2 Authentication tests

Verify:

- The application can obtain a token.
- The token is intended for the correct resource.
- The identity has the required permissions.
- Unauthorized requests fail safely.
- Expired tokens are handled appropriately.

### 15.3 Event parsing tests

Use representative event samples.

Test:

- Missing fields.
- Null values.
- Unexpected event types.
- Invalid numeric values.
- Invalid timestamps.
- Large event payloads.
- Duplicate events.

### 15.4 Calculation tests

Use small, known examples.

For instance:

```text
Base CUs: 64
Interval: 30 seconds
Measured consumption: 1440 CU-seconds

Expected basic ratio: 75%
```

Test the units separately from the calculations.

### 15.5 Compare with Capacity Metrics

Use the official Capacity Metrics app as a reference.

Compare equivalent time intervals and metric definitions.

Differences can arise from:

- Different aggregation windows.
- Event delivery delays.
- Different workload coverage.
- Duplicate events.
- Smoothing.
- Background versus interactive operation accounting.
- Different definitions of utilization.

Document these differences rather than silently changing the calculation.

---

## 16. Security Checklist

Before deploying the CLI, verify the following.

### Identity

- [ ] Use a dedicated application identity where appropriate.
- [ ] Restrict the application to the intended tenant.
- [ ] Choose an authentication flow supported by the actual endpoint.
- [ ] Avoid unnecessary application registrations.
- [ ] Keep development and production identities separate.

### Permissions

- [ ] Grant only the permissions needed for monitoring.
- [ ] Avoid tenant administrator permissions for the CLI.
- [ ] Avoid capacity administrator privileges unless explicitly required.
- [ ] Review workspace access.
- [ ] Obtain administrator consent where required.
- [ ] Confirm that the token audience matches the endpoint.

### Credentials

- [ ] Never embed secrets in source code.
- [ ] Never commit secrets to Git.
- [ ] Do not distribute a shared client secret with the executable.
- [ ] Prefer managed identity or certificate-based credentials where supported.
- [ ] Rotate credentials according to organizational policy.
- [ ] Revoke unused credentials.

### Network

- [ ] Use TLS.
- [ ] Validate certificates.
- [ ] Do not disable TLS validation to fix connection problems.
- [ ] Follow organizational network and private-link requirements.
- [ ] Do not expose Eventstream endpoints unnecessarily.

### Application

- [ ] Never print access tokens or secrets.
- [ ] Sanitize diagnostic output.
- [ ] Validate event fields before using them.
- [ ] Handle malformed events safely.
- [ ] Account for duplicate event delivery.
- [ ] Protect any local historical database.
- [ ] Limit retention of monitoring data to the required period.

### Deployment

- [ ] Document required permissions.
- [ ] Review the configuration before production deployment.
- [ ] Monitor authentication failures.
- [ ] Define a credential revocation procedure.
- [ ] Keep dependencies updated.

---

## 17. Troubleshooting

### Problem: The Eventstream receives no events

Check:

- The selected capacity.
- The capacity administrator permissions required for the source.
- Whether the Eventstream has been published.
- Whether the source is configured correctly.
- Whether the event type is supported in your environment.

### Problem: Authentication succeeds, but the endpoint rejects the request

Possible causes:

- The token audience is incorrect.
- The identity lacks endpoint permissions.
- The selected authentication mode does not match the client.
- The endpoint expects a different protocol.
- Tenant policy blocks the application.
- The client is using the wrong endpoint configuration.

> A successful token request does not prove that the application has permission to consume the stream.

### Problem: Fabric REST API requests return an authorization error

Confirm that the endpoint is actually a Fabric REST API and not an Eventstream endpoint.

Check the API's required permissions and authentication model.

Do not assume that every API supports application permissions.

### Problem: The CLI shows different utilization from Capacity Metrics

Verify that the metrics have the same:

- Time interval.
- Capacity.
- Workload coverage.
- Units.
- Aggregation method.

Do not expect a sum of operation events to reproduce every official capacity utilization calculation.

### Problem: Historical data is unavailable

A live event consumer does not automatically provide arbitrary historical queries.

For historical analysis, persist events in an appropriate destination or use another supported source that provides the required history.

---

## 18. Recommended Development Roadmap

The most efficient implementation sequence is:

### Phase 1: Validate the data source

1. Create the Eventstream.
2. Select capacity overview events.
3. Confirm that events arrive.
4. Save a sanitized sample event.
5. Document the schema.

**Deliverable:** A working monitoring source and a sample event.

### Phase 2: Establish secure connectivity

1. Register the application if required.
2. Select the authentication method.
3. Configure the required permissions.
4. Test endpoint connectivity.
5. Verify that secrets are not exposed.

**Deliverable:** A small program that authenticates and receives data.

### Phase 3: Build the first CLI

1. Parse events.
2. Display capacity information.
3. Add basic utilization calculations.
4. Implement `status`.
5. Implement `watch`.

**Deliverable:** A usable capacity monitoring CLI.

### Phase 4: Add operation-level monitoring

1. Configure capacity operation events.
2. Parse operation records.
3. Aggregate CU consumption.
4. Rank workspaces and items.
5. Implement `top`.

**Deliverable:** A CLI that helps identify expensive workloads.

### Phase 5: Add historical analysis

1. Persist events.
2. Deduplicate where appropriate.
3. Add time-window queries.
4. Implement `history`.
5. Add CSV and JSON export.

**Deliverable:** A monitoring tool for recent and historical CU consumption.

### Phase 6: Production hardening

1. Add robust reconnection.
2. Improve diagnostics.
3. Add unit and integration tests.
4. Review security.
5. Document deployment.
6. Establish credential rotation and revocation procedures.

**Deliverable:** A maintainable monitoring application.

---

## 19. Official Documentation

Use Microsoft's current documentation as the source of truth for permissions, endpoint configuration, authentication, and event schemas.

### Fabric capacity events

- Capacity overview events
- Capacity operation events
- Configure operation events in Eventstream

### Eventstream connectivity

- Fabric Eventstreams overview
- Add a custom endpoint or custom app
- Connect to Eventstream using Microsoft Entra ID authentication

### Authentication and APIs

- Microsoft Entra admin center
- Microsoft identity platform documentation
- Microsoft Fabric REST APIs

### Development tools

- CMake documentation
- libcurl documentation
- nlohmann/json documentation
- librdkafka documentation

---

## Final Recommendation

Start with capacity overview events and a minimal C++ consumer.

Do not begin by trying to reverse-engineer the Capacity Metrics semantic model or by assuming that the entire monitoring interface is exposed through a public REST API.

First, prove that you can securely consume the selected Fabric event source. Then implement the CLI, add operation-level events, and introduce persistent storage only when you need historical queries.

The most important implementation decision is selecting the supported endpoint and authentication method. Once those are confirmed, the C++ parsing, aggregation, and terminal rendering are relatively conventional software development tasks.

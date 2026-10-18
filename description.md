# Microsoft Fabric Capacity Monitor CLI (`fabric-cu`)
## Project Architecture and Technical Specification

---

## 1. Project Overview

The **Microsoft Fabric Capacity Monitor CLI (`fabric-cu`)** is a lightweight, high-performance native C++20 command-line tool designed to monitor Microsoft Fabric Capacity Unit (CU) consumption, capacity health, and workload costs on demand.

### 1.1 The Core Problem with 24/7 Streaming (Eventstream)
The original approach proposed in legacy guides relies on Microsoft Fabric Eventstreams pushing telemetry continuously to a custom endpoint or Lakehouse. While functional, that architecture has a critical drawback:

> **The Observer Effect:** An Eventstream in Microsoft Fabric is an active, always-on resource. Running an Eventstream 24/7 consumes Fabric Capacity Units continuously simply to keep ingestion and routing alive. Persisting events into a Lakehouse and querying them introduces further SQL/Spark compute charges. Using capacity to monitor capacity directly conflicts with the goal of reducing CU consumption and avoiding throttling.

### 1.2 The Hybrid On-Demand Solution
To eliminate continuous strain on Fabric capacity, `fabric-cu` employs an **On-Demand Hybrid Pull Architecture**:
- **Zero Idle Resource Cost:** When the CLI is not running, **zero** compute is consumed. No always-on streaming jobs or persistent pipelines are required.
- **Phase 1 (Capacity Health & Real-Time Status):** Queries the **Fabric Control Plane REST API** and **Azure Monitor Metrics REST API**. These calls run entirely on the Azure management plane and consume **0 Fabric CUs**.
- **Phase 2 (Workload & Item-Level Top Consumers):** Executes targeted, on-demand DAX queries against the existing **Fabric Capacity Metrics (FUAM) Semantic Model** via the Power BI `executeQueries` REST API. Only quick read queries are executed when requested (e.g., `fabric-cu top`), reusing the metrics infrastructure Microsoft already provisions.

---

## 2. System Architecture

```text
                                  ┌───────────────────────────┐
                                  │      fabric-cu CLI        │
                                  │         (C++20)           │
                                  └─────────────┬─────────────┘
                                                │
                     ┌──────────────────────────┴──────────────────────────┐
                     │                                                     │
                     ▼                                                     ▼
           [Phase 1: Real-Time Status]                           [Phase 2: Workload Breakdown]
                     │                                                     │
         ┌───────────┴───────────┐                                         │
         ▼                       ▼                                         ▼
┌──────────────────┐   ┌──────────────────┐                     ┌────────────────────┐
│ Fabric REST API  │   │  Azure Monitor   │                     │ Fabric / Power BI  │
│(Control Plane)   │   │   Metrics API    │                     │`executeQueries` API│
└────────┬─────────┘   └─────────┬────────┘                     └─────────┬──────────┘
         │                       │                                        │
         │ SKU, State,           │ Utilization %,                         │ Top Operations,
         │ Admins, Metadata      │ Throttling %                           │ Workspaces, Items
         │                       │                                        │
         ▼                       ▼                                        ▼
   0 Fabric CUs            0 Fabric CUs                             Near-0 CUs
(Azure Mgmt Plane)      (Azure Mgmt Plane)                       (On-Demand DAX Read)
```

---

## 3. Directory and File Structure

```text
CapacityMetrics CLI/
├── description.md                 # Project architecture, design decisions, and technical spec
├── fabric-cu-monitor-guide.md     # Reference guide and background concepts
├── CMakeLists.txt                 # CMake build configuration (C++20)
├── config/
│   └── config.example.json        # Template configuration file for tenant & capacity details
├── include/
│   ├── auth.hpp                   # Microsoft Entra ID (OAuth 2.0) token manager & cache
│   ├── azure_monitor_client.hpp   # Azure Monitor Metrics REST API client
│   ├── cli.hpp                    # Command line argument parser and dispatcher
│   ├── config.hpp                 # Configuration loader and credential validator
│   ├── fabric_client.hpp          # Microsoft Fabric Capacities REST API client
│   ├── http_client.hpp            # HTTP/HTTPS client abstraction
│   ├── metrics_model.hpp          # Data models: CapacityInfo, UtilizationMetric, OperationRecord
│   ├── terminal_ui.hpp            # Terminal formatting, ASCII tables, live refresh (watch)
│   └── nlohmann/
│       └── json.hpp               # Modern JSON for C++ (header-only)
├── src/
│   ├── main.cpp                   # Application entry point
│   ├── auth.cpp                   # Entra ID token acquisition (Client Credentials / Device Code)
│   ├── azure_monitor_client.cpp   # Metric query execution and time-series extraction
│   ├── cli.cpp                    # Command handlers (`status`, `watch`, `config`, `top`)
│   ├── config.cpp                 # JSON configuration loading & environment variable overrides
│   ├── fabric_client.cpp          # Fabric capacity metadata retrieval
│   ├── http_client.cpp            # HTTPS request/response engine (Native WinHTTP / Libcurl)
│   └── terminal_ui.cpp            # Terminal rendering and table layout
└── tests/
    ├── test_config.cpp            # Unit tests for configuration validation
    └── test_metrics_parser.cpp    # Unit tests for JSON response parsing and unit conversion
```

---

## 4. Module Responsibilities

### 4.1 `include/http_client.hpp` & `src/http_client.cpp`
- Provides an abstracted HTTPS client interface: `Get(url, headers)` and `Post(url, headers, body)`.
- Implemented natively using **Windows WinHTTP** (`winhttp.lib`), requiring **zero third-party DLL dependencies** on Windows, with built-in OS TLS 1.3 certificate validation.
- Supports cross-platform compilation via `libcurl` fallback if built on Linux.

### 4.2 `include/auth.hpp` & `src/auth.cpp`
- Manages Microsoft Entra ID OAuth 2.0 bearer tokens.
- Supports:
  1. **Client Credentials Flow** (Service Principal `client_id` + `client_secret`) for unattended monitoring and automated scripts.
  2. **Device Code / Interactive Flow** for developer workstations without needing to store persistent client secrets.
- In-memory token caching with proactive token expiration and refresh handling (avoiding redundant token requests).

### 4.3 `include/fabric_client.hpp` & `src/fabric_client.cpp`
- Communicates with `https://api.fabric.microsoft.com/v1/capacities`.
- Retrieves capacity metadata: Capacity ID, Display Name, SKU (e.g. `F64`), State (`Active`, `Paused`, `Transitioning`), Region, and Capacity Administrators.
- Consumes **0 Fabric CUs**.

### 4.4 `include/azure_monitor_client.hpp` & `src/azure_monitor_client.cpp`
- Communicates with `https://management.azure.com/.../providers/microsoft.insights/metrics`.
- Fetches real-time time-series telemetry:
  - `AverageCapacityUnitsUtilizationPercentage`
  - Throttling metrics and burst status.
- Consumes **0 Fabric CUs**.

### 4.5 `include/terminal_ui.hpp` & `src/terminal_ui.cpp`
- Renders formatted, human-readable terminal output.
- Features:
  - Colorized utilization bars (`[██████████░░░░░░░░░░] 52%`).
  - Warning levels (Green < 70%, Yellow 70–89%, Red ≥ 90% or Throttling).
  - Clean ASCII table formatting for operations and workspaces.
  - Interactive live refresh mode for `watch`.

---

## 5. Command Specification

| Command | Description | Phase |
|---|---|:---:|
| `fabric-cu status` | Displays capacity metadata, current status, SKU, and recent CU utilization % | Phase 1 |
| `fabric-cu watch [--interval 30]` | Polls and refreshes capacity utilization live in the terminal | Phase 1 |
| `fabric-cu config` | Validates Entra ID credentials and Fabric capacity connection | Phase 1 |
| `fabric-cu top [--limit 10]` | Queries the FUAM semantic model for the highest-consuming operations/items | Phase 2 |
| `fabric-cu history [--minutes 60]` | Summarizes capacity trends and consumption history | Phase 2 |

---

## 6. Development & Phased Implementation Plan

- [x] **Architecture Selection:** Finalized Hybrid On-Demand model (0 CU idle cost).
- [ ] **Phase 1A: Project Scaffolding & Dependencies**
  - Set up directory structure.
  - Vendor `nlohmann/json.hpp`.
  - Create `CMakeLists.txt` configured for C++20 and WinHTTP.
  - Implement `config.hpp` and `config.example.json`.
- [ ] **Phase 1B: Networking & Authentication**
  - Implement `http_client` using native Windows WinHTTP.
  - Implement `auth` (OAuth 2.0 token acquisition from `login.microsoftonline.com`).
- [ ] **Phase 1C: Fabric & Azure Monitor Clients**
  - Implement `fabric_client` (`api.fabric.microsoft.com/v1/capacities`).
  - Implement `azure_monitor_client` (`management.azure.com/providers/microsoft.insights/metrics`).
  - Implement `terminal_ui` and CLI commands: `status`, `watch`, and `config`.
- [ ] **Phase 2: Workload Breakdown (DAX Integration)**
  - Implement Power BI REST API client (`api.powerbi.com/v1.0/myorg/datasets/{id}/executeQueries`).
  - Implement DAX query templates for top CU consuming items and operations.
  - Add `top` and `history` commands.

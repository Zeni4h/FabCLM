# FabCLM - Microsoft Fabric Capacity Monitor CLI

[![Language](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)]()

**FabCLM** is a lightweight, high-performance native C++20 command-line application designed to monitor Microsoft Fabric Capacity Unit (CU) consumption, health, and workload costs on-demand without burning continuous capacity or requiring the Power BI portal to be opened.

---

## Why FabCLM?

| Feature | Legacy Eventstream Streaming | FabCLM Hybrid Pull CLI |
|---|---|---|
| **Idle Cost** | Consumes Fabric CUs **24/7** just to keep stream alive | **0 Fabric CUs** when not running |
| **Compute Overhead** | High ingestion and query strain on monitored capacity | Pure on-demand REST / DAX execution |
| **Dependencies** | Requires Kafka/Eventhouse/Lakehouse infrastructure | **Zero external DLLs** (Native WinHTTP) |
| **Trial Capacity Support** | Complex routing | Full support for **FTL4 Trial** & **Paid F-SKUs** |

---

## Key Features

- **Live Capacity Health (`status`):** Real-time capacity state (`Active`/`Paused`), SKU (`F64`, `FTL4`), Azure region, and utilization trends.
- **Top Consuming Workloads (`top`):** On-demand DAX query targeting the Fabric Capacity Metrics semantic model to find expensive items (notebooks, warehouses, lakehouses, semantic models).
- **Interactive Live Monitor (`watch`):** Live-refreshing terminal dashboard with colorized ASCII progress bars and trend sparklines (`. <25%`, `: <50%`, `o <75%`, `O <90%`, `# >90%`).
- **Semantic Model Auto-Discovery (`discover`):** Scans all accessible Fabric workspaces to locate and map your Capacity Metrics dataset automatically.
- **Diagnostic Engine (`config`):** Validates Entra ID tokens, Fabric Platform API, and Power BI DAX endpoints with actionable error reporting.

---

## Directory Structure

```text
FabCLM/
├── CMakeLists.txt                 # Modern CMake C++20 build configuration
├── build.bat                      # One-command quick build script using GCC/MinGW
├── .gitignore                     # Ignores binaries and local credentials (config.json)
├── config/
│   ├── config.example.json        # Template configuration file
│   └── config.json                # Local credentials (git-ignored, never committed)
├── include/
│   ├── auth.hpp                   # Entra ID OAuth 2.0 token manager with auto-expiration caching
│   ├── azure_monitor_client.hpp   # Azure Monitor Metrics API client (0 Fabric CU cost)
│   ├── cli.hpp                    # Command router and CLI dispatcher
│   ├── config.hpp                 # Configuration loader and validator
│   ├── dax_client.hpp             # Power BI executeQueries DAX engine
│   ├── fabric_client.hpp          # Microsoft Fabric Platform REST API client
│   ├── http_client.hpp            # Native WinHTTP client (zero DLL dependencies on Windows)
│   ├── metrics_model.hpp          # Core data models: CapacityInfo, TopConsumerItem
│   ├── terminal_ui.hpp            # ANSI color rendering, gauges, and sparklines
│   └── nlohmann/
│       └── json.hpp               # Modern C++ JSON parser v3.11.3 (header-only)
├── src/
│   ├── main.cpp                   # Application entry point
│   ├── auth.cpp                   # Entra ID token acquisition implementation
│   ├── azure_monitor_client.cpp   # ARM metrics timeseries extraction
│   ├── cli.cpp                    # Command handlers and live watch loop
│   ├── config.cpp                 # JSON parsing and environment variable overrides
│   ├── dax_client.cpp             # Semantic model discovery and DAX queries
│   ├── fabric_client.cpp          # Fabric capacity lookup
│   ├── http_client.cpp            # Native WinHTTP implementation (TLS 1.3)
│   └── terminal_ui.cpp            # Terminal UI rendering
└── tests/
    └── test_metrics_parser.cpp    # Unit tests for capacity models and JSON parsing
```

---

## Prerequisites

1. **Operating System:** Windows 10/11 (or Linux)
2. **Compiler:** C++20 compliant compiler:
   * **GCC 11+** (e.g. MinGW-w64 on Windows) or **MSVC 2019+**
3. **Microsoft Entra ID (Azure AD) App Registration:**
   * An App Registration with a Client ID and Client Secret (or Device Code flow).
   * Permissions: Access to Microsoft Fabric API and Power BI service.
4. **Microsoft Fabric Capacity Metrics App:**
   * Installed in your Fabric tenant (available from Fabric AppSource).
   * **Important:** Add your Service Principal as a **Viewer** to the workspace where the Capacity Metrics App is installed.

---

## Installation & Setup

### 1. Clone the Repository
```bash
git clone https://github.com/Zeni4h/FabCLM.git
cd FabCLM
```

### 2. Configure Credentials
Copy the template configuration file:
```powershell
Copy-Item config\config.example.json config\config.json
```

Edit `config\config.json`:
```json
{
  "auth": {
    "tenant_id": "YOUR_TENANT_ID",
    "client_id": "YOUR_CLIENT_ID",
    "client_secret": "YOUR_CLIENT_SECRET",
    "use_device_code_flow": false
  },
  "fabric": {
    "capacity_id": "YOUR_CAPACITY_ID",
    "capacity_name": "YOUR_CAPACITY_NAME"
  },
  "azure": {
    "subscription_id": "OPTIONAL_FOR_PAID_F_SKUS",
    "resource_group": "OPTIONAL_FOR_PAID_F_SKUS"
  },
  "metrics_app": {
    "dataset_id": ""
  }
}
```
*(Alternatively, you can supply credentials via environment variables: `FABRIC_TENANT_ID`, `FABRIC_CLIENT_ID`, `FABRIC_CLIENT_SECRET`, `FABRIC_CAPACITY_ID`).*

### 3. Build the Application
On Windows with GCC/MinGW:
```powershell
.\build.bat
```
*(Or with CMake: `cmake -B build && cmake --build build --config Release`).*

### 4. Verify Connection
Run the diagnostic check:
```powershell
.\bin\fabclm.exe config
```

### 5. Auto-Discover Your Capacity Metrics Dataset
Scan your workspaces to locate the Fabric Capacity Metrics dataset ID:
```powershell
.\bin\fabclm.exe discover
```
Copy the discovered `Dataset ID` into `config\config.json` under `"metrics_app": { "dataset_id": "..." }`.

---

## Usage & Commands

| Command | Description | Example |
|---|---|---|
| `status` | Displays capacity health, SKU, region, and recent CU utilization trend | `fabclm status` |
| `top` | Queries top CU-consuming operations and items via DAX | `fabclm top --limit 10` |
| `watch` | Live interactive terminal monitor (polls every N seconds) | `fabclm watch --interval 30` |
| `list` | Lists all Fabric capacities accessible in the tenant | `fabclm list` |
| `discover` | Scans workspaces to find the Capacity Metrics dataset | `fabclm discover` |
| `config` | Runs diagnostic tests on authentication and API endpoints | `fabclm config` |

### Making `fabclm` Globally Accessible
To run `fabclm` from any directory without typing `.\bin\fabclm.exe`, add the `bin` folder to your User PATH:
```powershell
[Environment]::SetEnvironmentVariable(
    "Path",
    [Environment]::GetEnvironmentVariable("Path", "User") + ";$((Get-Item .).FullName)\bin",
    "User"
)
```
*(Restart your terminal for changes to take effect).*

---

## Security & Secrets Management

- **Zero Hardcoded Secrets:** No tenant IDs, client secrets, or private tokens are stored in the codebase.
- **Git-Ignored Credentials:** `config/config.json` is strictly git-ignored and will never be tracked or pushed to remote repositories.
- **Least Privilege:** The Service Principal only requires `Viewer` access to the Capacity Metrics workspace to read monitoring data.

---

## License

This project is licensed under the MIT License.

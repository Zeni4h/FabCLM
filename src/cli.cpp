#include "cli.hpp"
#include "http_client.hpp"
#include "auth.hpp"
#include "fabric_client.hpp"
#include "azure_monitor_client.hpp"
#include "dax_client.hpp"
#include "terminal_ui.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

namespace fabclm {

namespace {

std::atomic<bool> g_keepRunning(true);

void SignalHandler(int) {
    g_keepRunning = false;
}

} // anonymous namespace

Cli::Cli(const AppConfig& config) : config_(config) {}

int Cli::Run(int argc, char* argv[]) {
    TerminalUI::InitializeConsole();

    if (argc < 2) {
        return HandleStatus();
    }

    std::string cmd = argv[1];

    if (cmd == "status") {
        return HandleStatus();
    } else if (cmd == "watch") {
        int interval = 30;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--interval" || arg == "-i") && i + 1 < argc) {
                interval = std::max(5, std::atoi(argv[++i]));
            }
        }
        return HandleWatch(interval);
    } else if (cmd == "list") {
        return HandleList();
    } else if (cmd == "top") {
        int limit = 10;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--limit" || arg == "-l") && i + 1 < argc) {
                limit = std::max(1, std::atoi(argv[++i]));
            }
        }
        return HandleTop(limit);
    } else if (cmd == "discover") {
        return HandleDiscover();
    } else if (cmd == "config") {
        return HandleConfig();
    } else if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        PrintHelp();
        return 0;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n\n";
        PrintHelp();
        return 1;
    }
}

int Cli::HandleStatus() {
    std::vector<std::string> configErrors;
    if (!config_.Validate(configErrors)) {
        std::cerr << "\nConfiguration incomplete. Please configure 'config/config.json' or set environment variables:\n";
        for (const auto& err : configErrors) {
            std::cerr << "  - " << err << "\n";
        }
        std::cerr << "\nRun 'fabclm config' to diagnose connection settings.\n";
        return 1;
    }

    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);
    FabricClient fabricClient(http, tokenProvider);
    AzureMonitorClient azureClient(http, tokenProvider);
    DaxClient daxClient(http, tokenProvider);

    std::string targetCap = config_.fabric.capacityId.empty() ? config_.fabric.capacityName : config_.fabric.capacityId;

    CapacityInfo capInfo;
    std::string fabricErr;
    std::cout << "Connecting to Microsoft Fabric...\r" << std::flush;

    if (!fabricClient.GetCapacity(targetCap, capInfo, fabricErr)) {
        std::cerr << "\nError retrieving Fabric capacity: " << fabricErr << "\n";
        return 1;
    }

    CapacityMetricsSummary summary;
    summary.capacity = capInfo;

    // Check Azure Monitor only if it is not a trial SKU and azure config is provided
    bool isTrialSku = (capInfo.sku.rfind("FT", 0) == 0 || capInfo.displayName.rfind("Trial-", 0) == 0);

    if (!isTrialSku && !config_.azure.subscriptionId.empty() && !config_.azure.resourceGroup.empty()) {
        std::string capNameForAzure = capInfo.displayName.empty() ? config_.fabric.capacityName : capInfo.displayName;
        std::cout << "Querying Azure Monitor metrics...\r" << std::flush;
        std::string azureErr;
        azureClient.GetCapacityMetrics(config_.azure.subscriptionId, config_.azure.resourceGroup, capNameForAzure, summary, azureErr);
    }

    // If Azure Monitor was skipped or gave no data (Trial capacity), query the Capacity Metrics App (FUAM) semantic model via DAX!
    if (summary.currentUtilizationPct == 0.0) {
        std::string dsId = config_.metricsApp.datasetId;
        if (dsId.empty()) {
            std::vector<DiscoveredDataset> found;
            std::string dErr;
            if (daxClient.DiscoverMetricsDatasets(found, dErr) && !found.empty()) {
                dsId = found[0].id;
            }
        }

        if (!dsId.empty()) {
            std::string daxErr;
            if (daxClient.GetRecentTimepoints(dsId, capInfo.id, summary, daxErr)) {
                // Successfully retrieved real metrics from FUAM model
            }
        }
    }

    TerminalUI::RenderStatus(summary);
    return 0;
}

int Cli::HandleWatch(int intervalSeconds) {
    std::vector<std::string> configErrors;
    if (!config_.Validate(configErrors)) {
        std::cerr << "\nConfiguration incomplete for watch mode:\n";
        for (const auto& err : configErrors) {
            std::cerr << "  - " << err << "\n";
        }
        return 1;
    }

    std::signal(SIGINT, SignalHandler);
    g_keepRunning = true;

    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);
    FabricClient fabricClient(http, tokenProvider);
    AzureMonitorClient azureClient(http, tokenProvider);
    DaxClient daxClient(http, tokenProvider);

    std::string targetCap = config_.fabric.capacityId.empty() ? config_.fabric.capacityName : config_.fabric.capacityId;
    CapacityInfo capInfo;
    std::string fabricErr;

    if (!fabricClient.GetCapacity(targetCap, capInfo, fabricErr)) {
        std::cerr << "Failed to resolve capacity: " << fabricErr << "\n";
        return 1;
    }

    bool isTrialSku = (capInfo.sku.rfind("FT", 0) == 0 || capInfo.displayName.rfind("Trial-", 0) == 0);
    std::string capNameForAzure = capInfo.displayName.empty() ? config_.fabric.capacityName : capInfo.displayName;

    // Cache dataset ID for watch mode
    std::string dsId = config_.metricsApp.datasetId;
    if (dsId.empty()) {
        std::vector<DiscoveredDataset> found;
        std::string dErr;
        if (daxClient.DiscoverMetricsDatasets(found, dErr) && !found.empty()) {
            dsId = found[0].id;
        }
    }

    while (g_keepRunning) {
        CapacityMetricsSummary summary;
        summary.capacity = capInfo;

        if (!isTrialSku && !config_.azure.subscriptionId.empty() && !config_.azure.resourceGroup.empty() && !capNameForAzure.empty()) {
            std::string azureErr;
            azureClient.GetCapacityMetrics(config_.azure.subscriptionId, config_.azure.resourceGroup, capNameForAzure, summary, azureErr);
        }

        if (summary.currentUtilizationPct == 0.0 && !dsId.empty()) {
            std::string daxErr;
            daxClient.GetRecentTimepoints(dsId, capInfo.id, summary, daxErr);
        }

        TerminalUI::ClearScreen();
        TerminalUI::RenderStatus(summary);

        std::cout << "Live watch active (refreshing every " << intervalSeconds << "s). Press Ctrl+C to stop.\n";

        for (int i = 0; i < intervalSeconds && g_keepRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "\nWatch stopped.\n";
    return 0;
}

int Cli::HandleList() {
    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);
    FabricClient fabricClient(http, tokenProvider);

    std::vector<CapacityInfo> caps;
    std::string err;
    std::cout << "Listing Fabric capacities...\n";

    if (!fabricClient.ListCapacities(caps, err)) {
        std::cerr << "Failed to list capacities: " << err << "\n";
        return 1;
    }

    std::cout << "\nFound " << caps.size() << " capacity/capacities:\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "Name                 | SKU   | State    | Region         | ID\n";
    std::cout << "------------------------------------------------------------------------\n";
    for (const auto& c : caps) {
        std::cout << c.displayName << " | "
                  << c.sku << " | "
                  << CapacityStateToString(c.state) << " | "
                  << c.region << " | "
                  << c.id << "\n";
    }
    std::cout << "------------------------------------------------------------------------\n\n";
    return 0;
}

int Cli::HandleTop(int limit) {
    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);
    DaxClient daxClient(http, tokenProvider);

    std::string dsId = config_.metricsApp.datasetId;
    if (dsId.empty()) {
        std::cout << "Searching for Capacity Metrics dataset in workspaces...\r" << std::flush;
        std::vector<DiscoveredDataset> found;
        std::string dErr;
        if (daxClient.DiscoverMetricsDatasets(found, dErr) && !found.empty()) {
            dsId = found[0].id;
        } else {
            std::cerr << "\nCould not auto-discover Capacity Metrics dataset: " << dErr << "\n";
            std::cerr << "Tips:\n";
            std::cerr << " 1. Ensure your Service Principal is added as a 'Viewer' to the workspace where the Metrics App is installed.\n";
            std::cerr << " 2. Run 'fabclm discover' to test workspace visibility.\n";
            std::cerr << " 3. Manually set 'metrics_app.dataset_id' in 'config/config.json'.\n";
            return 1;
        }
    }

    std::cout << "Querying top " << limit << " capacity-consuming workloads via DAX...\r" << std::flush;
    std::vector<TopConsumerItem> items;
    std::string err;
    if (!daxClient.GetTopConsumers(dsId, limit, items, err)) {
        std::cerr << "\nFailed to query top consumers: " << err << "\n";
        return 1;
    }

    TerminalUI::RenderTopConsumers(items);
    return 0;
}

int Cli::HandleDiscover() {
    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);
    DaxClient daxClient(http, tokenProvider);

    std::cout << "Scanning tenant and workspaces for Capacity Metrics datasets...\n";
    std::vector<DiscoveredDataset> datasets;
    std::string err;
    if (!daxClient.DiscoverMetricsDatasets(datasets, err)) {
        std::cerr << "\nDiscovery failed or no metrics datasets found: " << err << "\n";
        std::cerr << "\nNote: Ensure your Service Principal has at least 'Viewer' access to the workspace where the Capacity Metrics App is installed.\n\n";
        return 1;
    }

    std::cout << "\nFound " << datasets.size() << " Capacity Metrics dataset(s):\n";
    std::cout << "--------------------------------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(28) << "Dataset Name"
              << std::left << std::setw(38) << "Workspace Name"
              << std::left << std::setw(38) << "Dataset ID" << "\n";
    std::cout << "--------------------------------------------------------------------------------------------------------\n";
    for (const auto& ds : datasets) {
        std::cout << std::left << std::setw(28) << ds.name
                  << std::left << std::setw(38) << ds.workspaceName
                  << std::left << std::setw(38) << ds.id << "\n";
    }
    std::cout << "--------------------------------------------------------------------------------------------------------\n";
    std::cout << "\nTo use one of these datasets, set 'metrics_app.dataset_id' in 'config/config.json':\n";
    std::cout << "  \"metrics_app\": { \"dataset_id\": \"" << datasets[0].id << "\" }\n\n";
    return 0;
}

int Cli::HandleConfig() {
    std::cout << "Testing credentials and endpoints...\n\n";

    HttpClient http;
    TokenProvider tokenProvider(config_.auth, http);

    std::string fabricToken;
    std::string authErr;
    bool authOk = tokenProvider.GetToken("https://api.fabric.microsoft.com/.default", fabricToken, authErr);

    std::string pbiToken;
    std::string pbiErr;
    bool pbiOk = tokenProvider.GetToken("https://analysis.windows.net/powerbi/api/.default", pbiToken, pbiErr);

    bool fabricOk = false;
    std::string details;
    if (authOk) {
        FabricClient fabricClient(http, tokenProvider);
        std::vector<CapacityInfo> caps;
        if (fabricClient.ListCapacities(caps, details)) {
            fabricOk = true;
            details = "Successfully connected to Fabric API. Accessible capacities: " + std::to_string(caps.size());
        }
    } else {
        details = "Fabric Auth error: " + authErr;
    }

    bool azureOk = false;
    if (authOk && !config_.azure.subscriptionId.empty() && !config_.azure.resourceGroup.empty()) {
        AzureMonitorClient azureClient(http, tokenProvider);
        CapacityMetricsSummary s;
        std::string azErr;
        std::string capName = config_.fabric.capacityName.empty() ? "test" : config_.fabric.capacityName;
        azureOk = azureClient.GetCapacityMetrics(config_.azure.subscriptionId, config_.azure.resourceGroup, capName, s, azErr);
        if (azureOk) {
            details += "\nAzure Monitor API: Successfully retrieved metrics.";
        } else {
            details += "\nAzure Monitor API Note: " + azErr;
        }
    }

    if (pbiOk) {
        details += "\nPower BI / DAX API Token: Acquired successfully.";
    } else {
        details += "\nPower BI API Token Note: " + pbiErr;
    }

    TerminalUI::RenderConfigCheck(authOk, fabricOk, azureOk, details);
    return authOk ? 0 : 1;
}

void Cli::PrintHelp() {
    std::cout << "FabCLM - Microsoft Fabric Capacity Monitor CLI\n\n"
              << "Usage:\n"
              << "  fabclm [command] [options]\n\n"
              << "Commands:\n"
              << "  status                 Display capacity status and current CU utilization (default)\n"
              << "  top [--limit N]        Display top CU-consuming operations and workloads via DAX (default: 10)\n"
              << "  discover               Scan workspaces to find the Capacity Metrics dataset ID\n"
              << "  watch [--interval N]   Live refresh capacity utilization every N seconds (default: 30)\n"
              << "  list                   List all capacities accessible in your tenant\n"
              << "  config                 Verify Entra ID credentials and API connectivity\n"
              << "  help, --help, -h       Show this help message\n\n"
              << "Configuration:\n"
              << "  Configure 'config/config.json' with your Entra ID and Fabric details.\n\n";
}

} // namespace fabclm

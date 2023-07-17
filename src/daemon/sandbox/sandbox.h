/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: zhongtao
 * Create: 2023-06-19
 * Description: provide sandbox class definition
 *********************************************************************************/

#ifndef DAEMON_SANDBOX_SANDBOX_H
#define DAEMON_SANDBOX_SANDBOX_H

#include <string>
#include <mutex>
#include <google/protobuf/map.h>

#include <isula_libutils/container_network_settings.h>
#include <isula_libutils/sandbox_state.h>
#include <isula_libutils/sandbox_metadata.h>

#include "api_v1.grpc.pb.h"
#include "errors.h"
#include "controller.h"
#include "controller_manager.h"
#include "cstruct_wrapper.h"
#include "read_write_lock.h"

namespace sandbox {

const std::string SANDBOX_METADATA_JSON = "sandbox_metadata.json";
const std::string SANDBOX_STATE_JSON = "sandbox_state.json";
const std::string NETWORK_SETTINGS_JSON = "network_settings.json";

// Keep consistent with the default values set in containerd and cri-o.
const uint32_t DEFAULT_STOP_TIMEOUT = 10;

enum SandboxStatus {
    SANDBOX_STATUS_UNKNOWN = 0,
    SANDBOX_STATUS_CREATED,
    SANDBOX_STATUS_RUNNING,
    SANDBOX_STATUS_STOPPED,
    SANDBOX_STATUS_REMOVING,
};

struct StatsInfo {
    int64_t timestamp;
    uint64_t cpuUseNanos;
};

class SandboxState {
public:
    SandboxState() = default;
    auto GetPid() -> uint32_t;
    auto GetCreatedAt() -> uint64_t;
    auto GetExitedAt() -> uint64_t;
    auto GetUpdateAt() -> uint64_t;
    auto GetStatus() -> SandboxStatus;
    auto GetExitStatus() -> uint32_t;

    void SetPid(uint32_t pid);
    void SetUpdatedAt(uint64_t time);
    void SetCreatedAt(uint64_t time);
    void SetExitedAt(uint64_t time);
    void SetExitStatus(uint32_t code);
    void SetStatus(SandboxStatus st);

    auto UpdateStatus(SandboxStatus st) -> SandboxStatus;

private:
    uint32_t m_pid;
    uint64_t m_createdAt;
    // now, m_updatedAt is unused
    uint64_t m_updatedAt;
    uint64_t m_exitedAt;
    uint32_t m_exitStatus;
    SandboxStatus m_status;
    RWMutex m_stateMutex;
};

class Sandbox : public SandboxExitCallback, public std::enable_shared_from_this<Sandbox> {
public:
    Sandbox(const std::string id, const std::string &rootdir, const std::string &statedir,
            const std::string &name = nullptr,
            const std::string &sandboxer = nullptr, const std::string &runtime = nullptr, std::string netNsPath = nullptr,
            const runtime::v1::PodSandboxConfig &sandboxConfig = runtime::v1::PodSandboxConfig::default_instance());
    virtual ~Sandbox() = default;

    auto IsReady() -> bool;

    auto GetId() -> const std::string &;
    auto GetName() -> const std::string &;
    auto GetSandboxer() -> const std::string &;
    auto GetRuntimeHandle() -> const std::string &;
    auto GetContainers() -> const std::vector<std::string> &;
    auto GetSandboxConfig() -> std::shared_ptr<runtime::v1::PodSandboxConfig>;
    auto GetRootDir() -> std::string;
    auto GetStateDir() -> std::string;
    auto GetResolvPath() -> std::string;
    auto GetShmPath() -> std::string;
    auto GetStatsInfo() -> const StatsInfo &;
    auto GetNetworkReady() -> bool;

    void AddAnnotations(const std::string &key, const std::string &value);
    void RemoveAnnotations(const std::string &key);
    void AddLabels(const std::string &key, const std::string &value);
    void RemoveLabels(const std::string &key);
    void AddContainer(const std::string &id);
    void SetConatiners(const std::vector<std::string> &cons);
    void RemoveContainers(const std::string &id);
    void UpdateNetworkSettings(const std::string &settingsJson, Errors &error);
    auto UpdateStatsInfo(StatsInfo &info) -> StatsInfo;
    void SetNetworkReady(bool ready);

    // Save to file
    auto Save(Errors &error) -> bool;
    // Load from file
    auto Load(Errors &error) -> bool;

    void OnSandboxExit(const ControllerExitInfo &exitInfo);

    auto UpdateStatus(Errors &error) -> bool;

    auto Create(Errors &error) -> bool;
    auto Start(Errors &error) -> bool;
    auto Stop(uint32_t timeoutSecs, Errors &error) -> bool;
    auto Remove(bool force, Errors &error) -> bool;
    auto Status(Errors &error) -> std::unique_ptr<runtime::v1::PodSandboxStatus>;

private:
    auto SaveState(Errors &error) -> bool;
    auto SaveMetadata(Errors &error) -> bool;
    auto SaveNetworkSetting(Errors &error) -> bool;

    auto LoadState(Errors &error) -> bool;
    auto LoadMetadata(Errors &error) -> bool;
    auto LoadNetworkSetting(Errors &error) -> bool;

    void SetSandboxConfig(const runtime::v1::PodSandboxConfig &config);
    void SetNetworkSettings(std::string &settings, Errors &error);
    auto SetupSandboxFiles(Errors &error) -> int;
    void DoUpdateStatus(std::unique_ptr<ControllerSandboxStatus> status, Errors &error);

    auto GetTaskAddress() -> const std::string &;
    auto GetNetworkSettings() -> const std::string &;

    auto GetHostnamePath() -> std::string;
    auto GetHostsPath() -> std::string;
    auto GetMetadataJsonPath() ->  std::string;
    auto GetStatePath() -> std::string;
    auto GetNetworkSettingsPath() -> std::string;

    void FillSandboxState(sandbox_state *state);
    void FillSandboxMetadata(sandbox_metadata* metadata, Errors &error);

    auto GenerateSandboxStateJson(sandbox_state *state) -> std::string;
    auto GenerateSandboxMetadataJson(sandbox_metadata *metadata) -> std::string;
    auto ParseSandboxStateFile() ->std::unique_ptr<CStructWrapper<sandbox_state>>;
    auto ParseSandboxMetadataFile() ->std::unique_ptr<CStructWrapper<sandbox_metadata>>;

    auto DoStop(uint32_t timeoutSecs, Errors &error) -> bool;
    auto IsRemovalInProcess(Errors &error) -> bool;

private:
    // Since the cri module will operate concurrently on the sandbox instance,
    // use m_mutex to ensure the correctness of the sandbox instance
    // and its member variables (m_statsInfo and m_networkSettings)
    RWMutex m_mutex;
    SandboxState m_state;
    std::string m_id;
    std::string m_name;
    std::string m_sandboxer;
    std::string m_runtimeHandler;
    // m_rootdir = conf->rootpath + / + sandbox
    std::string m_rootdir;
    std::string m_statedir;
    std::string m_taskAddress;
    StatsInfo m_statsInfo;
    // Store network information in the sandbox, which is convenient for the cri module to obtain
    // and update the network settings of the pause container in the shim-controller.
    std::string m_netNsPath;
    bool m_networkReady;
    std::string m_networkSettings;
    // container id lists
    std::vector<std::string> m_containers;
    // TOOD: m_sandboxConfig is a protobuf message, it can be converted to json string directly
    //       if save json string directly for sandbox recover, we need to consider hot
    //       upgrade between different CRI versions
    std::shared_ptr<runtime::v1::PodSandboxConfig> m_sandboxConfig;

    // it should select accroding to the config
    std::shared_ptr<Controller> m_controller { nullptr };
};

} // namespace sandbox

#endif // DAEMON_SANDBOX_SANDBOX_H
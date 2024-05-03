#include <hgx.h>
#include <nlohmann/json.hpp>
#include <openbmc/kv.hpp>
#include <openbmc/libgpio.hpp>
#include <openbmc/i2c_cdev.h>
#include <string.h>
#include <syslog.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "time_utils.hpp"
#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"

#include <fstream>
#include <streambuf>

// HTTP response status codes
enum {
  HTTP_OK = 200,
  HTTP_ACCEPTED = 202,
  HTTP_NO_CONTENT = 204,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
};

constexpr auto HMC_USR = "root";
constexpr auto HMC_PWD = "0penBmc";

const std::string HMC_BASE_URL = "http://192.168.31.1";
const auto HMC_URL = HMC_BASE_URL + "/redfish/v1/";
const auto HMC_UPDATE_SERVICE = HMC_URL + "UpdateService";
const auto HMC_TASK_SERVICE = HMC_URL + "TaskService/Tasks/";
const auto HMC_FW_INVENTORY = HMC_URL + "UpdateService/FirmwareInventory/";
const auto HGX_TELEMETRY_SERVICE_EVT = HMC_URL +
    "TelemetryService/MetricReportDefinitions/PlatformEnvironmentMetrics";
const auto HGX_TELEMETRY_SERVICE_DVT = HMC_URL +
    "TelemetryService/MetricReports/HGX_PlatformEnvironmentMetrics_0/";
const auto HMC_FACTORY_RESET_SERVICE = "/Actions/Manager.ResetToDefaults";
const auto HMC_RESET_SERVICE = "/Actions/Manager.Reset";
const auto HMC_DUMP_SERVICE = HMC_URL +
    "Managers/HGX_BMC_0/LogServices/Dump/Actions/LogService.CollectDiagnosticData";
const auto SYSTEM_DUMP_SERVICE = HMC_URL +
    "Systems/HGX_Baseboard_0/LogServices/Dump/Actions/LogService.CollectDiagnosticData";

const std::vector<std::string> HMC_PATCH_TARGETS_EVT = {
  "ERoT_FPGA_Firmware", "ERoT_GPU0_Firmware", "ERoT_GPU1_Firmware",
  "ERoT_GPU2_Firmware", "ERoT_GPU3_Firmware", "ERoT_GPU4_Firmware",
  "ERoT_GPU5_Firmware", "ERoT_GPU6_Firmware", "ERoT_GPU7_Firmware",
  "ERoT_NVSwitch0_Firmware", "ERoT_NVSwitch1_Firmware",
  "ERoT_NVSwitch2_Firmware", "ERoT_NVSwitch3_Firmware",
  "ERoT_PCIeSwitch_Firmware", "ERoT_HMC_Firmware", "HMC_Firmware"
};
const std::vector<std::string> HMC_PATCH_TARGETS_DVT = {
  "HGX_FW_ERoT_FPGA_0", "HGX_FW_ERoT_GPU_SXM_1",
  "HGX_FW_ERoT_GPU_SXM_2", "HGX_FW_ERoT_GPU_SXM_3",
  "HGX_FW_ERoT_GPU_SXM_4", "HGX_FW_ERoT_GPU_SXM_5",
  "HGX_FW_ERoT_GPU_SXM_6", "HGX_FW_ERoT_GPU_SXM_7",
  "HGX_FW_ERoT_GPU_SXM_8", "HGX_FW_ERoT_NVSwitch_0",
  "HGX_FW_ERoT_NVSwitch_1", "HGX_FW_ERoT_NVSwitch_2",
  "HGX_FW_ERoT_NVSwitch_3", "HGX_FW_ERoT_PCIeSwitch_0",
  "HGX_FW_ERoT_HMC_0", "HGX_FW_HMC_0"
};

const std::vector<std::string> BMC_PATCH_TARGETS_DVT = {
  "HGX_FW_ERoT_FPGA_0", "HGX_FW_ERoT_GPU_SXM_1",
  "HGX_FW_ERoT_GPU_SXM_2", "HGX_FW_ERoT_GPU_SXM_3",
  "HGX_FW_ERoT_GPU_SXM_4", "HGX_FW_ERoT_GPU_SXM_5",
  "HGX_FW_ERoT_GPU_SXM_6", "HGX_FW_ERoT_GPU_SXM_7",
  "HGX_FW_ERoT_GPU_SXM_8", "HGX_FW_ERoT_NVSwitch_0",
  "HGX_FW_ERoT_NVSwitch_1", "HGX_FW_ERoT_NVSwitch_2",
  "HGX_FW_ERoT_NVSwitch_3", "HGX_FW_ERoT_PCIeSwitch_0",
  "HGX_FW_ERoT_BMC_0", "HGX_FW_BMC_0"
};

constexpr auto TIME_OUT = 6;

using nlohmann::json;

namespace hgx {

class HGXMgr {
 public:
  HGXMgr() {
    RestClient::init();
  }
  ~HGXMgr() {
    RestClient::disable();
  }
  static std::string get(const std::string& url) {
    RestClient::Response result;
    RestClient::Connection conn(url);

    conn.SetTimeout(TIME_OUT);
    conn.SetBasicAuth(HMC_USR, HMC_PWD);

    result = conn.get("");
    if (result.code != HTTP_OK) {
      throw HTTPException(result.code);
    }
    return result.body;
  }

  static std::string post(const std::string& url, std::string&& args, bool isFile) {
    RestClient::Response result;
    RestClient::Connection conn(url);

    if (isFile) {
      std::ifstream t(std::move(args));
      std::string str((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());
      result =  conn.post("", str);
    }
    else {
      result =  conn.post("", std::move(args));
    }

    if (result.code != HTTP_OK && result.code != HTTP_ACCEPTED &&
        result.code != HTTP_NO_CONTENT) {
      throw HTTPException(result.code);
    }
    return result.body;
  }

  static std::string patch(const std::string& url, std::string&& args) {
    RestClient::Response result;
    RestClient::Connection conn(url);

    result = conn.patch("", std::move(args));

    if (result.code != HTTP_OK && result.code != HTTP_ACCEPTED) {
      throw HTTPException(result.code);
    }
    return result.body;
  }
};

static HGXMgr hgx;

GPUConfig getConfig() {
  int ret = -1;
  GPIO gpio("GPU_PRSNT_N_ISO_R");
  const std::string config_key{"gpu_config"};

  gpio.open();
  if (gpio.get_value() == GPIO_VALUE_HIGH) {
    throw GPUNotReady();
  }
  gpio.close();

  try {
    auto val = kv::get(config_key, kv::region::persist);
    if (val == "hgx") {
      return GPU_CONFIG_HGX;
    } else if (val == "ubb") {
      return GPU_CONFIG_UBB;
    } else {
      return GPU_CONFIG_UNKNOWN;
    }
  } catch (std::filesystem::filesystem_error& e) {
    const int hgx_eeprom_bus = 0x9;
    int fd = 0;
    uint8_t tlen, rlen;
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};
    const uint8_t hgx_eeprom_addr = 0xA6;

    fd = i2c_cdev_slave_open(hgx_eeprom_bus, hgx_eeprom_addr >> 1, I2C_SLAVE_FORCE_CLAIM);
    if (fd < 0) {
      syslog(LOG_WARNING, "%s() Failed to open 9", __func__);
      throw std::runtime_error("I2C Bus open failure");
    }

    tbuf[0] = 0x00;
    tlen = 1;
    rlen = 1;

    ret = i2c_rdwr_msg_transfer(fd, hgx_eeprom_addr, tbuf, tlen, rbuf, rlen);
    i2c_cdev_slave_close(fd);
    GPUConfig cfg;
    if (ret == 0) {
      cfg = GPU_CONFIG_HGX;
      kv::set(config_key, "hgx", kv::region::persist);
    } else {
      cfg = GPU_CONFIG_UBB;
      kv::set(config_key, "ubb", kv::region::persist);
    }
    return cfg;
  }
  return GPU_CONFIG_UNKNOWN;
}

std::string redfishGet(const std::string& subpath) {
  if (subpath.starts_with("/redfish/v1")) {
    return hgx.get(HMC_BASE_URL + subpath);
  }
  return hgx.get(HMC_URL + subpath);
}

std::string redfishPost(const std::string& subpath, std::string&& args) {
  if (subpath.starts_with("/redfish/v1")) {
    return hgx.post(HMC_BASE_URL + subpath, std::move(args), false);
  }
  return hgx.post(HMC_URL + subpath, std::move(args), false);
}

std::string redfishPatch(const std::string& subpath, std::string&& args) {
  if (subpath.starts_with("/redfish/v1")) {
    return hgx.patch(HMC_BASE_URL + subpath, std::move(args));
  }
  return hgx.patch(HMC_URL + subpath, std::move(args));
}

int setFWVersion() {
  const auto exp = "UpdateService/FirmwareInventory?$expand=*($levels=1)";

  if (getConfig() == GPU_CONFIG_HGX && getHMCPhase() < HMCPhase::BMC_FW_DVT) {
    return -1;
  }

  json resp = json::parse(redfishGet(exp));
  for(const auto& comp : resp["Members"]) {
    if (!comp.contains("Id") || !comp.contains("Version")) {
      return -1;
    }

    auto& fp = comp["Id"];
    auto& ver = comp["Version"];
    if (ver.dump() == "\"\"") {
      return -1;
    }

    if (getConfig() == GPU_CONFIG_HGX) {
      if (fp.dump().find("HGX_") != std::string::npos) {
        kv::set(fp, ver);
      }
    }
    else if (getConfig() == GPU_CONFIG_UBB) {
      if (comp.contains("VersionScheme") &&
          comp["VersionScheme"] == "OEM" &&
          comp.contains("Oem") &&
          comp["Oem"].contains("VersionID") &&
          comp["Oem"]["VersionID"].contains("ComponentDetails")) {
        auto& verDetails = comp["Oem"]["VersionID"]["ComponentDetails"];
        kv::set(fp, verDetails);
      }
      else {
        kv::set(fp, ver);
      }
    }
  }
  return 0;
}

std::string version(const std::string& comp, bool returnJson) {
  std::string url = HMC_FW_INVENTORY + comp;

  if (returnJson) {
    std::string respStr = hgx.get(url);
    return respStr;
  }

  try {
    auto ver = kv::get(comp);
    return ver;
  } catch (std::filesystem::filesystem_error& e) {
    hgx::setFWVersion();
  }

  try {
    json resp = json::parse(hgx.get(url));
    if (getConfig() == GPU_CONFIG_UBB) {
      if (resp.contains("VersionScheme") && resp["VersionScheme"] == "OEM") {
        return resp["Oem"]["VersionID"]["ComponentDetails"];
      }
    }
    return resp["Version"];
  } catch(const std::exception& e) {
    return "NA";
  }
}

std::string updateNonBlocking(const std::string& comp, const std::string& path, bool returnJson) {
  std::string url = HMC_UPDATE_SERVICE;
  std::string fp = path;

  if (comp != "") {
    const std::string targetsPath = "/redfish/v1/UpdateService/FirmwareInventory/";
    json targetJson;
    targetJson["HttpPushUriTargets"] = json::array({targetsPath + comp});
    std::string target = targetJson.dump();
    std::cout << "target: " << target << std::endl;
    hgx.patch(url, std::move(target));
  }

  std::string respStr = hgx.post(url, std::move(fp), true);
  if (returnJson) {
    return respStr;
  }
  json resp = json::parse(respStr);
  return resp["Id"];
}

HMCPhase getHMCPhase() {
  static HMCPhase phase = HMCPhase::HMC_FW_UNKNOWN;
  // TODO Try to do this with one Get at the root.
  auto tryPhase = [](const std::string& url) {
    try {
      hgx.get(url);
      return true;
    } catch (std::exception&) {
      return false;
    }
  };
  if (phase != HMCPhase::HMC_FW_UNKNOWN) {
    return phase;
  }
  if (tryPhase(HMC_FW_INVENTORY + "HGX_FW_BMC_0")) {
    phase = HMCPhase::BMC_FW_DVT;
  } else if (tryPhase(HMC_FW_INVENTORY + "HGX_FW_HMC_0")) {
    phase = HMCPhase::HMC_FW_DVT;
  } else if (tryPhase(HMC_FW_INVENTORY + "HMC_Firmware")) {
    phase = HMCPhase::HMC_FW_EVT;
  }
  return phase;
}

TaskStatus getTaskStatus(const std::string& id) {
  std::string url = HMC_TASK_SERVICE + id;
  TaskStatus status;
  status.resp = hgx.get(url);
  json resp = json::parse(status.resp);
  resp.at("TaskState").get_to(status.state);
  status.status = resp.value("TaskStatus", "Unknown");
  for (auto& j : resp.at("Messages")) {
    status.messages.emplace_back(j.at("Message"));
  }
  return status;
}

bool containStr(const std::string& str, const std::initializer_list<std::string>& substrings) {
  for (const auto& substr : substrings) {
    if (str.find(substr) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void getMetricReports() {
  std::string url, urlUBB;
  std::string snr_path;
  std::string snr_val;
  std::string snr_valid = "gpu_snr_valid";
  std::string resp;
  unsigned int pos_start, pos_end;

  if (get_gpu_config() == GPU_CONFIG_HGX) {
    url = HGX_TELEMETRY_SERVICE_DVT;
  }
  else if (get_gpu_config() == GPU_CONFIG_UBB) {
    url = HMC_URL + "TelemetryService/MetricReports/All";
    urlUBB = HMC_URL + "Chassis/UBB";
    json jUBB = json::parse(hgx.get(urlUBB));
    if (jUBB.contains("Status") && jUBB["Status"].contains("State")) {
      if(jUBB["Status"]["State"].dump() == "\"Enabled\"") {
        kv::set(snr_valid, "valid");
      }
      else {
       kv::set(snr_valid, jUBB["Status"]["State"].dump());
      }
    }
    else {
      kv::set(snr_valid, "valid");
    }
  }
  else {
    return;
  }

  resp = hgx.get(url);
  json jresp = json::parse(resp);
  json &tempArray = jresp["MetricValues"];
  for(auto &x : tempArray)
  {
    auto jname = x.find("MetricProperty");
    snr_path = jname.value();
    std::string upperStr;

    if (snr_path.find("#") != std::string::npos) {
      continue;
    }
    else if (snr_path.find("Reading") != std::string::npos) {
      pos_end = snr_path.find_last_of("/\\");
      pos_start = snr_path.find_last_of("/\\", pos_end - 1);
      snr_path = snr_path.substr(pos_start + 1, pos_end - pos_start -1);
    }
    else {
      pos_start = snr_path.find_last_of("/\\");
      snr_path = snr_path.substr(pos_start + 1);
    }

    upperStr.resize(snr_path.size());
    std::transform(snr_path.begin(), snr_path.end(), upperStr.begin(), ::toupper);

    if (!containStr(upperStr, {"TEMP", "POWER", "ENERGY"})) {
      continue;
    }

    if (get_gpu_config() == GPU_CONFIG_HGX) {
      if (x.contains("Oem") && x["Oem"].contains("Nvidia") &&
          x["Oem"]["Nvidia"].contains("MetricValueStale")) {
        if (x["Oem"]["Nvidia"]["MetricValueStale"].dump() == "true") {
          kv::set(snr_path, "NA");
          continue;
        }
      }
    }

    auto jvalue = x.find("MetricValue");
    if (jvalue.value().is_null()) {
      continue;
    }
    else {
      snr_val = jvalue.value();
    }

    pos_start = snr_val.find_first_not_of("0123456789");
    if (pos_start != 0) {
      kv::set(snr_path, snr_val);
    }
    else {
      kv::set(snr_path, "");
    }
  }
}

void factoryReset() {
  static const std::map<HMCPhase, std::string> urlMap = {
    {HMCPhase::HMC_FW_EVT, HMC_URL + "Managers/bmc" + HMC_FACTORY_RESET_SERVICE},
    {HMCPhase::HMC_FW_DVT, HMC_URL + "Managers/HGX_HMC_0" + HMC_FACTORY_RESET_SERVICE},
    {HMCPhase::BMC_FW_DVT, HMC_URL + "Managers/HGX_BMC_0" + HMC_FACTORY_RESET_SERVICE}
  };
  std::string url = urlMap.at(getHMCPhase());
  hgx.post(url, json::object({{"ResetToDefaultsType", "ResetAll"}}).dump(), false);
}

void reset() {
  static const std::map<HMCPhase, std::string> urlMap = {
    {HMCPhase::HMC_FW_EVT, HMC_URL + "Managers/bmc" + HMC_RESET_SERVICE},
    {HMCPhase::HMC_FW_DVT, HMC_URL + "Managers/HGX_HMC_0" + HMC_RESET_SERVICE},
    {HMCPhase::BMC_FW_DVT, HMC_URL + "Managers/HGX_BMC_0" + HMC_RESET_SERVICE}
  };
  std::string url = urlMap.at(getHMCPhase());
  hgx.post(url, json::object({{"ResetType", "GracefulRestart"}}).dump(), false);
}

void patch_bf_update() {
  static const std::map<HMCPhase, const std::vector<std::string>*> patchMap = {
    {HMCPhase::HMC_FW_EVT, &HMC_PATCH_TARGETS_EVT},
    {HMCPhase::HMC_FW_DVT, &HMC_PATCH_TARGETS_DVT},
    {HMCPhase::BMC_FW_DVT, &BMC_PATCH_TARGETS_DVT}
  };
  json patchJson = json::object({{"HttpPushUriTargets", json::array()}});
  for (const auto& target : *patchMap.at(getHMCPhase())) {
    auto& j = patchJson["HttpPushUriTargets"];
    j.push_back("/redfish/v1/UpdateService/FirmwareInventory/" + target);
  }
  hgx.patch(HMC_UPDATE_SERVICE, patchJson.dump());
  auto jresp = json::parse(hgx.get(HMC_UPDATE_SERVICE));
  std::cout << "Patching: " << std::endl \
            << jresp["HttpPushUriTargets"] << std::endl;
}

std::string dumpNonBlocking(DiagnosticDataType type) {
  std::string url;
  const std::map<DiagnosticDataType, std::string> typeMap = {
      {DiagnosticDataType::MANAGER, "Manager"},
      {DiagnosticDataType::OEM_EROT, "EROT"},
      {DiagnosticDataType::OEM_SELF_TEST, "SelfTest"},
      {DiagnosticDataType::OEM_FPGA, "FPGA"},
      {DiagnosticDataType::OEM_RETIMER, "RetLTSSM"}};
  json req;
  if (type == DiagnosticDataType::MANAGER) {
    req["DiagnosticDataType"] = "Manager";
    url = HMC_DUMP_SERVICE;
  } else {
    url = SYSTEM_DUMP_SERVICE;
    req["DiagnosticDataType"] = "OEM";
    req["OEMDiagnosticDataType"] = "DiagnosticType=" + typeMap.at(type);
  }
  std::string respStr = hgx.post(url, req.dump(), false);
  json resp = json::parse(respStr);
  return resp["Id"];
}

TaskStatus waitTask(const std::string& taskID, bool verbose = true) {
  using namespace std::chrono_literals;
  size_t nextMessage = 0;
  for (int retry = 0; retry < 500; retry++) {
    TaskStatus status = getTaskStatus(taskID);
    if (verbose) {
      for (; nextMessage < status.messages.size(); nextMessage++) {
        std::cout << status.messages[nextMessage++] << std::endl;
      }
    }
    if (status.state != "Running") {
      return status;
    }
    std::this_thread::sleep_for(5s);
  }
  throw std::runtime_error("Timeout");
}

std::string findTaskPayloadLocation(const TaskStatus& status) {
  json taskResp = json::parse(status.resp);
  for (const std::string hdr : taskResp["Payload"]["HttpHeaders"]) {
    auto sepLoc = hdr.find(": ");
    if (sepLoc == hdr.npos) {
      continue;
    }
    std::string tag = hdr.substr(0, sepLoc);
    std::string val = hdr.substr(sepLoc + 2, hdr.length());
    if (tag == "Location") {
      return val;
    }
  }
  throw std::runtime_error("Header did not contain a location!");
}

void retrieveDump(const std::string& taskID, const std::string& path) {
  using namespace std::chrono_literals;
  TaskStatus status = waitTask(taskID);
  std::string loc = findTaskPayloadLocation(status);
  std::cout << "Task Additional Data: " << loc << std::endl;
  json dumpResp = json::parse(hgx.get(HMC_BASE_URL + loc));
  if (!dumpResp.contains("AdditionalDataURI")) {
    throw std::runtime_error(
        "Task result location does not contain an AdditionalDataURI");
  }
  std::string attachmentURL = dumpResp["AdditionalDataURI"];
  std::cout << "Getting attachment from: " << attachmentURL << std::endl;
  std::string attachment = hgx.get(HMC_BASE_URL + attachmentURL);
  std::ofstream outfile(path, std::ofstream::binary);
  outfile.write(&attachment[0], attachment.size());
}

void dump(DiagnosticDataType type, const std::string& path) {
  std::string taskID = dumpNonBlocking(type);
  retrieveDump(taskID, path);
}

int update(const std::string& comp, const std::string& path) {
  std::string taskID = updateNonBlocking(comp, path);
  std::cout << "Started update task: " << taskID << std::endl;
  TaskStatus status = waitTask(taskID);
  std::cout << "Update completed with state: " << status.state
            << " status: " << status.status << std::endl;
  if (status.state != "Completed") {
    return -1;
  }
  return 0;
}

void printEventLog(std::ostream& os, bool jsonFmt) {
  std::string url =
      HMC_URL + "Systems/HGX_Baseboard_0/LogServices/EventLog/Entries";
  json entries = json::parse(hgx.get(url));
  if (jsonFmt) {
    os << entries["Members"].dump();
    return;
  }
  for (const auto& entry : entries["Members"]) {
    auto& time = entry["Created"];
    auto& msg = entry["Message"];
    auto& resolution = entry["Resolution"];
    bool resolved = entry["Resolved"];
    auto& severity = entry["Severity"];
    auto res_str = resolved ? "RESOLVED" : "UNRESOLVED";
    os << time << '\t' << severity << '\t' << res_str << '\t' << msg << '\t'
       << resolution << std::endl;
  }
}

void clearEventLog() {
  std::string url = HMC_URL +
      "Systems/HGX_Baseboard_0/LogServices/EventLog/Actions/LogService.ClearLog";
  json data = json::object();
  hgx.post(url, data.dump(), false);
}

void syncTime() {
  std::string url;

  if (get_gpu_config() == GPU_CONFIG_HGX) {
    url = HMC_URL + "Managers/HGX_BMC_0";
  } else if (get_gpu_config() == GPU_CONFIG_UBB) {
    url = HMC_URL + "Managers/AMC";
  } else {
    return;
  }

  json data = json::object({{"DateTime", hgx::time::now()}});
  hgx.patch(url, data.dump());
}

std::string sensorRaw(const std::string& component, const std::string& name) {
  constexpr auto fru = "Chassis";
  const std::string url = HMC_URL + fru + "/" + component + "/Sensors/" + name;
  return hgx.get(url);
}

float sensor(const std::string& component, const std::string& name) {
  std::string str = sensorRaw(component, name);
  json resp = json::parse(str);
  float val;
  resp.at("Reading").get_to(val);
  return val;
}

std::vector<std::string> integrityComponents() {
  json resp = json::parse(hgx.get(HMC_URL + "ComponentIntegrity"));
  std::vector<std::string> comps{};
  for (auto& comp : resp.at("Members")) {
    std::string url = comp.at("@odata.id");
    comps.push_back(url.substr(url.find_last_of('/') + 1));
  }
  return comps;
}

std::string getMeasurement(const std::string& comp) {
  std::string url = HMC_URL + "ComponentIntegrity/" + comp;
  json respMember = json::parse(hgx.get(url));
  std::string targetURL = respMember.at("Actions")
    .at("#ComponentIntegrity.SPDMGetSignedMeasurements")
    .at("target");
  json data = json::object();
  // TODO we can potentially provide parameters
  // {"SlotID": 0, "MeasurementIndices": [1], "Nonce": "<hash>"}
  json respTask = json::parse(hgx.post(HMC_BASE_URL + targetURL, data.dump(), false));
  std::string taskID = respTask.at("Id");
  TaskStatus status = waitTask(taskID, false);
  std::string loc = findTaskPayloadLocation(status);
  return hgx.get(HMC_BASE_URL + loc);
}

std::string setPowerLimit(int gpuID, std::string pwrLimit) {
  std::string url = HMC_URL +
      "Systems/HGX_Baseboard_0/Processors/GPU_SXM_"
       + std::to_string(gpuID) + "/EnvironmentMetrics";

  json patchJson = json::object();
  patchJson["PowerLimitWatts"]["SetPoint"] = std::stoi(pwrLimit);

  return hgx.patch(url, patchJson.dump());
}

std::string getPowerLimit(int gpuID) {
  long pwrLimitDec = 0;
  int low = 0, mid = 0, high = 0;
  std::string pwrLimitLow, pwrLimitMid, pwrLimitHigh;

  std::string url = HMC_URL +
  "Managers/HGX_BMC_0/Actions/Oem/NvidiaManager.AsyncOOBRawCommand";

  json jsonObject = {
      {"TargetType", "GPU"},
      {"TargetInstanceId", gpuID},
      {"AsyncArg1", "0x00"},
      {"AsyncDataIn", {"00", "00", "00", "00", "00", "00", "00", "00"}},
      {"RequestedDataOutBytes", 16}
  };

  json jresp = json::parse(hgx.post(url, jsonObject.dump(), false));

  if (jresp.contains("AsyncDataOut") && jresp["AsyncDataOut"].is_array()) {
    json asyncDataOut = jresp["AsyncDataOut"];
    if (asyncDataOut.size() == 16) {
      pwrLimitLow =  asyncDataOut[8];
      pwrLimitMid = asyncDataOut[9];
      pwrLimitHigh =  asyncDataOut[10];
      pwrLimitLow.erase(0, 2);
      pwrLimitMid.erase(0, 2);
      pwrLimitHigh.erase(0, 2);

      low = std::stoi(pwrLimitLow, nullptr, 16);
      mid = std::stoi(pwrLimitMid, nullptr, 16);
      high = std::stoi(pwrLimitHigh, nullptr, 16);
      pwrLimitDec = ((high << 16) | (mid << 8) | low)/1000;
    }
  }
  return std::to_string(pwrLimitDec);
}

std::tuple<std::string, std::string> getGpuCompName(int gpu_config) {
  switch (gpu_config) {
    case GPU_CONFIG_HGX:
      return { "HGX", "HMC" };
      break;
    case GPU_CONFIG_UBB:
      return { "UBB" , "SMC" };
      break;
    default:
      return { "Unknown GPU", "" };
  }
}

} // namespace hgx.

int get_hgx_sensor(const char* component, const char* snr_name, float* value) {
  try {
    *value = hgx::sensor(component, snr_name);
  } catch (std::exception& e) {
    return -1;
  }
  return 0;
}

int get_hgx_ver(const char* component, char *version) {
  try {
    std::string resStr;
    resStr = hgx::version(component, 0);
    strcpy(version, resStr.c_str());
  } catch (std::exception& e) {
    return -1;
  }
  return 0;
}

int hgx_get_metric_reports() {
  try {
      hgx::getMetricReports();
  } catch (std::exception& e) {
    return -1;
  }
  return 0;
}

HMCPhase get_hgx_phase() {
  HMCPhase phase;
  try {
    phase = hgx::getHMCPhase();
  } catch (std::exception& e) {
    phase = HMCPhase::HMC_FW_UNKNOWN;
  }
  return phase;
}

GPUConfig get_gpu_config() {
  GPUConfig config;
  try {
    config = hgx::getConfig();
  } catch (std::exception&) {
    config = GPU_CONFIG_UNKNOWN;
  }
  return config;
}

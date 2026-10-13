#include "registry.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>      // for std::find

namespace fs = std::filesystem;

static std::string getSourceFileDir() {
    std::string file(__FILE__);
    size_t pos = file.find_last_of("/\\");
    if (pos != std::string::npos) return file.substr(0, pos);
    return ".";
}

NameRegistry::NameRegistry() {
    jsonPath = fs::path(getSourceFileDir()) / "clip_registry.json";
    load();
}

void NameRegistry::parseJson(const std::string& content) {
    usedIds.clear();
    size_t pos = 0;
    while (pos < content.size()) {
        size_t q1 = content.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string key = content.substr(q1 + 1, q2 - q1 - 1);

        size_t bracket = content.find('[', q2);
        if (bracket == std::string::npos) break;
        size_t close = content.find(']', bracket);
        if (close == std::string::npos) break;

        std::string nums = content.substr(bracket + 1, close - bracket - 1);
        std::stringstream ss(nums);
        int id;
        while (ss >> id) {
            usedIds[key].push_back(id);
            if (ss.peek() == ',') ss.ignore();
        }
        pos = close + 1;
    }
}

std::string NameRegistry::escapeJsonString(const std::string& s) const {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

void NameRegistry::load() {
    std::ifstream f(jsonPath);
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    parseJson(content);
}

void NameRegistry::save() const {
    std::ofstream f(jsonPath);
    if (!f.is_open()) {
        std::cerr << "Warning: Could not write registry to " << jsonPath << std::endl;
        return;
    }
    f << "{\n";
    bool firstEntry = true;
    for (const auto& [name, ids] : usedIds) {
        if (!firstEntry) f << ",\n";
        f << "    \"" << escapeJsonString(name) << "\": [";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) f << ", ";
            f << ids[i];
        }
        f << "]";
        firstEntry = false;
    }
    f << "\n}\n";
}

int NameRegistry::allocateId(const std::string& videoFilename) {
    auto& ids = usedIds[videoFilename];
    int id = 1;
    while (std::find(ids.begin(), ids.end(), id) != ids.end()) ++id;
    ids.push_back(id);
    save();
    return id;
}

std::string NameRegistry::generateUniqueName(const std::string& videoPath) {
    fs::path p(videoPath);
    std::string filename = p.filename().string();
    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    if (ext.empty()) ext = ".mp4";

    int id = allocateId(filename);
    std::ostringstream oss;
    oss << stem << "_" << std::setw(3) << std::setfill('0') << id << ext;
    return oss.str();
}

const std::string& NameRegistry::getJsonPath() const { return jsonPath; }
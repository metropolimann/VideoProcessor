#ifndef REGISTRY_H
#define REGISTRY_H

#include <string>
#include <vector>
#include <unordered_map>

class NameRegistry {
public:
    NameRegistry();
    void load();
    void save() const;
    int allocateId(const std::string& videoFilename);
    std::string generateUniqueName(const std::string& videoPath);
    const std::string& getJsonPath() const;

private:
    std::string jsonPath;
    std::unordered_map<std::string, std::vector<int>> usedIds;
    void parseJson(const std::string& content);
    std::string escapeJsonString(const std::string& s) const;
};

#endif // REGISTRY_H
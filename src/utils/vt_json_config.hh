/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * vt_json_config.hh - JSON configuration helper
 * Provides easy-to-use JSON config reading/writing for ViewTouch
 */

#ifndef VT_JSON_CONFIG_HH
#define VT_JSON_CONFIG_HH

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <filesystem>

namespace vt {

// Convenience alias
using json = nlohmann::json;

/**
 * @brief JSON configuration helper for ViewTouch
 * 
 * Provides simple API for reading/writing JSON config files with:
 * - Safe value retrieval with defaults
 * - Pretty printing
 * - Validation
 * - Automatic backups
 * 
 * Usage:
 *   JsonConfig cfg("/usr/viewtouch/dat/conf/settings.json");
 *   if (cfg.Load()) {
 *       auto store_name = cfg.Get<std::string>("store_name", "My Restaurant");
 *       auto tax_rate = cfg.Get<double>("tax_food", 0.07);
 *   }
 */
class JsonConfig {
public:
    /**
     * @brief Construct a JSON config handler
     * @param filepath Path to the JSON config file
     */
    explicit JsonConfig(std::string_view filepath);

    /**
     * @brief Load JSON from file
     * @return true if successful, false if file doesn't exist or is invalid
     */
    bool Load();

    /**
     * @brief Save JSON to file
     * @param pretty_print Whether to format with indentation (default: true)
     * @param create_backup Create backup before saving (default: true)
     * @return true if successful
     */
    bool Save(bool pretty_print = true, bool create_backup = true);

    /**
     * @brief Get a value from the config with optional default
     * @tparam T Type of value to retrieve
     * @param key JSON key (can use nested keys like "network.timeout")
     * @param default_value Default value if key doesn't exist
     * @return The value or default
     */
    template<typename T>
    [[nodiscard]] [[nodiscard]] T Get(std::string_view key, const T& default_value = T{}) const {
        try {
            auto keys = SplitKey(key);
            const json* current = &data_;
            
            for (const auto& k : keys) {
                if (current->contains(k)) {
                    current = &(*current)[k];
                } else {
                    return default_value;
                }
            }
            
            return current->get<T>();
        } catch (...) {
            return default_value;
        }
    }

    /**
     * @brief Set a value in the config
     * @tparam T Type of value to set
     * @param key JSON key (can use nested keys like "network.timeout")
     * @param value Value to set
     */
    template<typename T>
    void Set(std::string_view key, const T& value) {
        auto keys = SplitKey(key);
        json* current = &data_;
        
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                (*current)[keys[i]] = json::object();
            }
            current = &(*current)[keys[i]];
        }
        
        (*current)[keys.back()] = value;
    }

    /**
     * @brief Check if a key exists
     */
    [[nodiscard]] bool Has(std::string_view key) const;

    /**
     * @brief Remove a key from the config
     */
    bool Remove(std::string_view key);

    /**
     * @brief Get direct access to underlying JSON object
     */
    json& Data() { return data_; }
    [[nodiscard]] const json& Data() const { return data_; }

    /**
     * @brief Get the file path
     */
    [[nodiscard]] const std::string& GetPath() const { return filepath_; }

    /**
     * @brief Check if config was loaded successfully
     */
    [[nodiscard]] bool IsLoaded() const { return loaded_; }

    /**
     * @brief Clear all data
     */
    void Clear() { data_ = json::object(); }

    /**
     * @brief Create example configuration file
     * @param filepath Path where to create example
     * @return true if successful
     */
    static bool CreateExample(std::string_view filepath);

private:
    std::string filepath_;
    json data_;
    bool loaded_;

    /**
     * @brief Split nested key like "network.timeout" into ["network", "timeout"]
     */
    static std::vector<std::string> SplitKey(std::string_view key);

    /**
     * @brief Create backup of config file
     */
    [[nodiscard]] bool CreateBackup() const;
};

/**
 * @brief Load JSON from file (convenience function)
 */
std::optional<json> LoadJsonFile(std::string_view filepath);

/**
 * @brief Save JSON to file (convenience function)
 */
bool SaveJsonFile(std::string_view filepath, const json& data, bool pretty_print = true);

} // namespace vt

#endif // VT_JSON_CONFIG_HH


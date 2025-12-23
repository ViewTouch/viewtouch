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
 * vt_json_config.cc - JSON configuration implementation
 */

#include "vt_json_config.hh"
#include <fstream>
#include <sstream>
#include <iostream>

namespace vt {

JsonConfig::JsonConfig(std::string_view filepath)
    : filepath_(filepath)
    , data_(json::object())
    , loaded_(false)
{
}

bool JsonConfig::Load() {
    try {
        std::ifstream file(filepath_);
        if (!file.is_open()) {
            return false;
        }

        file >> data_;
        loaded_ = true;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error in " << filepath_ << ": " << e.what() << '\n';
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading JSON config " << filepath_ << ": " << e.what() << '\n';
        return false;
    }
}

bool JsonConfig::Save(bool pretty_print, bool create_backup) {
    try {
        // Create backup if requested and file exists
        if (create_backup && std::filesystem::exists(filepath_)) {
            if (!CreateBackup()) {
                std::cerr << "Warning: Could not create backup of " << filepath_ << '\n';
            }
        }

        // Ensure directory exists
        auto parent = std::filesystem::path(filepath_).parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream file(filepath_);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filepath_ << " for writing" << '\n';
            return false;
        }

        if (pretty_print) {
            file << data_.dump(4); // 4-space indentation
        } else {
            file << data_.dump();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving JSON config " << filepath_ << ": " << e.what() << '\n';
        return false;
    }
}

bool JsonConfig::Has(std::string_view key) const {
    try {
        auto keys = SplitKey(key);
        const json* current = &data_;
        
        for (const auto& k : keys) {
            if (current->contains(k)) {
                current = &(*current)[k];
            } else {
                return false;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool JsonConfig::Remove(std::string_view key) {
    try {
        auto keys = SplitKey(key);
        if (keys.empty()) return false;

        json* current = &data_;
        
        // Navigate to parent
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                return false;
            }
            current = &(*current)[keys[i]];
        }
        
        // Remove the final key
        return current->erase(keys.back()) > 0;
    } catch (...) {
        return false;
    }
}

bool JsonConfig::CreateExample(std::string_view filepath) {
    try {
        json example = {
            {"store_name", "My Restaurant"},
            {"store_address", "123 Main St"},
            {"region", "US"},
            {"tax", {
                {"food", 0.07},
                {"alcohol", 0.09},
                {"merchandise", 0.065}
            }},
            {"network", {
                {"terminals", json::array({
                    {{"id", 1}, {"name", "Front Counter"}, {"display", ":0.0"}},
                    {{"id", 2}, {"name", "Kitchen"}, {"display", ":0.1"}}
                })},
                {"printers", {
                    {"kitchen", "192.168.1.100"},
                    {"receipts", "192.168.1.101"}
                }}
            }},
            {"settings", {
                {"screen_blank_time", 300},
                {"language", "en_US"},
                {"use_seats", true},
                {"price_rounding", 2}
            }}
        };

        std::ofstream file(filepath.data());
        if (!file.is_open()) {
            return false;
        }

        file << example.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating example config: " << e.what() << '\n';
        return false;
    }
}

std::vector<std::string> JsonConfig::SplitKey(std::string_view key) {
    std::vector<std::string> result;
    std::stringstream ss(key.data());
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

bool JsonConfig::CreateBackup() const {
    try {
        if (!std::filesystem::exists(filepath_)) {
            return false;
        }

        std::string backup_path = filepath_ + ".backup";
        std::filesystem::copy_file(
            filepath_,
            backup_path,
            std::filesystem::copy_options::overwrite_existing
        );
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Backup error: " << e.what() << '\n';
        return false;
    }
}

// Convenience functions
std::optional<json> LoadJsonFile(std::string_view filepath) {
    try {
        std::ifstream file(filepath.data());
        if (!file.is_open()) {
            return std::nullopt;
        }

        json data;
        file >> data;
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

bool SaveJsonFile(std::string_view filepath, const json& data, bool pretty_print) {
    try {
        std::ofstream file(filepath.data());
        if (!file.is_open()) {
            return false;
        }

        if (pretty_print) {
            file << data.dump(4);
        } else {
            file << data.dump();
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace vt


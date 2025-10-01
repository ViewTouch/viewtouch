//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//

#include "conf_file.hh"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <locale>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr std::string_view comment_indicators{";#"};
constexpr std::string_view equal_indicators{"=:"};
constexpr std::string_view whitespace{" 	\n\r"};
constexpr char default_assignment_char = '=';

[[nodiscard]] constexpr bool is_trim_char(char ch) noexcept
{
    return whitespace.find(ch) != std::string_view::npos ||
           equal_indicators.find(ch) != std::string_view::npos;
}

void trim_in_place(std::string& str)
{
    const auto first = std::find_if_not(str.begin(), str.end(), is_trim_char);
    if (first == str.end())
    {
        str.clear();
        return;
    }

    const auto last = std::find_if_not(str.rbegin(), str.rend(), is_trim_char).base();
    str.assign(first, last);
}

[[nodiscard]] bool equals_ignore_case(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    const auto to_lower = [](unsigned char c) noexcept {
        return static_cast<char>(std::tolower(c));
    };

    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                      [&](char a, char b) {
                          return to_lower(static_cast<unsigned char>(a)) ==
                                 to_lower(static_cast<unsigned char>(b));
                      });
}
} // namespace

ConfFile::ConfFile(std::string file, const bool load)
    : fileName(std::move(file))
{
    if (load && !Load())
    {
        throw std::runtime_error("ConfFile: error loading file: " + fileName);
    }
}

ConfFile::~ConfFile()
{
    if (dirty)
    {
        static_cast<void>(Save());
    }
    dirty = false;
}

void ConfFile::set_dirty(const bool value) noexcept
{
    dirty = value;
}

bool ConfFile::Load()
{
    std::ifstream file(fileName);
    if (!file.is_open())
    {
        return false;
    }

    section_names.clear();
    section_names.emplace_back(); // default section
    data.clear();
    data.emplace_back();

    std::string line;
    std::string section_name;

    while (std::getline(file, line))
    {
        trim_in_place(line);

        if (line.empty())
        {
            continue;
        }

        if (comment_indicators.find(line.front()) != std::string_view::npos)
        {
            continue;
        }

        if (line.front() == '[')
        {
            line.erase(0, 1);
            if (const auto closing_bracket = line.find(']'); closing_bracket != std::string::npos)
            {
                line.erase(closing_bracket);
                trim_in_place(line);
                section_name = line;
                CreateSection(section_name);
            }
            continue;
        }

        std::string value;
        if (const auto split = line.find_first_of(equal_indicators);
            split != std::string::npos && split > 0)
        {
            value = line.substr(split + 1);
            trim_in_place(value);
            line.erase(split);
        }

        trim_in_place(line);

        if (!line.empty())
        {
            SetValue(value, line, section_name);
        }
    }

    if (file.bad())
    {
        return false;
    }

    dirty = false;
    return true;
}

bool ConfFile::Save()
{
    if ((KeyCount() == 0) && (SectionCount() == 0))
    {
        return false;
    }

    std::ofstream file(fileName, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        std::cerr << "ConfFile::Save: file open failure (" << fileName << ")\n";
        return false;
    }

    assert(section_names.size() == data.size());

    for (size_t i = 0; i < section_names.size(); ++i)
    {
        const auto& section_name = section_names[i];
        const auto& entries = data[i];

        if (!section_name.empty())
        {
            file << '\n' << '[' << section_name << ']' << '\n';
        }

        for (const auto& [key, value] : entries)
        {
            assert(!key.empty());
            file << key << default_assignment_char << value << '\n';
        }
    }

    dirty = false;
    return true;
}

bool ConfFile::SetValue(const std::string& value, std::string_view keyName, std::string_view sectName)
{
    if (value.empty() || keyName.empty())
    {
        return false;
    }

    if (!contains(sectName) && !CreateSection(sectName))
    {
        return false;
    }

    try
    {
        SectionEntries& section = at(sectName);
        for (auto& entry : section)
        {
            if (entry.first == keyName)
            {
                entry.second = value;
                dirty = true;
                return true;
            }
        }

        section.emplace_back(std::string{keyName}, value);
        dirty = true;
        return true;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
}

bool ConfFile::SetValue(const double value, std::string_view key, std::string_view section)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    return SetValue(stream.str(), key, section);
}

bool ConfFile::SetValue(const int value, std::string_view key, std::string_view section)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    return SetValue(stream.str(), key, section);
}

bool ConfFile::GetValue(std::string& value, std::string_view keyName, std::string_view sectName) const
{
    if (keyName.empty())
    {
        return false;
    }

    const auto index = find_section_index(sectName);
    if (!index)
    {
        return false;
    }

    const auto& section = data[*index];
    const auto it = std::find_if(section.begin(), section.end(),
                                 [&](const SectionEntry& entry) {
                                     return entry.first == keyName;
                                 });

    if (it == section.end())
    {
        return false;
    }

    value = it->second;
    return true;
}

bool ConfFile::GetValue(double& value, std::string_view key, std::string_view section) const
{
    std::string raw_value;
    if (!GetValue(raw_value, key, section) || raw_value.empty())
    {
        return false;
    }

    std::istringstream stream(raw_value);
    stream.imbue(std::locale::classic());
    stream >> value;
    if (stream.fail())
    {
        try
        {
            value = std::stod(raw_value);
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
    }
    return true;
}

bool ConfFile::GetValue(int& value, std::string_view key, std::string_view section) const
{
    std::string raw_value;
    if (!GetValue(raw_value, key, section) || raw_value.empty())
    {
        return false;
    }

    std::istringstream stream(raw_value);
    stream.imbue(std::locale::classic());
    stream >> value;
    return !stream.fail();
}

std::optional<std::string> ConfFile::TryGetValue(std::string_view key, std::string_view section) const
{
    std::string value;
    if (GetValue(value, key, section))
    {
        return value;
    }
    return std::nullopt;
}

bool ConfFile::DeleteSection(std::string_view section)
{
    if (section.empty())
    {
        return false;
    }

    for (size_t i = 0; i < section_names.size(); ++i)
    {
        if (equals_ignore_case(section_names[i], section))
        {
            data.erase(data.begin() + static_cast<std::ptrdiff_t>(i));
            section_names.erase(section_names.begin() + static_cast<std::ptrdiff_t>(i));
            dirty = true;
            assert(section_names.size() == data.size());
            return true;
        }
    }
    return false;
}

bool ConfFile::DeleteKey(std::string_view key, std::string_view sectName)
{
    if (key.empty())
    {
        return false;
    }

    const auto index = find_section_index(sectName);
    if (!index)
    {
        return false;
    }

    auto& section = data[*index];
    const auto it = std::find_if(section.begin(), section.end(),
                                 [&](const SectionEntry& entry) {
                                     return equals_ignore_case(entry.first, key);
                                 });

    if (it == section.end())
    {
        return false;
    }

    section.erase(it);
    dirty = true;
    return true;
}

bool ConfFile::CreateSection(std::string_view sectName)
{
    if (contains(sectName))
    {
        return false;
    }

    section_names.emplace_back(sectName);
    data.emplace_back();
    dirty = true;

    assert(section_names.size() == data.size());
    return true;
}

size_t ConfFile::SectionCount() const noexcept
{
    return section_names.size();
}

size_t ConfFile::KeyCount() const noexcept
{
    return std::accumulate(data.begin(), data.end(), size_t{0},
                           [](size_t total, const SectionEntries& section) {
                               return total + section.size();
                           });
}

const std::vector<std::string>& ConfFile::getSectionNames() const noexcept
{
    return section_names;
}

std::vector<std::string> ConfFile::keys(std::string_view sectName) const
{
    const auto index = find_section_index(sectName);
    if (!index)
    {
        throw std::out_of_range("ConfFile: section not found: " + std::string(sectName));
    }

    const auto& section = data[*index];
    std::vector<std::string> key_names;
    key_names.reserve(section.size());

    for (const auto& entry : section)
    {
        key_names.push_back(entry.first);
    }

    return key_names;
}

const ConfFile::SectionEntries& ConfFile::at(std::string_view sectName) const
{
    const auto index = find_section_index(sectName);
    if (!index)
    {
        throw std::out_of_range("ConfFile: section not found: " + std::string(sectName));
    }
    return data[*index];
}

ConfFile::SectionEntries& ConfFile::at(std::string_view sectName)
{
    const auto index = find_section_index(sectName);
    if (!index)
    {
        throw std::out_of_range("ConfFile: section not found: " + std::string(sectName));
    }
    return data[*index];
}

bool ConfFile::contains(std::string_view section) const noexcept
{
    return find_section_index(section).has_value();
}

std::optional<size_t> ConfFile::find_section_index(std::string_view section) const noexcept
{
    if (section_names.empty())
    {
        return std::nullopt;
    }

    if (section.empty())
    {
        return size_t{0};
    }

    for (size_t idx = 0; idx < section_names.size(); ++idx)
    {
        if (section_names[idx] == section)
        {
            return idx;
        }
    }

    return std::nullopt;
}

/* done */

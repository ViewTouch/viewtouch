//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//

#ifndef CONF_FILE_HH
#define CONF_FILE_HH

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ConfFile
{
public:
    using SectionEntry = std::pair<std::string, std::string>;
    using SectionEntries = std::vector<SectionEntry>;

    explicit ConfFile(std::string fileName, bool load = false);
    ~ConfFile();

    void set_dirty(bool value = true) noexcept;

    [[nodiscard]] bool Load();
    [[nodiscard]] bool Save();

    [[nodiscard]] bool GetValue(std::string& value, std::string_view key, std::string_view section = {}) const;
    [[nodiscard]] bool GetValue(double& value, std::string_view key, std::string_view section = {}) const;
    [[nodiscard]] bool GetValue(int& value, std::string_view key, std::string_view section = {}) const;
    [[nodiscard]] std::optional<std::string> TryGetValue(std::string_view key, std::string_view section = {}) const;

    bool SetValue(const std::string& value, std::string_view key, std::string_view section = {});
    bool SetValue(double value, std::string_view key, std::string_view section = {});
    bool SetValue(int value, std::string_view key, std::string_view section = {});

    bool DeleteKey(std::string_view key, std::string_view section = {});
    bool DeleteSection(std::string_view section);
    bool CreateSection(std::string_view section);

    [[nodiscard]] size_t SectionCount() const noexcept;
    [[nodiscard]] size_t KeyCount() const noexcept;

    [[nodiscard]] const std::vector<std::string>& getSectionNames() const noexcept;
    [[nodiscard]] std::vector<std::string> keys(std::string_view section = {}) const;

    [[nodiscard]] const SectionEntries& at(std::string_view section) const;
    [[nodiscard]] bool contains(std::string_view section) const noexcept;

protected:
    SectionEntries& at(std::string_view section);

private:
    [[nodiscard]] std::optional<size_t> find_section_index(std::string_view section) const noexcept;

    const std::string fileName;
    std::vector<std::string> section_names{""};
    std::vector<SectionEntries> data{{}};
    bool dirty{false};
};

#endif // CONF_FILE_HH

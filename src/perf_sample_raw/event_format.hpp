#ifndef LO2S_EVENT_FORMAT_HPP
#define LO2S_EVENT_FORMAT_HPP

#include <vector>

#include <cstddef>

#include <boost/filesystem.hpp>

namespace lo2s
{

class event_field
{
public:
    event_field(const std::string& name, std::ptrdiff_t offset, std::size_t size)
    : name_(name), offset_(offset), size_(size)
    {
    }

    const std::string& name() const
    {
        return name_;
    }

    std::ptrdiff_t offset() const
    {
        return offset_;
    }

    std::size_t size() const
    {
        return size_;
    }

private:
    std::string name_;
    std::ptrdiff_t offset_;
    std::size_t size_;
};

class event_format
{
public:
    event_format(const std::string& name);

    int id() const
    {
        return id_;
    }

    const auto& common_fields() const
    {
        return common_fields_;
    }

    const auto& fields() const
    {
        return fields_;
    }

private:
    void parse_format_line(const std::string& line);

    std::string name_;
    int id_;
    std::vector<event_field> common_fields_;
    std::vector<event_field> fields_;

    const static boost::filesystem::path base_path_;
};
}
#endif // LO2S_EVENT_FORMAT_HPP

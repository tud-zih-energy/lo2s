#pragma once

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/util.hpp>

#include <map>
#include <memory>
#include <string>

extern "C"
{
#include <elfutils/libdwfl.h>
}

namespace lo2s
{
class DwarfFunctionResolver : public FunctionResolver
{
public:
    DwarfFunctionResolver(const std::string& name);

    static std::shared_ptr<FunctionResolver> cache(const std::string& name)
    {
        return BinaryCache<DwarfFunctionResolver>::instance()[name];
    }

    // _One_ DwarfFunctionResolver for every object file, globally
    DwarfFunctionResolver(DwarfFunctionResolver&) = delete;
    DwarfFunctionResolver& operator=(DwarfFunctionResolver&) = delete;
    DwarfFunctionResolver(DwarfFunctionResolver&&) = delete;
    DwarfFunctionResolver& operator=(DwarfFunctionResolver&&) = delete;

    ~DwarfFunctionResolver() override;
    LineInfo lookup_line_info(Address addr) override;

    std::string name()
    {
        return name_;
    }

private:
    std::map<Range, LineInfo> cache_;

    Dwfl_Callbacks cb;

    Dwfl* dwfl_ = nullptr;
    Dwfl_Module* mod_ = nullptr;
    std::string name_;
};
} // namespace lo2s

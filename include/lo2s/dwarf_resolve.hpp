#pragma once

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>

#include <map>
extern "C"
{
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <fcntl.h>
#include <unistd.h>
}

namespace lo2s
{
class DwarfFunctionResolver : public FunctionResolver
{
public:
    DwarfFunctionResolver(std::string name);

    static FunctionResolver& cache(std::string name)
    {
        return StringCache<DwarfFunctionResolver>::instance()[name];
    }

    ~DwarfFunctionResolver();
    virtual LineInfo lookup_line_info(Address addr) override;

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

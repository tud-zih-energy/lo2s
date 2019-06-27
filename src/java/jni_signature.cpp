/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2019,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lo2s/java/jni_signature.hpp>

#include <lo2s/log.hpp>

#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <cassert>

namespace lo2s
{
namespace java
{

std::vector<std::string> parse_type_list(const std::string& input)
{
    static std::regex internal_class_type("Ljava\\/lang\\/([^;\\/]+);");
    static std::regex class_type("L([^;]+);");

    auto it = input.begin();
    bool is_array = false;

    std::vector<std::string> result;

    while (it != input.end())
    {
        if (is_array)
        {
            assert(!result.empty());
            is_array = false;
            result.back() += "[]";
            continue;
        }

        switch (*it)
        {
        case 'Z':
            result.push_back("boolean");
            ++it;
            continue;
        case 'B':
            result.push_back("byte");
            ++it;
            continue;
        case 'C':
            result.push_back("char");
            ++it;
            continue;
        case 'S':
            result.push_back("short");
            ++it;
            continue;
        case 'I':
            result.push_back("int");
            ++it;
            continue;
        case 'J':
            result.push_back("long");
            ++it;
            continue;
        case 'F':
            result.push_back("float");
            ++it;
            continue;
        case 'D':
            result.push_back("double");
            ++it;
            continue;
        case 'V':
            result.push_back("void");
            ++it;
            continue;
        case '[':
            assert(!is_array);
            is_array = true;
            ++it;
            continue;
        default:
            break;
        }

        std::smatch m;
        if (std::regex_match(it, input.end(), m, internal_class_type,
                             std::regex_constants::match_continuous))
        {
            // this matches `java/lang/String` and appends just `String`
            result.push_back(m[1]);
            it += m.length();

            continue;
        }

        if (std::regex_match(it, input.end(), m, internal_class_type,
                             std::regex_constants::match_continuous))
        {
            // this matches any class
            std::string class_signature = m[1];

            for (auto& c : class_signature)
            {
                if (c == '/')
                {
                    c = '.';
                }
            }

            result.push_back(class_signature);
            it += m.length();

            continue;
        }

        Log::debug() << "Cannot parse the JNI type list: " << input;
        return { input };
    }

    return result;
}

std::string parse_signature(const std::string& input)
{
    static std::regex signature("\\(([^\\)]+)\\)(.*)");

    std::smatch m;
    if (std::regex_match(input.begin(), input.end(), m, signature))
    {
        auto arguments = parse_type_list(m[1]);
        auto return_type = parse_type(m[2]);

        std::stringstream str;

        str << "(";

        bool first = true;
        for (const auto& arg : arguments)
        {
            str << (first ? "" : ", ") << arg;
            first = false;
        }

        str << ") -> " << return_type;

        return str.str();
    }

    Log::debug() << "Cannot parse the JNI signature: " << input;

    return input;
}

std::string parse_type(const std::string& input)
{
    auto types = parse_type_list(input);
    assert(types.size() == 1);

    return types.front();
}

} // namespace java
} // namespace lo2s

#ifndef GENERATOR_TOOL_HPP
#define GENERATOR_TOOL_HPP

#include <string>

namespace vkgen
{

    class Generator;

    void toolScreen();

    namespace tools
    {
        void init(vkgen::Generator &gen);

        void analyzeCode(vkgen::Generator &gen, std::string_view dir);
    }  // namespace tools

}  // namespace vkgen

#endif  // GENERATOR_TOOL_HPP
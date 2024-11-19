#ifndef SHELL_HXX
#define SHELL_HXX

#include "parser.hxx"

namespace shell {
    Result<void> ExecuteCommandGroup(const parser::CommandGroup& cmd_group);
    std::string GetCurrentDir();
}

#endif // SHELL_HXX

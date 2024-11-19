#ifndef SHELL_HXX
#define SHELL_HXX

#include "parser.hxx"

namespace shell {
    Result<void> executeCommandGroup(const parser::CommandGroup& cmdGroup);
    void executeCommand(std::vector<parser::Command>::const_reverse_iterator cmd, const parser::CommandGroup& cmd_group, std::optional<int> fd);
    std::string get_current_dir();
}

#endif // SHELL_HXX

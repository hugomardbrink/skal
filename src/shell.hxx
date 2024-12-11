#ifndef SHELL_HXX_H_
#define SHELL_HXX_H_

#include "result.hxx"
#include "parser.hxx"

using namespace result;

namespace shell {
    Result<void> RunShell();
    Result<void> ExecuteCommandGroup(const parser::CommandGroup& cmd_group);
    Result<void> ExecuteCommandGroups(const parser::CommandGroups& cmd_groups);
}

#endif // SHELL_HXX_H_

#ifndef CORE_HXX_H_
#define CORE_HXX_H_

#include "parser.hxx"

namespace core {
    Result<void> ExecuteCommandGroup(const parser::CommandGroup& cmd_group);
    Result<std::string> GetCurrentDir();
}

#endif // CORE_HXX_H_

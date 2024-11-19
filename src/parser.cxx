#include <sstream>
#include <unordered_map>
#include <functional>

#include "parser.hxx"

using namespace parser;

namespace {
    std::unordered_map<string, std::function<Result<Command>(CommandGroup&, Command, std::stringstream&)>> tokenActions = {
        {"|", [](CommandGroup& cmdGroup, Command cmd, std::stringstream& _) -> Result<Command> {                     
            if(cmd.cmd == "") {
                return Error{"Parse error at '|'"};
            }
            cmdGroup.commands.emplace_back(cmd);
            return Result<Command>(Command{});
        }},
        {"<", [](CommandGroup& cmdGroup, Command cmd, std::stringstream& ss) -> Result<Command> {
            if(cmd.cmd == "") {
                return Error{"Parse error at '<'"};
            }

            std::string fileName;
            ss >> fileName;
            if (fileName == ""){
                return Error{"Parse error at '<'"};
            }

            cmdGroup.rstdin = std::make_optional(fileName);
            return Result<Command>(cmd);
        }},
        {">", [](CommandGroup& cmdGroup, Command cmd, std::stringstream& ss) -> Result<Command> {
            if(cmd.cmd == "") {
                return Error{"Parse error at '>'"};
            }

            std::string fileName;
            ss >> fileName;
            if (fileName == ""){
                return Error{"Parse error at '>'"};
            }

            cmdGroup.rstdout = std::make_optional(fileName);
            return Result<Command>(cmd);
        }},
        {"2>", [](CommandGroup& cmdGroup, Command cmd, std::stringstream& ss) -> Result<Command> {
            if(cmd.cmd == "") {
                return Error{"Parse error at '2>'"};
            }

            std::string fileName;
            ss >> fileName;
            if (fileName == ""){
                return Error{"Parse error at '2>'"};
            }

            cmdGroup.rstderr = std::make_optional(fileName);
            return Result<Command>(cmd);
        }},
        {"&", [](CommandGroup& cmdGroup, Command cmd, std::stringstream& _) -> Result<Command> {
            cmdGroup.isBackground = true;
            return Result<Command>(cmd);
        }}
    };
} // namespace

Result<CommandGroup> Parser::parse(string input) {
    CommandGroup cmdGroup;
    std::stringstream ss(input);
    string token;

    Command cmd;
    while (ss >> token) {
        if (tokenActions.find(token) != tokenActions.end()) {
            Result<Command> result = tokenActions[token](cmdGroup, cmd, ss);
            if (result.is_error()) {
                return result.unwrap_error();
            } else {
                cmd = result.unwrap();
            }
        } else {
            if(cmd.cmd.empty()) {
                cmd.cmd = token;
            } else {
                cmd.args.emplace_back(token);
            }
        } 
    }
    if (cmd.cmd != "") {
        cmdGroup.commands.emplace_back(cmd);
    }

    return Result<CommandGroup>(cmdGroup);
}

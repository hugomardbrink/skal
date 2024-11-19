#include <sstream>
#include <unordered_map>
#include <functional>

#include "parser.hxx"

using namespace parser;

std::unordered_map<string, std::function<Result<Command>(CommandGroup&, Command, std::stringstream&)>> token_actions = {
    {"|", [](CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command> {                     
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '|'"};
        }
        cmd_group.commands.emplace_back(cmd);
        return Result<Command>(Command{});
    }},
    {"<", [](CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> {
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '<'"};
        }

        std::string file_name;
        ss >> file_name;
        if (file_name.empty()) {
            return Error{"Parse error at '<'"};
        }

        cmd_group.rstdin = std::optional<string>(file_name);
        return Result<Command>(cmd);
    }},
    {">", [](CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> {
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '>'"};
        }

        std::string file_name;
        ss >> file_name;
        if (file_name.empty()){
            return Error{"Parse error at '>'"};
        }

        cmd_group.rstdout = std::optional<string>(file_name); 
        return Result<Command>(cmd);
    }},
    {"2>", [](CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> {
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '2>'"};
        }

        std::string file_name;
        ss >> file_name;
        if (file_name.empty()){
            return Error{"Parse error at '2>'"};
        }

        cmd_group.rstderr = std::make_optional(file_name);
        return Result<Command>(cmd);
    }},
    {"&", [](CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command> {
        cmd_group.is_background = true;
        return Result<Command>(cmd);
    }}
};


Result<CommandGroup> Parser::parse(string input) {
    CommandGroup cmd_group;
    std::stringstream ss(input);
    string token;

    Command cmd;
    while (ss >> token) {
        if (token_actions.find(token) != token_actions.end()) {
            Result<Command> result = token_actions[token](cmd_group, cmd, ss);
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

    if (!cmd.cmd.empty()) { 
        cmd_group.commands.emplace_back(cmd);
    }

    return Result<CommandGroup>(cmd_group);
}

#include <sstream>
#include <unordered_map>
#include <vector>
#include <iostream>

#include "parser.hxx"

using namespace parser;

std::unordered_map<string, TokenAction> token_actions = {
    {"|", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command>{                     
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '|'"};
        }
        cmd_group.commands.emplace_back(cmd);
        return Result<Command>(Command{});
    }},
    {"<", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> { 
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
    {">", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> { 
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
    {"2>", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& ss) -> Result<Command> { 
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
    {"&", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command> { 
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '&'"};
        }
         
        cmd_group.is_background = true;
        return Result<Command>(cmd);
    }},
    {"&&", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command> { 
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '&&'"};
        }
        cmd_group.commands.emplace_back(cmd);
        cmd_group.logical_operator = std::make_optional("&&");
        cmd_groups.emplace_back(CommandGroup{});

        return Result<Command>(Command{});
    }},
    {"||", [](CommandGroups& cmd_groups, CommandGroup& cmd_group, Command cmd, std::stringstream& _) -> Result<Command> { 
        if(cmd.cmd.empty()) {
            return Error{"Parse error at '||'"};
        }
        cmd_group.commands.emplace_back(cmd);
        cmd_group.logical_operator = std::make_optional("||");
        cmd_groups.emplace_back(CommandGroup{});

        return Result<Command>(Command{});
    }}
};


Result<CommandGroups> Parser::parse(string input) {
    Command cmd;
    CommandGroups cmd_groups;
    cmd_groups.emplace_back(CommandGroup{});

    string token;
    std::stringstream ss(input);
    while (ss >> token) {
        CommandGroup& current_group = cmd_groups.back(); 

        if (token_actions.find(token) != token_actions.end()) {
            Result<Command> result = token_actions[token](cmd_groups, current_group, cmd, ss);

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
        cmd_groups.back().commands.emplace_back(cmd);
    }
    
    return Result<CommandGroups>(cmd_groups);
}


void parser::PrintCommandGroup(const CommandGroup& cmd_group) {
    std::cout << "Commands: ";
    for (auto& cmd : cmd_group.commands) {
        std::cout << cmd.cmd << " ";
        std::cout << "Args: ";
        for (auto& arg : cmd.args) {
            std::cout << arg << " ";
        }
        std::cout << std::endl;
    }
    if (cmd_group.rstdin.has_value()) {
        std::cout << "< " << cmd_group.rstdin.value() << std::endl;
    }
    if (cmd_group.rstdout.has_value()) {
        std::cout << "> " << cmd_group.rstdout.value() << std::endl;
    }
    if (cmd_group.rstderr.has_value()) {
        std::cout << "2> " << cmd_group.rstderr.value() << std::endl;
    }
    if (cmd_group.is_background) {
        std::cout << "&" << std::endl;
    }
    if (cmd_group.logical_operator.has_value()) {
        std::cout << "logical operator" << cmd_group.logical_operator.value() << std::endl;
    }

}

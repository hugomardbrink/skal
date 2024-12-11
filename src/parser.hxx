#ifndef PARSER_HXX_H_
#define PARSER_HXX_H_

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <sstream>


#include "result.hxx"

using namespace result;
using std::string, std::vector, std::optional, std::nullopt;

namespace parser {
    struct Command {
        string cmd = "";
        vector<string> args = {};
    };

    struct CommandGroup {
        vector<Command> commands = {};

        optional<string> rstdin = nullopt;
        optional<string> rstdout = nullopt;
        optional<string> rstderr = nullopt;

        bool is_background = false;
        optional<string> logical_operator = nullopt;
    };
   
    void PrintCommandGroup(const CommandGroup& cmd_group);

    using CommandGroups = vector<CommandGroup>;

    using TokenAction = std::function<Result<Command>(CommandGroups&, CommandGroup&, Command, std::stringstream&)>;

    struct Parser {
        public:
            Result<CommandGroups> parse(string input);
    };
};

#endif // PARSER_HXX_H_


#ifndef PARSER_HXX_H_
#define PARSER_HXX_H_

#include <string>
#include <vector>
#include <optional>

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
    };

    struct Parser {
        public:
            Result<parser::CommandGroup> parse(string input);
    };
};

#endif // PARSER_HXX_H_


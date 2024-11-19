#ifndef PARSER_HXX
#define PARSER_HXX

#include <string>
#include <vector>
#include <optional>

#include "result.hxx"

using namespace result;
using std::string, std::vector, std::optional, std::nullopt;

namespace parser {
    struct Command {
        string cmd;
        vector<string> args = {};
    };

    struct CommandGroup {
        vector<Command> commands = {};

        optional<string> rstdin = nullopt;
        optional<string> rstdout = nullopt;
        optional<string> rstderr = nullopt;

        bool isBackground = false;
    };

    struct Parser {
        public:
        Result<parser::CommandGroup> parse(string input);
    };
};

#endif // PARSER_HXX


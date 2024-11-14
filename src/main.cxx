#include <iostream>
#include <string>
#include <variant>

#include "parser.hxx"
#include "result.hxx"

using namespace parser;
using namespace result;

int main() {
    parser::Parser p;

    while(true) {
        std::string input;
        std::cout << std::endl << "> ";
        std::getline(std::cin, input);
        result::Result<parser::CommandGroup> cmdGroup = p.parse(input);
        
        if (std::holds_alternative<Error>(cmdGroup)) {
            std::cout << std::get<Error>(cmdGroup).message << std::endl;
        } else {
            CommandGroup cg = std::get<parser::CommandGroup>(cmdGroup);
            std::cout << "Commands: " << std::endl;
            for (auto cmd : cg.commands) {
                std::cout << "Command: " << cmd.cmd << std::endl;
                std::cout << "Args: ";
                for (auto arg : cmd.args) {
                    std::cout << arg << " ";
                }
                std::cout << std::endl;
            }

            std::cout << "rstdin: " << cg.rstdin.value_or("None") << std::endl;
            std::cout << "rstdout: " << cg.rstdout.value_or("None") << std::endl;
            std::cout << "rstderr: " << cg.rstderr.value_or("None") << std::endl;
            std::cout << "isBackground: " << cg.isBackground << std::endl;
        }
    }


    return 0;
}

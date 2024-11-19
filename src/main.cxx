#include <iostream>
#include <string>

#include "parser.hxx"
#include "result.hxx"
#include "shell.hxx"

using namespace parser;
using namespace result;

using std::string, std::cout, std::endl;

int main() {
    parser::Parser p;
    
    while(true) {
        string input;
        auto current_dir_result = shell::GetCurrentDir();

        if (current_dir_result.is_error()) {
            cout << current_dir_result.unwrap_error().message << endl;
            return 1;
        } else {
            cout << std::endl << current_dir_result.unwrap() << " > ";
        }

        std::getline(std::cin, input);
        result::Result<parser::CommandGroup> cmd_group_result = p.parse(input);
        
        if (cmd_group_result.is_error()) {
            std::cerr << cmd_group_result.unwrap_error().message << endl;
        } else {
            auto cmd_group = cmd_group_result.unwrap();
            shell::ExecuteCommandGroup(cmd_group);
        }        
    }

    return 0;
}

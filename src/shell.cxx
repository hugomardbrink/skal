#include <iostream>
#include <string>
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.hxx"
#include "result.hxx"
#include "core.hxx"

using namespace parser;
using namespace result;

using std::string, std::cout, std::endl;

int main() {
    parser::Parser p;

    while(true) {
        string input;
        auto current_dir_result = core::GetCurrentDir();

        if (current_dir_result.is_error()) {
            cout << current_dir_result.unwrap_error().message << endl;
            return 1;
        }  
        string prompt_str = current_dir_result.unwrap() + string(" > ");
        const char* prompt = prompt_str.c_str();

        input = readline(prompt);
        add_history(input.c_str());

        result::Result<parser::CommandGroup> cmd_group_result = p.parse(input); 
        if (cmd_group_result.is_error()) {
            std::cerr << cmd_group_result.unwrap_error().message << endl;
        } else {
            auto cmd_group = cmd_group_result.unwrap();
            core::ExecuteCommandGroup(cmd_group);
        }
    }

    return 0;
}

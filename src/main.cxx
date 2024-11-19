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
        cout << std::endl << shell::get_current_dir() << " > ";
        std::getline(std::cin, input);
        result::Result<parser::CommandGroup> cmdGroupResult = p.parse(input);
        
        if (cmdGroupResult.is_error()) {
            cout << cmdGroupResult.unwrap_error().message << endl;
        } else {
            auto cmdGroup = cmdGroupResult.unwrap();
            shell::executeCommandGroup(cmdGroup);
        }        
    }


    return 0;
}

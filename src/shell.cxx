#include <iostream>
#include <string>
#include <filesystem>

#include <readline/readline.h>
#include <readline/history.h>
#include <csignal> 

#include "parser.hxx"
#include "result.hxx"
#include "core.hxx"

using namespace parser;
using namespace result;

using std::string, std::cout, std::endl;

constexpr std::string_view HISTORY_FILE{".skal_history"};

Result<bool> HistoryFileExists() {
    const char* home{getenv("HOME")};
    if (home == nullptr) {
        return Error{"HOME environment variable not set"};
    }

    string history_file{string(home) + HISTORY_FILE.data()};
    return std::filesystem::exists(history_file);
}

Result<void> WriteHistoryFile() {
    const char* home = getenv("HOME");
    if (home == nullptr) {
        return Error{"HOME environment variable not set"};
    }

    string history_file = string(home) + "/.skal_history";
    write_history(history_file.c_str());
    return {};
}

Result<void> ReadHistoryFile() {
    const char* home = getenv("HOME");
    if (home == nullptr) {
        return Error{"HOME environment variable not set"};
    }

    string history_file = string(home) + "/.skal_history";
    read_history(history_file.c_str());
    return {};
}

Result<void> QuitShell() { 
    auto history_file_exists = HistoryFileExists();
    if (history_file_exists.is_error()) {
        return history_file_exists.unwrap_error(); 
    }
    else {
        auto write_history_result{WriteHistoryFile()};
        if (write_history_result.is_error()) {
           return write_history_result.unwrap_error();  
        }
    }
    return {};
}

void HandleCtrlC(int sig) {
    cout << endl;
    auto quit_result{QuitShell()};
    if (quit_result.is_error()) {
        cout << quit_result.unwrap_error().message << endl;
        exit(1);
    } else {
        exit(0);
    }
}

Result<void> InitShell() {
    signal(SIGINT, HandleCtrlC);

    auto history_file_exists{HistoryFileExists()};
    if (history_file_exists.is_error()) {
       return history_file_exists.unwrap_error(); 
    }
    else {
        auto read_history_result{ReadHistoryFile()};
        if (read_history_result.is_error()) {
            return read_history_result.unwrap_error();
        }
    }
    return {};
}

int main() {
    auto init_result{InitShell()};
    if (init_result.is_error()) {
        cout << init_result.unwrap_error().message << endl;
        return 1;
    }

    parser::Parser p;
    while(true) {
        auto current_dir_result{core::GetCurrentDir()};
        if (current_dir_result.is_error()) {
            cout << current_dir_result.unwrap_error().message << endl;
            return 1;
        }  
        string prompt_str{current_dir_result.unwrap() + string(" > ")};
        const char* prompt{prompt_str.c_str()};
        const char* input{readline(prompt)};

        if (input == NULL) { // EOF
            cout << endl;
            auto quit_result{QuitShell()};
            if (quit_result.is_error()) {
                cout << quit_result.unwrap_error().message << endl;
                return 1;
            } else {
                return 0;
            }
        }

        add_history(input);

        result::Result<parser::CommandGroup> cmd_group_result{p.parse(input)}; 
        if (cmd_group_result.is_error()) {
            std::cerr << cmd_group_result.unwrap_error().message << endl;
        } else {
            auto cmd_group{cmd_group_result.unwrap()};
            core::ExecuteCommandGroup(cmd_group);
        }
    }

    return 0;
}

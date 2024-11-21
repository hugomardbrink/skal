#include <iostream>
#include <optional>
#include <string>
#include <filesystem>

#include <unistd.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <csignal> 

#include "shell.hxx"
#include "result.hxx"
#include "parser.hxx"

using namespace parser;
using namespace result;
using std::string, std::cout, std::endl;

constexpr std::string_view HISTORY_FILE{".skal_history"};

std::optional<int> foreground_pid = std::nullopt;

constexpr int FD_READ{0};
constexpr int FD_WRITE{1};


char **GetExecArgs(const Command& cmd){
    char **exec_args{new char*[cmd.args.size() + 2]};
    exec_args[0] = const_cast<char*>(cmd.cmd.c_str());
    for (int i{1}; i < cmd.args.size() + 1; i++) {
        exec_args[i] = const_cast<char*>(cmd.args[i-1].c_str());
    }
    // Never freed but process will exit after execvp, thus freeing memory
    return exec_args;
} 

// Void since only successes will return
void ExecuteCommand(std::vector<Command>::const_reverse_iterator cmd_it, const CommandGroup& cmd_group, std::optional<int> fd) {    
    if(fd.has_value()) { 
        dup2(fd.value(), STDOUT_FILENO);
        close(fd.value());
    }
    
    auto cmd = *cmd_it;
    auto next_cmd_it = cmd_it+1;
    
    if(next_cmd_it == cmd_group.commands.rend()) { // last command
        if(cmd_group.rstdout.has_value()) {
            int stdout_fd{open(cmd_group.rstdout.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)};
            if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
                std::cerr << "dup2 failed" << std::endl;
                _exit(1);
            }

            dup2(stdout_fd, STDOUT_FILENO);
            close(stdout_fd);
        }
        if(cmd_group.rstderr.has_value()) { 
            int stderr_fd{open(cmd_group.rstderr.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)};
            if (dup2(stderr_fd, STDERR_FILENO) == -1) {
                std::cerr << "dup2 failed" << std::endl;
                _exit(1);
            }

            dup2(stderr_fd, STDERR_FILENO);
            close(stderr_fd);
        }
        if(cmd_group.rstdin.has_value()) {
            int stdin_fd{open(cmd_group.rstdin.value().c_str(), O_RDONLY)};
            if(stdin_fd == -1) {
                std::cerr << "open failed" << std::endl;
                _exit(1);
            }
            dup2(stdin_fd, STDIN_FILENO);
            close(stdin_fd);
        }
 
        char **exec_args = GetExecArgs(cmd);
        execvp(exec_args[0], exec_args);
 
        // If code reaches here, execvp failed
        std::cerr << "execvp failed" << std::endl;
        _exit(1);
    } else { // continue pipe
        int new_fd[2];
        if (pipe(new_fd) == -1) {
            std::cerr << "pipe failed" << std::endl;
            _exit(1);
        }
        auto pid{fork()};

        if(pid == 0) {
            ExecuteCommand(next_cmd_it, cmd_group, std::optional<int>{new_fd[FD_WRITE]});
        }
        else if(pid > 0) {
            close(new_fd[FD_WRITE]); 
            dup2(new_fd[FD_READ], STDIN_FILENO); 
            close(new_fd[FD_READ]); 
            
            waitpid(pid, NULL, 0);
            
            char **exec_args = GetExecArgs(cmd);
            execvp(exec_args[0], exec_args);

            // If code reaches here, execvp failed
            std::cerr << "execvp failed" << std::endl;
            _exit(1);
        }
        else {
            std::cerr << "fork failed" << std::endl;
            _exit(1);
        }
    }
}

Result<void> shell::ExecuteCommandGroup(const CommandGroup& cmd_group) {
    auto cmd_it = cmd_group.commands.rbegin();
    auto cmd = *cmd_it;

    if(cmd.cmd == "cd") {
        if(cmd.args.size() == 0) {
            chdir(getenv("HOME"));
        }
        else if(cmd.args.size() > 1) {
            return Error{"cd: too many arguments"};
        }
        else {
            int status = chdir(cmd.args[0].c_str());
            if (status != 0) {
               return Error{"cd failed"}; 
            }
        }
    } else if(cmd.cmd == "exit") {
        exit(0);
    } else {
        auto pid = fork();
        
        if(pid == 0) {
            if(cmd_group.is_background) {
                setpgid(0, 0);
                std::cout << "[" <<  getpid() << "]" << std::endl;
            }

            ExecuteCommand(cmd_it, cmd_group, std::nullopt);
        } else if(pid > 0) {
            if(cmd_group.is_background) {
                waitpid(pid, NULL, WNOHANG);
            }
            else {
                foreground_pid = pid;
                waitpid(pid, NULL, 0);
            }
            waitpid(pid, NULL, 0);
        } else {
            return Error{"fork failed"};
        }
    } 
    return {};
}

Result<std::string> GetCurrentDir() {
    constexpr auto MAX_UNIX_PATH{1024};
    char cwd[MAX_UNIX_PATH];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return std::string(cwd);
    } else {
        return Error{"getcwd failed"};
    }
}

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
    if(foreground_pid.has_value()) {
        kill(foreground_pid.value(), SIGINT);
    }
    else {
        std::cout << std::endl;
    }
}

void HandleBackgroundProcess(int sig)
{
    auto pid = waitpid(-1, NULL, WNOHANG);
    if(pid > 0 && foreground_pid != pid) 
    {
        std::cout << "[" << pid << "] Done" << std::endl;
    }

    if(foreground_pid == pid) 
    {
        foreground_pid = std::nullopt;
    }
}

Result<void> InitShell() {
    signal(SIGINT, HandleCtrlC);
    signal(SIGCHLD, HandleBackgroundProcess);

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

Result<void> shell::RunShell() {
    auto init_result{InitShell()};
    if (init_result.is_error()) {
        return init_result.unwrap_error();
    }

    parser::Parser p;
    while(true) {
        auto current_dir_result{GetCurrentDir()};
        if (current_dir_result.is_error()) {
            return current_dir_result.unwrap_error();
        }  
        string prompt_str{current_dir_result.unwrap() + string(" > ")};
        const char* prompt{prompt_str.c_str()};
        const char* input{readline(prompt)};

        if (input == NULL) { // EOF
            cout << endl;
            auto quit_result{QuitShell()};
            if (quit_result.is_error()) {
                return quit_result.unwrap_error();
            } else {
                return {};
            }
        }

        add_history(input);

        result::Result<parser::CommandGroup> cmd_group_result{p.parse(input)}; 
        if (cmd_group_result.is_error()) {
            std::cerr << cmd_group_result.unwrap_error().message << endl;
        } else {
            auto cmd_group{cmd_group_result.unwrap()};
            ExecuteCommandGroup(cmd_group);
        }
    }
    return {};
}


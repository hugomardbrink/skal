#include <iostream>
#include <optional>

#include <unistd.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "core.hxx"
#include "result.hxx"

using namespace parser;
using namespace result;

constexpr int FD_READ = 0;
constexpr int FD_WRITE = 1;

char **GetExecArgs(const Command& cmd){
    char **exec_args = new char*[cmd.args.size() + 2];
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

Result<void> core::ExecuteCommandGroup(const CommandGroup& cmdGroup) {
    auto cmd_it = cmdGroup.commands.rbegin();
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
            ExecuteCommand(cmd_it, cmdGroup, std::nullopt);
        } else if(pid > 0) {
            waitpid(pid, NULL, 0);
        } else {
            return Error{"fork failed"};
        }
    } 
    return {};
}

Result<std::string> core::GetCurrentDir() {
    constexpr auto MAX_UNIX_PATH{1024};
    char cwd[MAX_UNIX_PATH];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return std::string(cwd);
    } else {
        return Error{"getcwd failed"};
    }
}

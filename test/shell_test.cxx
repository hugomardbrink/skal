#include "../src/shell.hxx"
#include "../src/parser.hxx"

#include "test.hxx"
#include "shell_test.hxx"

#include <unistd.h>
#include <sys/wait.h>

void TestShellExecute() {
    constexpr std::string_view prompt = "echo Hello, World!";
    parser::Parser p;

    Result<parser::CommandGroup> cmd_group_result = p.parse(prompt.data());
    if(cmd_group_result.is_error()) {
        test::LogFatalError(cmd_group_result.unwrap_error());
    }
    auto cmd_group = cmd_group_result.unwrap();

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        test::LogFatalError(Error{"pipe creation failed in test"});
    }

    auto pid = fork();
    if(pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[1]);

        Result<void> result = shell::ExecuteCommandGroup(cmd_group);
    } else {
        close(pipefd[1]);

        // Read from the pipe
        char buffer[1024];
        ssize_t bytes_read;

        std::string output;
        read(pipefd[0], buffer, sizeof(buffer) - 1);    
        output = buffer; 
        

        output.pop_back(); // Remove newline
        if(output != "Hello, World!\0") {
            test::LogFatalError(Error{"Execute stdout mismatched, Expected: Hello, World!, Got: " + output});
        }

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        test::LogInfo("TestShellExecute passed");
    }
}

void TestShellCdEmpty() {
    constexpr std::string_view prompt = "cd";
    parser::Parser p;

    Result<parser::CommandGroup> cmd_group_result = p.parse(prompt.data());
    if(cmd_group_result.is_error()) {
        test::LogFatalError(cmd_group_result.unwrap_error());
    }
    auto cmd_group = cmd_group_result.unwrap();

    Result<void> result = shell::ExecuteCommandGroup(cmd_group);
    if(result.is_error()) {
        test::LogFatalError(result.unwrap_error());
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        test::LogFatalError(Error{"getcwd failed"});
    }

    if(std::string(cwd) != std::string(getenv("HOME"))) {
        test::LogFatalError(Error{"cd failed"});
    }

    test::LogInfo("TestShellCdEmpty passed");
}

void TestShellCd() {
    parser::Parser p;
    //cd to binary directory
    system("cd ~/");
    system("mkdir testDir");
    system("chmod 777 testDir");
    

    constexpr std::string_view prompt = "cd testDir";
    auto cmd_group_result = p.parse(prompt.data());
    if(cmd_group_result.is_error()) {
        test::LogFatalError(cmd_group_result.unwrap_error());
    }
    auto cmd_group = cmd_group_result.unwrap();
    
    auto result = shell::ExecuteCommandGroup(cmd_group);
    if(result.is_error()) {
        test::LogFatalError(result.unwrap_error());
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        test::LogFatalError(Error{"getcwd failed"});
    }

    if(std::string(cwd) != std::string(getenv("HOME")) + "/testDir") {
        test::LogFatalError(Error{"cd failed"});
    }

    auto dir_location = std::string(getenv("HOME")) + "/testDir";
    rmdir(dir_location.c_str());

    test::LogInfo("TestShellCd passed");
}

void TestShellExit() {
    auto pid = fork();
    
    if(pid == 0) {
        constexpr std::string_view prompt = "exit";
        parser::Parser p;

        Result<parser::CommandGroup> cmd_group_result = p.parse(prompt.data());
        if(cmd_group_result.is_error()) {
        test::LogFatalError(cmd_group_result.unwrap_error());
        }
        auto cmd_group = cmd_group_result.unwrap();

        shell::ExecuteCommandGroup(cmd_group);

        // If code reaches here, exit failed
        test::LogFatalError(Error{"exit failed"});
    }
    else if(pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            test::LogInfo("TestShellExit passed");
        } else {
            test::LogFatalError(Error{"exit failed"});
        }
    }
    else {
        test::LogFatalError(Error{"fork failed"});
    }
}


void shell_test::TestShell() {
    test::LogInfo("Running shell tests...");
    TestShellExecute();
    TestShellCdEmpty();
    TestShellCd();
    TestShellExit();
}


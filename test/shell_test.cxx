#include "../src/shell.hxx"
#include "../src/parser.hxx"

#include "test.hxx"
#include "shell_test.hxx"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void TestShellExecute() {
    constexpr std::string_view prompt = "/bin/echo Hello, World!";
    parser::Parser p;

    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    auto cmd_groups = cmd_groups_result.unwrap();

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        test::LogFatalError(Error{"pipe creation failed in test"});
    }

    auto pid = fork();
    if(pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[1]);

        Result<void> result = shell::ExecuteCommandGroups(cmd_groups);
    } else {
        close(pipefd[1]);

        // Read from the pipe
        char buffer[1024];

        std::string output;
        read(pipefd[0], buffer, sizeof(buffer) - 1);
        output = buffer; 
        
        output.pop_back(); // Remove newline
        if(output.find("Hello, World!") == std::string::npos) {
            test::LogFatalError(Error{"Execute stdout mismatched, Expected: Hello, World!, Got: " + output});
        }

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status)) {
            if(WEXITSTATUS(status)) {
                test::LogFatalError(Error{"exit failed"});
            }
        }
        test::LogInfo("TestShellExecute passed");
    }
}

void TestShellCdEmpty() {
    constexpr std::string_view prompt = "cd";
    parser::Parser p;

    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    auto cmd_groups = cmd_groups_result.unwrap();

    Result<void> result = shell::ExecuteCommandGroups(cmd_groups);
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
    chdir(std::string(getenv("HOME")).c_str());
    system("mkdir testDir");
    system("chmod 777 testDir");
    

    constexpr std::string_view prompt = "cd testDir";
    auto cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    auto cmd_groups = cmd_groups_result.unwrap();
    
    auto result = shell::ExecuteCommandGroups(cmd_groups);
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

    chdir(std::string(getenv("HOME")).c_str());

    auto dir_location = std::string(getenv("HOME")) + "/testDir";
    if (rmdir(dir_location.c_str()) == -1) {
        test::LogFatalError(Error{"rmdir failed"});
    }

    test::LogInfo("TestShellCd passed");
}

void TestShellExit() {
    auto pid = fork();
    
    if(pid == 0) {
        constexpr std::string_view prompt = "exit";
        parser::Parser p;

        Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
        if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
        }
        auto cmd_groups = cmd_groups_result.unwrap();

        auto execution_result = shell::ExecuteCommandGroups(cmd_groups);
        
        if(execution_result.is_error()) {
            test::LogFatalError(execution_result.unwrap_error());
        }
    }
    else if(pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status)) {
            if(WEXITSTATUS(status)) {
                test::LogFatalError(Error{"exit failed"});
            }
            else {
                test::LogInfo("TestShellExit passed");
            }
        }
    }
    else {
        test::LogFatalError(Error{"fork failed"});
    }
}

void TestShellAndCommandChain() {
    constexpr std::string_view prompt = "echo Hello, 1! && echo Hello, 2!";
    parser::Parser p;

    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    auto cmd_groups = cmd_groups_result.unwrap();

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        test::LogFatalError(Error{"pipe creation failed in test"});
    }

    auto pid = fork();
    if(pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[1]);

        Result<void> result = shell::ExecuteCommandGroups(cmd_groups);
    } else {
        close(pipefd[1]);

        char buffer_fst[1024], buffer_snd[1024];

        read(pipefd[0], buffer_fst, sizeof(buffer_fst) - 1);   
        read(pipefd[0], buffer_snd, sizeof(buffer_snd) - 1);

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        std::string output_fst, output_snd;
        output_fst = buffer_fst; 
        output_fst.pop_back(); // Remove newline
        if(output_fst.find("Hello, 1!") == std::string::npos) {
            test::LogFatalError(Error{"Execute stdout mismatched, Expected: Hello, 1!, Got: " + output_fst});
        }

        output_snd = buffer_snd;
        output_snd.pop_back(); // Remove newline
        if(output_snd.find("Hello, 2!") == std::string::npos) {
            test::LogFatalError(Error{"Execute stdout mismatched, Expected: Hello, 2!, Got: " + output_snd});
        }

        if(WIFEXITED(status)) {
            if(WEXITSTATUS(status)) {
                test::LogFatalError(Error{"exit failed"});
            }
        }

        test::LogInfo("TestShellAndCommandChain passed");
    } 
}

void TestShellOrCommandChain() {
    constexpr std::string_view prompt = "echo Hello, Or? || echo not";
    parser::Parser p;

    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    auto cmd_groups = cmd_groups_result.unwrap();
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        test::LogFatalError(Error{"pipe creation failed in test"});
    }

    auto pid = fork();
    if(pid == 0) {
        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[1]);

        Result<void> result = shell::ExecuteCommandGroups(cmd_groups);
    } else {
        close(pipefd[1]);

        char buffer_echo[1024], buffer_not[1024];
        read(pipefd[0], buffer_echo, sizeof(buffer_echo) - 1);
        read(pipefd[0], buffer_not, sizeof(buffer_not) - 1);

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        std::string output = buffer_echo;

        output.pop_back(); // Remove newline
        if(output.find("Hello, Or?") == std::string::npos) {
            test::LogFatalError(Error{"Execute stdout mismatched, Expected: Hello, 2!, Got: " + output});
        }

        output = buffer_not;
        output.pop_back(); // Remove newline
        if(output.find("not") != std::string::npos) {
            test::LogFatalError(Error{"Should not have executed the second command"});
        }

        if(WIFEXITED(status)) {
            if(WEXITSTATUS(status)) {
                test::LogFatalError(Error{"exit failed"});
            }
        }

        test::LogInfo("TestShellOrCommandChain passed");
    }
}

void shell_test::TestShell() {
    test::LogInfo("Running shell tests...");

    TestShellExecute();
    TestShellCdEmpty();
    TestShellCd();
    TestShellExit();
    setenv("PATH", "/bin:/bin", 1);
    TestShellAndCommandChain();
    setenv("PATH", "/bin:/bin", 1);
    TestShellOrCommandChain();
}


#include "test.hxx"
#include "parser_test.hxx"

#include "../src/parser.hxx"
#include "../src/result.hxx"

parser::Parser p;

void TestParserBasicCommand() {
    constexpr std::string_view prompt = "ls";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }
    
    parser::CommandGroups cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].cmd != prompt.data()) {
        test::LogFatalError(Error{"Failed to parse command args"});
    }

    test::LogInfo("TestParserBasicCommand passed");
};

void TestParserArgs() {
    constexpr std::string_view prompt = "ls -a -l";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse args"});
    } else if(cmd_groups[0].commands[0].args[1] != "-l") {
        test::LogFatalError(Error{"Failed to parse args"});
    }

    test::LogInfo("TestParserArgs passed");
};

void TestParserPipes() {
    constexpr std::string_view prompt = "ls -a | grep 'test' | wc -l";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].cmd != "ls") {
        test::LogFatalError(Error{"Failed to parse piped command"});
    } else if(cmd_groups[0].commands[1].cmd != "grep") {
        test::LogFatalError(Error{"Failed to parse piped command"});
    } else if(cmd_groups[0].commands[2].cmd != "wc") {
        test::LogFatalError(Error{"Failed to parse piped command"});
    }

    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse piped args"});
    } else if(cmd_groups[0].commands[1].args[0] != "'test'") {
        test::LogFatalError(Error{"Failed to parse piped args"});
    } else if(cmd_groups[0].commands[2].args[0] != "-l") {
        test::LogFatalError(Error{"Failed to parse piped args"});
    }

    test::LogInfo("TestParserPipes passed");
}

void TestParserStdin() {
    constexpr std::string_view prompt = "ls -a < test.txt";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].cmd != "ls") {
        test::LogFatalError(Error{"Failed to parse stdin command"});
    }

    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse stdin args"});
    }

    if(cmd_groups[0].rstdin != "test.txt") {
        test::LogFatalError(Error{"Failed to parse stdin file"});
    }

    test::LogInfo("TestParserStdin passed");
}

void TestParserStdout() {
    constexpr std::string_view prompt = "ls -a > test.txt";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].cmd != "ls") {
        test::LogFatalError(Error{"Failed to parse stdout command"});
    }

    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse stdout args"});
    }

    if(cmd_groups[0].rstdout != "test.txt") {
        test::LogFatalError(Error{"Failed to parse stdout file"});
    }

    test::LogInfo("TestParserStdout passed");
}

void TestParserStderr() {
    constexpr std::string_view prompt = "ls -a 2> test.txt";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].commands[0].cmd != "ls") {
        test::LogFatalError(Error{"Failed to parse stderr command"});
    }

    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse stderr args"});
    }

    if(cmd_groups[0].rstderr != "test.txt") {
        test::LogFatalError(Error{"Failed to parse stderr file"});
    }

    test::LogInfo("TestParserStderr passed");
}

void TestParserCommandChain() {
    constexpr std::string_view prompt = "ls -a && echo 'test'";
    Result<parser::CommandGroups> cmd_groups_result = p.parse(prompt.data());
    if(cmd_groups_result.is_error()) {
        test::LogFatalError(cmd_groups_result.unwrap_error());
    }

    auto cmd_groups = cmd_groups_result.unwrap();
    if(cmd_groups[0].logical_operator != "&&") {
        test::LogFatalError(Error{"Failed to parse and chain command"});
    }

    if(cmd_groups[0].commands[0].cmd != "ls") {
        test::LogFatalError(Error{"Failed to parse and chain command"});
    }
    if(cmd_groups[1].commands[0].cmd != "echo") {
        test::LogFatalError(Error{"Failed to parse and chain command"});
    }

    if(cmd_groups[0].commands[0].args[0] != "-a") {
        test::LogFatalError(Error{"Failed to parse and chain args"});
    }

    if(cmd_groups[1].commands[0].args[0] != "'test'") {
        test::LogFatalError(Error{"Failed to parse and chain args"});
    }

    test::LogInfo("TestParserCommandChain passed");
}

void parser_test::TestParser() {
    test::LogInfo("Running parser tests...");
    TestParserBasicCommand();
    TestParserArgs();
    TestParserPipes();
    TestParserStdin();
    TestParserStdout();
    TestParserStderr();
    TestParserCommandChain();
}; 


#include "test.hxx"
#include "parser_test.hxx"
#include "shell_test.hxx"

#include <iostream>

int main() {
    std::cout << "Running tests..." << std::endl;
    parser_test::TestParser();
    shell_test::TestShell();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}

void test::LogFatalError(const result::Error& err) {
    std::cerr << err.message << std::endl;
    exit(1);
}

void test::LogWarn(const std::string& log) {
    std::cerr << log << std::endl;
}

void test::LogInfo(const std::string& log) {
    std::cout << log << std::endl;
}


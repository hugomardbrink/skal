#include "test.hxx"
#include "parser_test.hxx"
#include "core_test.hxx"

#include <iostream>

int main() {
    std::cout << "Running tests..." << std::endl;
    parser_test::TestParser();
    core_test::TestCore();
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


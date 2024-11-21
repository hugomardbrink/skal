#ifndef TEST_HXX_H_
#define TEST_HXX_H_

#include <string>
#include <vector>
#include <functional>

#include "../src/result.hxx"

namespace test {
    static std::vector<std::function<void()>> function_registry = {};
    void RegisterFunction(std::function<void()> func);
    void LogFatalError(const result::Error& err);
    void LogWarn(const std::string& log);
    void LogInfo(const std::string& log);
}

#endif // TEST_HXX_H_

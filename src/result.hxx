#ifndef RESULT_HXX
#define RESULT_HXX

#include <variant>
#include <string>

namespace result {
    struct Error {
        std::string message;
    };

    template <typename T>
    using Result = std::variant<T, Error>;
} // namespace
#endif // RESULT_HXX


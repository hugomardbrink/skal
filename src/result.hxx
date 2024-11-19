#ifndef RESULT_HXX
#define RESULT_HXX

#include <variant>
#include <string>

namespace result {
    struct Error {
        std::string message;
    };

    template <typename T>
    class Result { 
        public:
            Result(const T& value) : data(value) {}
            Result(const Error& error) : data(error) {}

            bool is_ok() const {
                return std::holds_alternative<T>(data);
            }

            bool is_error() const {
                return std::holds_alternative<Error>(data);
            }

            T unwrap() const {
                return std::get<T>(data);
            }

            Error unwrap_error() const {
                return std::get<Error>(data); 
            }

        private:
            std::variant<T, Error> data;
    };

    template <>
    class Result<void> {
        public:
            Result() : data(std::monostate{}) {}

            Result(const Error& error) : data(error) {}

            bool is_ok() const {
                return std::holds_alternative<std::monostate>(data);
            }

            bool is_error() const {
                return std::holds_alternative<Error>(data);
            }

            Error unwrap_error() const {
                return std::get<Error>(data);
            }

        private:
            std::variant<std::monostate, Error> data;
    }; 

} // namespace
#endif // RESULT_HXX


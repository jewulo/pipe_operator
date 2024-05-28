#pragma once

// https://www.cppstories.com/2024/pipe-operator/

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <random>

// A Basic Example 
std::string StringProc_1(std::string&& s) {
    s += " proc by 1,";
    std::cout << "I'm in StringProc_1, s = " << s << "\n";
    return s;
}

std::string StringProc_2(std::string&& s) {
    s += " proc by 2,";
    std::cout << "I'm in StringProc_2, s = " << s << "\n";
    return s;
}

std::string StringProc_3(std::string&& s) {
    s += " proc by 3,";
    std::cout << "I'm in StringProc_3, s = " << s << "\n";
    return s;
}

namespace v1 {
    using Function = std::function<std::string(std::string&&)>;

    auto operator | (std::string&& s, Function f) -> std::string {
        return f(std::move(s));
    }

    void SimplePipeTest() {
        std::string start_str("Start string ");
        std::cout << (std::move(start_str) |
            StringProc_1 | StringProc_2 | StringProc_3);
    }
}

// Making it more general 
namespace v2 {
    using Function = std::function<std::string(std::string&&)>;

    template <typename T, typename Function>
        requires (std::invocable<Function, T>)
    constexpr auto operator | (T&& t, Function&& f) -> typename std::invoke_result_t<Function, T> {
        return std::invoke(std::forward<Function>(f), std::forward<T>(t));
    }

    // we can now use any callable type, see lambda below
    void SimplePipeTest() {
        std::string start_str("Start string ");
        std::cout << (std::move(start_str) |
            StringProc_1 | StringProc_2 | [](std::string&& s) {
                s += " proc by 3,";
                std::cout << "I'm in StringProc_3, s = " << s << "\n";
                return s;
            });
    }

    // we can now use any callable type, see function object below
    struct StringProc_funobj {
        std::string operator()(std::string&& s) {
            s += " proc by 3,";
            std::cout << "I'm in StringProc_3, s = " << s << "\n";
            return s;
        }
    };

    void SimplePipeTest_2() {
        std::string start_str("Start string ");
        std::cout << (std::move(start_str) |
            StringProc_1 | StringProc_2 | StringProc_funobj{});
    }
}

// Handling errors
namespace v3 {


    template < typename T >
    concept is_expected = requires(T t)
    {
        typename T::value_type;	// type requirement – nested member name exists
        typename T::error_type;	// type requirement – nested member name exists

        requires std::is_constructible_v<bool, T>;
        requires std::same_as<std::remove_cvref_t<decltype(*t)>, typename T::value_type>;
        requires std::constructible_from<T, std::unexpected< typename T::error_type>>;
    };

    template <typename T, typename E, typename Function>
        requires std::invocable<Function, T>&&
            is_expected<typename std::invoke_result_t<Function, T>>
        constexpr auto operator | (std::expected<T, E>&& ex, Function&& f)
        -> typename std::invoke_result_t<Function, T>
    {
        return ex ? std::invoke(std::forward<Function>(f),
            *std::forward< std::expected<T, E>>(ex)) : ex;
    }

    // We have a data structure to process
    struct Payload
    {
        std::string	fStr{};
        int		fVal{};
    };

    // Some error types just for the example
    enum class OpErrorType : unsigned char { kInvalidInput, kOverflow, kUnderflow };

    // For the pipe-line operation - the expected type is Payload,
    // while the 'unexpected' is OpErrorType
    using PayloadOrError = std::expected<Payload, OpErrorType>;

    PayloadOrError Payload_Proc_1(PayloadOrError&& s)
    {
        if (!s)
            return s;

        ++s->fVal;
        s->fStr += " proc by 1,";

        std::cout << "I'm in Payload_Proc_1, s = " << s->fStr << "\n";

        return s;
    }

    PayloadOrError Payload_Proc_2(PayloadOrError&& s)
    {
        if (!s)
            return s;

        ++s->fVal;
        s->fStr += " proc by 2,";

        std::cout << "I'm in Payload_Proc_2, s = " << s->fStr << "\n";

        // Emulate the error, at least once in a while ...
        std::mt19937 rand_gen(std::random_device{} ());
        return (rand_gen() % 2) ? s :
            std::unexpected{ rand_gen() % 2 ?
                OpErrorType::kOverflow : OpErrorType::kUnderflow };
    }

    PayloadOrError Payload_Proc_3(PayloadOrError&& s)
    {
        if (!s)
            return s;

        s->fVal += 3;
        s->fStr += " proc by 3,";

        std::cout << "I'm in Payload_Proc_3, s = " << s->fStr << "\n";

        return s;
    }


    void Payload_PipeTest()
    {
        static_assert(is_expected<PayloadOrError>);	// a short-cut to verify the concept

        auto res = PayloadOrError{ Payload { "Start string ", 42 } } |
            Payload_Proc_1 | Payload_Proc_2 | Payload_Proc_3;

        // Do NOT forget to check if there is a value before accessing that value (otherwise UB)                    
        if (res)
            std::cout << "Success! Result of the pipe: fStr == " << res->fStr << " fVal == " << res->fVal;
        else
            switch (res.error())
            {
            case OpErrorType::kInvalidInput:    std::cout << "Error: OpErrorType::kInvalidInput\n";     break;
            case OpErrorType::kOverflow:		std::cout << "Error: OpErrorType::kOverflow\n";		    break;
            case OpErrorType::kUnderflow:		std::cout << "Error: OpErrorType::kUnderflow\n";	    break;
            default:                            std::cout << "That's really an unexpected error ...\n"; break;
            }
    }
}

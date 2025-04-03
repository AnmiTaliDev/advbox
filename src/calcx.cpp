#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <map>
#include <stack>
#include <iomanip>
#include <stdexcept>

const char* HELP = R"(
Calculator - Command Line Calculator

Usage:
    calc [expression]
    calc [-h|--help]
    calc [-m|--memory]
    calc [-c|--clear]

Options:
    -h, --help      Show this help message
    -m, --memory    Show stored variables
    -c, --clear     Clear stored variables
    -p, --precision Set decimal precision (default: 6)
    -b, --bin       Show result in binary
    -x, --hex       Show result in hexadecimal

Operators:
    +  Addition
    -  Subtraction
    *  Multiplication
    /  Division
    %  Modulo
    ^  Power
    () Parentheses

Functions:
    sqrt(x)    Square root
    sin(x)     Sine (radians)
    cos(x)     Cosine (radians)
    tan(x)     Tangent (radians)
    log(x)     Natural logarithm
    exp(x)     Exponential
    abs(x)     Absolute value
    round(x)   Round to nearest integer
    floor(x)   Round down
    ceil(x)    Round up

Variables:
    ans        Last result
    pi         3.141592653589793
    e          2.718281828459045

Example:
    calc 2 + 2
    calc sin(pi/2)
    calc "2^3 * 4"
)";

class Calculator {
private:
    std::map<std::string, double> variables;
    int precision;
    bool show_binary;
    bool show_hex;

public:
    Calculator() : precision(6), show_binary(false), show_hex(false) {
        variables["pi"] = M_PI;
        variables["e"] = M_E;
        variables["ans"] = 0.0;
    }

    void set_precision(int p) {
        if (p < 0 || p > 15) {
            throw std::runtime_error("Precision must be between 0 and 15");
        }
        precision = p;
    }

    void set_binary(bool b) { show_binary = b; }
    void set_hex(bool h) { show_hex = h; }

    void clear_memory() {
        variables.clear();
        variables["pi"] = M_PI;
        variables["e"] = M_E;
        variables["ans"] = 0.0;
    }

    void show_memory() {
        for (const auto& var : variables) {
            std::cout << var.first << " = " << std::fixed 
                     << std::setprecision(precision) << var.second << "\n";
        }
    }

    bool is_operator(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/' || 
               c == '^' || c == '%' || c == '(' || c == ')';
    }

    int get_precedence(char op) {
        switch (op) {
            case '+': case '-': return 1;
            case '*': case '/': case '%': return 2;
            case '^': return 3;
            default: return 0;
        }
    }

    double apply_operator(double a, double b, char op) {
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': 
                if (b == 0) throw std::runtime_error("Division by zero");
                return a / b;
            case '%': 
                if (b == 0) throw std::runtime_error("Modulo by zero");
                return std::fmod(a, b);
            case '^': return std::pow(a, b);
            default: throw std::runtime_error("Unknown operator");
        }
    }

    double apply_function(const std::string& name, double arg) {
        if (name == "sqrt") return std::sqrt(arg);
        if (name == "sin") return std::sin(arg);
        if (name == "cos") return std::cos(arg);
        if (name == "tan") return std::tan(arg);
        if (name == "log") return std::log(arg);
        if (name == "exp") return std::exp(arg);
        if (name == "abs") return std::abs(arg);
        if (name == "round") return std::round(arg);
        if (name == "floor") return std::floor(arg);
        if (name == "ceil") return std::ceil(arg);
        throw std::runtime_error("Unknown function: " + name);
    }

    double evaluate(const std::string& expr) {
        std::stack<double> values;
        std::stack<char> ops;
        std::string token;
        bool expect_operator = false;

        for (size_t i = 0; i < expr.length(); ++i) {
            char c = expr[i];

            if (std::isspace(c)) continue;

            if (std::isdigit(c) || c == '.') {
                token = "";
                while (i < expr.length() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                    token += expr[i++];
                }
                --i;
                values.push(std::stod(token));
                expect_operator = true;
            }
            else if (std::isalpha(c)) {
                token = "";
                while (i < expr.length() && std::isalnum(expr[i])) {
                    token += expr[i++];
                }
                --i;

                if (variables.find(token) != variables.end()) {
                    values.push(variables[token]);
                    expect_operator = true;
                }
                else {
                    // Это функция
                    while (i < expr.length() && std::isspace(expr[i])) ++i;
                    if (i >= expr.length() || expr[i] != '(') {
                        throw std::runtime_error("Expected '(' after function name");
                    }

                    int brackets = 1;
                    std::string arg_expr;
                    ++i;
                    while (i < expr.length() && brackets > 0) {
                        if (expr[i] == '(') ++brackets;
                        else if (expr[i] == ')') --brackets;
                        if (brackets > 0) arg_expr += expr[i];
                        ++i;
                    }
                    if (brackets > 0) {
                        throw std::runtime_error("Missing closing parenthesis");
                    }
                    --i;

                    double arg = evaluate(arg_expr);
                    values.push(apply_function(token, arg));
                    expect_operator = true;
                }
            }
            else if (is_operator(c)) {
                if (c == '-' && !expect_operator) {
                    values.push(0.0); // Unary minus
                }
                else if (!expect_operator && c != '(') {
                    throw std::runtime_error("Unexpected operator");
                }

                if (c == '(') {
                    ops.push(c);
                    expect_operator = false;
                }
                else if (c == ')') {
                    while (!ops.empty() && ops.top() != '(') {
                        double b = values.top(); values.pop();
                        double a = values.top(); values.pop();
                        values.push(apply_operator(a, b, ops.top()));
                        ops.pop();
                    }
                    if (ops.empty()) {
                        throw std::runtime_error("Mismatched parentheses");
                    }
                    ops.pop(); // Remove '('
                    expect_operator = true;
                }
                else {
                    while (!ops.empty() && ops.top() != '(' && 
                           get_precedence(ops.top()) >= get_precedence(c)) {
                        double b = values.top(); values.pop();
                        double a = values.top(); values.pop();
                        values.push(apply_operator(a, b, ops.top()));
                        ops.pop();
                    }
                    ops.push(c);
                    expect_operator = false;
                }
            }
            else {
                throw std::runtime_error("Invalid character in expression");
            }
        }

        while (!ops.empty()) {
            if (ops.top() == '(') {
                throw std::runtime_error("Mismatched parentheses");
            }
            double b = values.top(); values.pop();
            double a = values.top(); values.pop();
            values.push(apply_operator(a, b, ops.top()));
            ops.pop();
        }

        if (values.empty()) {
            throw std::runtime_error("Empty expression");
        }

        double result = values.top();
        variables["ans"] = result;
        return result;
    }

    void print_result(double result) {
        if (show_binary) {
            std::cout << "Binary: 0b";
            int64_t int_part = static_cast<int64_t>(result);
            if (int_part == 0) {
                std::cout << "0";
            } else {
                std::string bin;
                while (int_part > 0) {
                    bin = (int_part & 1 ? "1" : "0") + bin;
                    int_part >>= 1;
                }
                std::cout << bin;
            }
            std::cout << "\n";
        }
        
        if (show_hex) {
            std::cout << "Hex: 0x" << std::hex << std::uppercase 
                     << static_cast<int64_t>(result) << std::dec << "\n";
        }

        std::cout << std::fixed << std::setprecision(precision) << result << "\n";
    }
};

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        
        if (args.empty() || args[0] == "-h" || args[0] == "--help") {
            std::cout << HELP;
            return 0;
        }

        Calculator calc;

        size_t i = 0;
        while (i < args.size() && args[i][0] == '-') {
            if (args[i] == "-m" || args[i] == "--memory") {
                calc.show_memory();
                return 0;
            }
            else if (args[i] == "-c" || args[i] == "--clear") {
                calc.clear_memory();
                std::cout << "Memory cleared\n";
                return 0;
            }
            else if (args[i] == "-p" || args[i] == "--precision") {
                if (++i >= args.size()) {
                    throw std::runtime_error("Precision value required");
                }
                calc.set_precision(std::stoi(args[i]));
            }
            else if (args[i] == "-b" || args[i] == "--bin") {
                calc.set_binary(true);
            }
            else if (args[i] == "-x" || args[i] == "--hex") {
                calc.set_hex(true);
            }
            else {
                throw std::runtime_error("Unknown option: " + args[i]);
            }
            i++;
        }

        std::string expr;
        for (; i < args.size(); i++) {
            if (!expr.empty()) expr += " ";
            expr += args[i];
        }

        if (expr.empty()) {
            throw std::runtime_error("Expression required");
        }

        double result = calc.evaluate(expr);
        calc.print_result(result);
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Try 'calc --help' for more information.\n";
        return 1;
    }
}
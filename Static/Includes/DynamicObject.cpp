#include <iostream>
#include <map>
#include <functional>
#include <string>
#include <any>

class DynamicObject {
public:
    template<typename Ret, typename... Args>
    void addMethod(const std::string& name, std::function<Ret(Args...)> func) {
        methods[name] = [func](std::any* args) -> std::any {
            return callFunc(func, args, std::index_sequence_for<Args...>{});
        };
    }

    template<typename Ret, typename... Args>
    Ret call(const std::string& name, Args... args) {
        auto iter = methods.find(name);
        if (iter != methods.end()) {
            std::any arguments[] = {args...};
            return std::any_cast<Ret>(iter->second(arguments));
        }
        throw std::runtime_error("Method not found");
    }

private:
    std::map<std::string, std::function<std::any(std::any*)>> methods;

    template<typename Func, typename Tuple, std::size_t... I>
    static auto callFunc(Func& func, Tuple& args, std::index_sequence<I...>) {
        return func(std::any_cast<std::remove_reference_t<decltype(std::get<I>(args))>>(std::get<I>(args))...);
    }
};

int main() {
    DynamicObject obj;

    obj.addMethod<int, int, int>("add", [](int a, int b) {
        return a + b;
    });

    int result = obj.call<int>("add", 5, 3);
    std::cout << "Result: " << result << std::endl; // Output: Result: 8

    return 0;
}
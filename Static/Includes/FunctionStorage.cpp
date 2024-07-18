#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <stdexcept>
using namespace std;

int main() {
    unordered_map<string, function<any(unordered_map<string, any>)>> functions = {
        {"add", [](unordered_map<string, any> args) {
            return any_cast<int>(args["a"]) + any_cast<int>(args["b"]);
        }},
        {"concat", [](unordered_map<string, any> args) {
            return any_cast<string>(args["a"]) + any_cast<string>(args["b"]);
        }}
    };

    cout << any_cast<int>(functions["add"]({{"a", 1}, {"b", 2}})) << endl;
    cout << any_cast<string>(functions["concat"]({{"a", string("1")}, {"b", string("2")}})) << endl;

    system("pause");

    return 0;
}

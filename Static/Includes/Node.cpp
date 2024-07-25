#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <any>

#include "glaze/glaze.hpp"

using namespace std;

class Node;
class NodeValue;

using NodeRef = shared_ptr<Node>;
using NodeArray = vector<NodeValue>;
using NodeArrayRef = shared_ptr<NodeArray>;

class NodeValue {
private:
	variant<nullptr_t, bool, int, double, string, NodeRef, NodeArrayRef, any> value;

public:
	NodeValue() : value(nullptr) {}
	NodeValue(nullptr_t v) : value(v) {}
	NodeValue(bool v) : value(v) {}
	NodeValue(int v) : value(v) {}
	NodeValue(double v) : value(v) {}
	NodeValue(const char* v) : value(string(v)) {}
	NodeValue(const string& v) : value(v) {}
	NodeValue(const Node& v) : value(make_shared<Node>(v)) {}
	NodeValue(const NodeRef& v) : value(v) {}
	NodeValue(const NodeArray& v) : value(make_shared<NodeArray>(v)) {}
	NodeValue(const NodeArrayRef& v) : value(v) {}
	NodeValue(const any v) : value(v) {}

	NodeValue(initializer_list<pair<const string, NodeValue>> v) : value(make_shared<Node>(v)) {}

	template <bool prioritize = false>
	NodeValue(initializer_list<NodeValue> v) : value(make_shared<NodeArray>(v)) {}

	template<typename T>
	T& get() {
		return ::get<T>(value);
	}

	template<typename T>
	const T& get() const {
		return ::get<T>(value);
	}

	template<typename T>
	auto casted() {
		if constexpr(is_same_v<T, string>) {
			if(holds_alternative<bool>(value)) {
				return get<bool>() ? "true" : "false";
			} else
			if(holds_alternative<int>(value)) {
				return to_string(get<int>());
			} else
			if(holds_alternative<double>(value)) {
				return to_string(get<double>());
			}
		} else
		if(holds_alternative<string>(value)) {
			if constexpr(is_same_v<T, bool>) {
				return get<string>() == "true" ||
					   get<string>() == "1";
			} else
			if constexpr(is_same_v<T, int>) {
				return stoi(get<string>());
			} else
			if constexpr(is_same_v<T, double>) {
				return stod(get<string>());
			}
		}

		return get<T>();
	}

	template<typename T>
	operator T() const {
		if constexpr(is_same_v<T, bool>) {
			if(holds_alternative<nullptr_t>(value))		return false;
			if(holds_alternative<NodeRef>(value))		return !!::get<NodeRef>(value);
			if(holds_alternative<NodeArrayRef>(value))	return !!::get<NodeArrayRef>(value);
		}
		/*
		if constexpr(is_same_v<T, vector<string>>) {
			auto values = ::get<NodeArrayRef>(value);
			T values_;

			for(const NodeValue& value : *values) {
				values_.push_back(value);
			}

			return values_;
		}
		*/

		if(!holds_alternative<T>(value) &&
			holds_alternative<nullptr_t>(value)) return T();

		try {
			return ::get<T>(value);
		} catch(const bad_variant_access& e) {
			cout << "Invalid type chosen to cast-access value ([" << typeid(T).name() << "]), factual is [" << type() << "]" << endl;

			throw;
		}
	}

	template<typename T>
	NodeValue& operator=(T&& v) {
		value = NodeValue(v).value;

		return *this;
	}

	template<typename T>
	bool operator==(const T& v) const {
		return ::get<T>(value) == v;
	}

	template<typename T>
	bool operator!=(const T& v) const {
		return !(*this == v);
	}

	bool operator==(const char* v) const {
		return ::get<string>(value) == v;
	}

	bool operator!=(const char* v) const {
		return !(*this == v);
	}

	/*
	bool operator==(const NodeValue& v) const {
		if(!holds_alternative<any>(value) && !holds_alternative<any>(v.value)) {
			return value == v.value;
		}

		return false;
	}

	bool operator!=(const NodeValue& v) const {
		return !(*this == v);
	}
	*/

	bool empty() const {
		return holds_alternative<nullptr_t>(value);
	}

	int type() const {
		return value.index();
	}
};

class Node {
private:
	unordered_map<string, NodeValue> data;

public:
	Node() {}

	Node(initializer_list<pair<const string, NodeValue>> items) {
		for(auto& item : items) {
			data[item.first] = item.second;
		}
	}

	void set(const string& key, const NodeValue& value) {
		data[key] = value;
	}

	NodeValue& set(const string& key) {
		return data[key];
	}

	/*
	template<typename... Keys, typename T>
	void set(const Keys&... keys, const NodeValue& value) {
		(set(keys, value), ...);
	}
	*/

	template<typename T>
	T get(const string& key, const T& defaultValue = T()) const {
		auto it = data.find(key);

		if(it != data.end()) {
			try {
				return it->second.get<T>();
			} catch(const bad_variant_access& e) {
				cout << "Invalid type chosen to access value with key \"" << key << "\" ([" << typeid(T).name() << "]), factual is [" << it->second.type() << "]" << endl;

				throw;
			}
		}

		return defaultValue;
	}

	auto get(const string& key) const {
		auto it = data.find(key);

		return it != data.end() ? it->second : NodeValue();
	}

	/*
	auto operator[](const string& key) {
		return get(key);
	}

	const auto operator[](const string& key) const {
		return get(key);
	}
	*/

	auto begin() const {
		return data.begin();
	}

	auto end() const {
		return data.end();
	}

	bool empty(const string& key) const {
		auto it = data.find(key);

		return it == data.end() || it->second.empty();
	}

	void remove(const string& key) {
		data.erase(key);
	}
};

string to_string(const Node& node);
string to_string(const NodeArray& node);

string to_string(const NodeValue& value) {
	string result = "";

	switch(value.type()) {
		case 1: result += value.get<bool>() ? "true" : "false";		break;
		case 2: result += to_string(value.get<int>());				break;
		case 3: result += to_string(value.get<double>());			break;
		case 4: result += "\""+value.get<string>()+"\"";			break;
		case 5: result += to_string(*value.get<NodeRef>());			break;
		case 6: result += to_string(*value.get<NodeArrayRef>());	break;
	}

	return result;
}

string to_string(const Node& node) {
	string result = "{";
	auto it = node.begin();

	while(it != node.end()) {
		auto& [k, v] = *it;
		string s = to_string(v);

		if(!s.empty()) {
			result += "\""+k+"\": "+s;

			if(next(it) != node.end()) {
				result += ", ";
			}
		}

		it++;
	}

	result += "}";

	return result;
}

string to_string(const NodeArray& node) {
	string result = "[";
	auto it = node.begin();

	while(it != node.end()) {
		auto& v = *it;
		string s = to_string(v);

		if(!s.empty()) {
			result += s;

			if(next(it) != node.end()) {
				result += ", ";
			}
		}

		it++;
	}

	result += "]";

	return result;
}

namespace glz::detail
{
	/*
	template <>
	struct from_json<Node>
	{
		template <auto Opts>
		static void op(Node& node, auto&&... args)
		{
			read<json>::op<Opts>(node.human_readable, args...);
			node.data = std::stoi(node.human_readable);
		}
	};
	*/

	template <>
	struct to_json<Node>
	{
		template <auto Opts>
		static void op(Node& node, auto&&... args) noexcept
		{
			constexpr glz::opts opts {
				.raw = true
			};

			write<json>::op<opts>(to_string(node), args...);
		}
	};
}

/*
int main() {
	Node node = {
		{"name", "John Doe"},
		{"age", 30},
		{"is_student", false},
		{"address", {
			{"city", "New York"},
			{"zip", 10001},
			{"a", {1, "2", 3.4, true}}
		}},
		{"friends", {"Alice", "Bob"}}
	};

	// Accessing values
	cout << "Name: " << node.get<string>("name") << endl;
	cout << "Age: " << node.get<int>("age") << endl;
	cout << "Is student: " << (node.get("is_student") ? "true" : "false") << endl;
	cout << "City: " << node.get<NodeRef>("address")->get<string>("city") << endl;
	cout << "Zip: " << node.get<NodeRef>("address")->get<int>("zip") << endl;

	NodeArrayRef friendsRetrieved = node.get("friends");
	cout << "Friends: ";
	for(const string& friendName : *friendsRetrieved) {
		cout << friendName << " ";
	}
	cout << endl;

	// Removing a field
	node.remove("age");
	if (node.empty("age")) {
		cout << "Age field is removed.\n";
	}

	// Adding a new child object
	Node newAddress {
		{"city", "San Francisco"},
		{"zip", 94105}
	};
	node.set("new_address") = newAddress;

	cout << "New city: " << node.get<NodeRef>("new_address")->get<string>("city") << endl;
	cout << "New zip: " << node.get<NodeRef>("new_address")->get<int>("zip") << endl;

	// Accessing a non-existent field with default value
	cout << "Non-existent field (with default): " << node.get<string>("non_existent_field", "Default Value") << endl;

	system("pause");

	return 0;
}
*/
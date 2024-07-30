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

string to_string(const Node& node);
string to_string(const NodeArray& node);

class NodeValue {
private:
	mutable variant<nullptr_t, bool, int, double, string, NodeRef, NodeArrayRef, any> value;

public:
	NodeValue() : value(nullptr) {}
	NodeValue(nullptr_t v) : value(v) {}
	NodeValue(bool v) : value(v) {}
	NodeValue(int v) : value(v) {}
	NodeValue(double v) : value(v) {}
	NodeValue(const char* v) : value(string(v)) {}
	NodeValue(const string& v) : value(v) {}
	NodeValue(const Node& v) : value(make_shared<Node>(v)) {}
	NodeValue(const NodeRef& v) : value(v ?: static_cast<decltype(value)>(nullptr)) {}
	NodeValue(const NodeArray& v) : value(make_shared<NodeArray>(v)) {}
	NodeValue(const NodeArrayRef& v) : value(v ?: static_cast<decltype(value)>(nullptr)) {}
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
	operator T() const {
		if(holds_alternative<T>(value))			return ::get<T>(value);
		if(holds_alternative<nullptr_t>(value))	return T();

		cout << "Invalid type chosen to cast-access value ([" << typeid(T).name() << "]), factual is [" << type() << "]" << endl;

		throw bad_variant_access();
	}

	operator bool() const {
		switch(type()) {
			case 1:		return ::get<bool>(value);
			case 2:		return ::get<int>(value);
			case 3:		return ::get<double>(value);
			case 4:		return ::get<string>(value) == "true" || ::get<string>(value) == "1";
			case 5:		return !!::get<NodeRef>(value);
			case 6:		return !!::get<NodeArrayRef>(value);
			default:	return bool();
		}
	}

	operator int() const {
		switch(type()) {
			case 0:		return int();
			case 1:		return ::get<bool>(value);
			case 2:		return ::get<int>(value);
			case 3:		return ::get<double>(value);
			default:	throw bad_variant_access();
		}
	}

	operator double() const {
		switch(type()) {
			case 0:		return double();
			case 1:		return ::get<bool>(value);
			case 2:		return ::get<int>(value);
			case 3:		return ::get<double>(value);
			default:	throw bad_variant_access();
		}
	}

	operator string() const {
		switch(type()) {
			case 1:		return ::get<bool>(value) ? "true" : "false";
			case 2:		return to_string(::get<int>(value));
			case 3:		return to_string(::get<double>(value));
			case 4:		return ::get<string>(value);
			case 5:		return to_string(*::get<NodeRef>(value));
			case 6:		return to_string(*::get<NodeArrayRef>(value));
			default:	return string();
		}
	}

	operator Node&() const {
		return *::get<NodeRef>(value);
	}

	operator NodeArray&() const {
		return *::get<NodeArrayRef>(value);
	}

	/*
	operator vector<string>() const {
		auto values = ::get<NodeArrayRef>(value);
		vector<string> values_;

		for(const NodeValue& value : *values) {
			values_.push_back(value);
		}

		return values_;
	}
	*/

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
		return !holds_alternative<any>(value) && !holds_alternative<any>(v.value) && value == v.value;
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

	Node(initializer_list<pair<const string, NodeValue>> items) : data(items) {}

	template<typename T>
	T get(const string& key, const NodeValue& defaultValue = nullptr) const {
		auto it = data.find(key);

		return it != data.end() ? it->second : defaultValue;
	}

	NodeValue& get(const string& key) {
		return data[key];
	}

	const NodeValue& get(const string& key) const {
		return data.at(key);
	}

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

	void clear() {
		data.clear();
	}
};

string to_string(const NodeValue& value) {
	if(value.type() == 4) {
		return "\""+(string)value+"\"";
	}

	return value;
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
	cout << "City: " << node.get<Node&>("address").get<string>("city") << endl;
	cout << "Zip: " << node.get<Node&>("address").get<int>("zip") << endl;

	NodeArray& friendsRetrieved = node.get("friends");
	cout << "Friends: ";
	for(const string& friendName : friendsRetrieved) {
		cout << friendName << " ";
	}
	cout << endl;

	// Removing a field
	node.remove("age");
	if(node.empty("age")) {
		cout << "Age field is removed.\n";
	}

	// Adding a new child object
	Node newAddress {
		{"city", "San Francisco"},
		{"zip", 94105}
	};
	node.get("new_address") = newAddress;

	cout << "New city: " << node.get<Node&>("new_address").get<string>("city") << endl;
	cout << "New zip: " << node.get<Node&>("new_address").get<int>("zip") << endl;

	// Accessing a non-existent field with default value
	cout << "Non-existent field (with default): " << node.get<string>("non_existent_field", "Default Value") << endl;

	system("pause");

	return 0;
}
*/
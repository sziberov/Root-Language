#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <optional>
#include <any>

#include "glaze/glaze.hpp"

using namespace std;

template <typename T>
constexpr auto type_name() {
	string_view name,
				prefix,
				suffix;

	#ifdef __clang__
		name = __PRETTY_FUNCTION__;
		prefix = "auto type_name() [T = ";
		suffix = "]";
	#elif defined(__GNUC__)
		name = __PRETTY_FUNCTION__;
		prefix = "constexpr auto type_name() [with T = ";
		suffix = "]";
	#elif defined(_MSC_VER)
		name = __FUNCSIG__;
		prefix = "auto __cdecl type_name<";
		suffix = ">(void)";
	#endif

	name.remove_prefix(prefix.size());
	name.remove_suffix(suffix.size());

	return name;
}

// ----------------------------------------------------------------

class Node;
class NodeValue;

using NodeRef = shared_ptr<Node>;
using NodeArray = vector<NodeValue>;
using NodeArrayRef = shared_ptr<NodeArray>;

static string to_string(const Node& node);
static string to_string(const NodeArray& node);

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
	NodeValue(const any& v) : value(v) {}

	NodeValue(initializer_list<pair<const string, NodeValue>> v) : value(make_shared<Node>(v)) {}

	template <bool prioritize = false>
	NodeValue(initializer_list<NodeValue> v) : value(make_shared<NodeArray>(v)) {}

	template <typename T>
	NodeValue(const optional<T>& v) : value(v ? *v : static_cast<decltype(value)>(nullptr)) {}

	/*
	template <typename T>
	requires (!is_same_v<T, int> &&
	          !is_same_v<T, double> &&
	          !is_same_v<T, string> &&
	          !is_same_v<T, const char*> &&
	          !is_same_v<T, NodeValue>)
	NodeValue(const T& v) : value(make_any<T>(v)) {}
	*/

	template <typename T>
	T& get() {
		return ::get<T>(value);
	}

	template <typename T>
	const T& get() const {
		return ::get<T>(value);
	}

	template <typename T>
	operator T() const {
		if(holds_alternative<T>(value))										return get<T>();
		if(holds_alternative<any>(value) && get<any>().type() == typeid(T)) return any_cast<T>(get<any>());
		if(holds_alternative<nullptr_t>(value))								return T();

		cout << "Invalid type chosen to cast-access value (" << type_name<T>() << "), factual is [" << type() << "]" << endl;

		throw bad_variant_access();
	}

	operator bool() const {
		switch(type()) {
			case 1:		return get<bool>();
			case 2:		return get<int>();
			case 3:		return get<double>();
			case 4:		return get<string>() == "true" || get<string>() == "1";
			case 5:		return !!get<NodeRef>();
			case 6:		return !!get<NodeArrayRef>();
			default:	return bool();
		}
	}

	operator int() const {
		switch(type()) {
			case 0:		return int();
			case 1:		return get<bool>();
			case 2:		return get<int>();
			case 3:		return get<double>();
			case 4:     return stoi(get<string>());
			default:	throw bad_variant_access();
		}
	}

	operator double() const {
		switch(type()) {
			case 0:		return double();
			case 1:		return get<bool>();
			case 2:		return get<int>();
			case 3:		return get<double>();
			case 4:     return stod(get<string>());
			default:	throw bad_variant_access();
		}
	}

	operator string() const {
		switch(type()) {
			case 1:		return get<bool>() ? "true" : "false";
			case 2:		return to_string(get<int>());
			case 3:		return to_string(get<double>());
			case 4:		return get<string>();
			case 5:		return to_string(*get<NodeRef>());
			case 6:		return to_string(*get<NodeArrayRef>());
			default:	return string();
		}
	}

	operator Node&() const {
		return *get<NodeRef>();
	}

	operator NodeArray&() const {
		return *get<NodeArrayRef>();
	}

	template <typename T, template <typename, typename = allocator<T>> class Container>
	operator Container<T>() const {
		auto values = get<NodeArrayRef>();
		Container<T> values_;

		for(const T& value : *values) {
			values_.push_back(value);
		}

		return values_;
	}

	template <typename T>
	NodeValue& operator=(T&& v) {
		value = NodeValue(v).value;

		return *this;
	}

	/*
	template <typename T>
	NodeValue& operator+=(const T& v) {
		value = ((T)*this)+v;

		return *this;
	}

	template <typename T>
	NodeValue& operator-=(const T& v) {
		value = ((T)*this)-v;

		return *this;
	}
	*/

	template <typename T>
	bool operator==(const T& v) const {
		return holds_alternative<T>(value) && get<T>() == v;
	}

	template <typename T>
	bool operator!=(const T& v) const {
		return !(*this == v);
	}

	bool operator==(const char* v) const {
		return holds_alternative<string>(value) && get<string>() == v;
	}

	bool operator!=(const char* v) const {
		return !(*this == v);
	}

	bool operator==(const NodeValue& v) const {
		switch(type()) {
			case 0:     return *this == (nullptr_t)v;
			case 1:		return *this == (bool)v;
			case 2:		return *this == (int)v;
			case 3:		return *this == (double)v;
			case 4:		return *this == (string)v;
			case 5:		return *this == (NodeRef)v;
			case 6:		return *this == (NodeArrayRef)v;
			default:	return false;
		}
	}

	/*
	bool operator==(const NodeValue& v) const {
		if(type() != v.type()) {
			return false;
		}

		switch(type()) {
			case 0:     return true;
			case 1:		return get<bool>() == v.get<bool>();
			case 2:		return get<int>() == v.get<int>();
			case 3:		return get<double>() == v.get<double>();
			case 4:		return get<string>() == v.get<string>();
			case 5:		return get<NodeRef>() == v.get<NodeRef>();
			case 6:		return get<NodeArrayRef>() == v.get<NodeArrayRef>();
			default:	return false;
		}
	}
	*/

	bool operator!=(const NodeValue& v) const {
		return !(*this == v);
	}

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

	template <typename T>
	T get(const string& key, const NodeValue& defaultValue = nullptr) const {
		auto it = data.find(key);

		return it != data.end() && !it->second.empty() ? it->second : defaultValue;
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

	bool contains(const string& key) {
		return data.count(key) > 0;
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

static string to_string(const NodeValue& value) {
	if(value.type() == 4) {
		return "\""+(string)value+"\"";
	}

	return value;
}

static string to_string(const Node& node) {
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

static string to_string(const NodeArray& node) {
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
	struct to_json<NodeValue>
	{
		template <auto Opts>
		static void op(NodeValue& value, auto&&... args) noexcept
		{
			constexpr glz::opts opts {
				.raw = true
			};

			write<json>::op<opts>(to_string(value), args...);
		}
	};

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
#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <any>

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
			if(holds_alternative<int>(value)) {
				return to_string(get<int>());
			} else
			if(holds_alternative<double>(value)) {
				return to_string(get<double>());
			} else
			if(holds_alternative<bool>(value)) {
				return get<bool>() ? "true" : "false";
			}
		} else
		if(holds_alternative<string>(value)) {
			if constexpr(is_same_v<T, int>) {
				return stoi(get<string>());
			} else
			if constexpr(is_same_v<T, double>) {
				return stod(get<string>());
			} else
			if constexpr(is_same_v<T, bool>) {
				return get<string>() == "true" ||
					   get<string>() == "1";
			}
		}

		return get<T>();
	}

	template<typename T>
	operator T() const {
		return ::get<T>(value);
	}

	template<typename T>
	bool operator==(const T& v) {
		return ::get<T>(value) == v;
	}

	bool operator==(const char* v) {
		return ::get<string>(value) == string(v);
	}

	template<typename T>
	bool operator!=(const T& v) {
		return ::get<T>(value) != v;
	}

	bool operator!=(const char* v) {
		return ::get<string>(value) != string(v);
	}

	bool empty() const {
		return holds_alternative<nullptr_t>(value);
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

	template<typename T>
	void set(const string& key, const T& value) {
		data[key] = NodeValue(value);
	}

	template<typename T>
	T get(const string& key, const T& defaultValue = T()) const {
		auto it = data.find(key);

		return it != data.end() ? it->second.get<T>() : defaultValue;
	}

	auto get(const string& key) const {
		auto it = data.find(key);

		return it != data.end() ? it->second : NodeValue();
	}

	auto operator[](const string& key) const {
		return get(key);
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
};

/*
int main() {
	Node node = {
		{"name", "John Doe"},
		{"age", 30},
		{"is_student", false},
		{"address", Node {
			{"city", "New York"},
			{"zip", 10001},
			{"a", NodeArray {1, "2", 3.4, true}}
		}},
		{"friends", NodeArray {"Alice", "Bob"}}
	};

	// Accessing values
	cout << "Name: " << node.get<string>("name") << endl;
	cout << "Age: " << node.get<int>("age") << endl;
	cout << "Is student: " << (node.get<bool>("is_student") ? "true" : "false") << endl;
	cout << "City: " << node.get<NodeRef>("address")->get<string>("city") << endl;
	cout << "Zip: " << node.get<NodeRef>("address")->get<int>("zip") << endl;

	NodeArrayRef friendsRetrieved = node.get<NodeArrayRef>("friends");
	cout << "Friends: ";
	for(const auto& friendName : *friendsRetrieved) {
		cout << friendName.get<string>() << " ";
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
	node.set("new_address", newAddress);

	cout << "New city: " << node.get<NodeRef>("new_address")->get<string>("city") << endl;
	cout << "New zip: " << node.get<NodeRef>("new_address")->get<int>("zip") << endl;

	// Accessing a non-existent field with default value
	cout << "Non-existent field (with default): " << node.get<string>("non_existent_field", "Default Value") << endl;

	system("pause");

	return 0;
}
*/
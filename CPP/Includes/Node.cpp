#pragma once

#include "Std.cpp"
#include "WebSocket.cpp"

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

using NodeSP = sp<Node>;
using NodeArray = vector<NodeValue>;
using NodeArraySP = sp<NodeArray>;

static string to_string(const Node& node);
static string to_string(const NodeArray& node);

class NodeValue {
private:
	mutable variant<nullptr_t, bool, int, double, string, NodeSP, NodeArraySP, any> value;

public:
	bool serialized = false;

	NodeValue() : value(nullptr) {}
	NodeValue(nullptr_t v) : value(v) {}
	NodeValue(bool v) : value(v) {}
	NodeValue(int v) : value(v) {}
	NodeValue(double v) : value(v) {}
	NodeValue(const char* v, bool serialized = false) : value(string(v)), serialized(serialized) {}
	NodeValue(const string& v, bool serialized = false) : value(v), serialized(serialized) {}
	NodeValue(const Node& v) : value(SP<Node>(v)) {}
	NodeValue(const NodeSP& v) : value(v ?: static_cast<decltype(value)>(nullptr)) {}
	NodeValue(const NodeArray& v) : value(SP<NodeArray>(v)) {}
	NodeValue(const NodeArraySP& v) : value(v ?: static_cast<decltype(value)>(nullptr)) {}
	NodeValue(const any& v) : value(v) {}

	NodeValue(initializer_list<pair<const string, NodeValue>> v) : value(SP<Node>(v)) {}

	template <bool prioritize = false>
	NodeValue(initializer_list<NodeValue> v) : value(SP<NodeArray>(v)) {}

	template <typename T>
	NodeValue(const optional<T>& v) : value(v ? *v : static_cast<decltype(value)>(nullptr)) {}

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
			case 5:		return !!get<NodeSP>();
			case 6:		return !!get<NodeArraySP>();
			default:	return bool();
		}
	}

	operator int() const {
		switch(type()) {
			case 1:		return get<bool>();
			case 2:		return get<int>();
			case 3:		return get<double>();
			case 4:     return stoi(get<string>());
			default:	return int();
		}
	}

	operator double() const {
		switch(type()) {
			case 1:		return get<bool>();
			case 2:		return get<int>();
			case 3:		return get<double>();
			case 4:     return stod(get<string>());
			default:	return double();
		}
	}

	operator string() const {
		switch(type()) {
			case 1:		return get<bool>() ? "true" : "false";
			case 2:		return to_string(get<int>());
			case 3:		return to_string(get<double>());
			case 4:		return get<string>();
			case 5:		return to_string(*get<NodeSP>());
			case 6:		return to_string(*get<NodeArraySP>());
			default:	return string();
		}
	}

	operator Node&() const {
		return *get<NodeSP>();
	}

	operator NodeArray&() const {
		return *get<NodeArraySP>();
	}

	template <typename T, template <typename, typename = allocator<T>> class Container>
	operator Container<T>() const {
		auto values = get<NodeArraySP>();
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
			case 0:     return v.empty();
			case 1:		return *this == (bool)v;
			case 2:		return *this == (int)v;
			case 3:		return *this == (double)v;
			case 4:		return *this == (string)v;
			case 5:		return v.type() == 5 && *this == (NodeSP)v;
			case 6:		return v.type() == 6 && *this == (NodeArraySP)v;
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
			case 5:		return get<NodeSP>() == v.get<NodeSP>();
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

string escape_string(const string& input) {
	string result = "\"";
	for(char c : input) {
		switch(c) {
			case '\"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default: result += c; break;
		}
	}
	result += "\"";
	return result;
}

string unescape_string(const string& input) {
	string result;
	usize i = 0;
	while(i < input.length()) {
		if(input[i] == '\\' && i + 1 < input.length()) {
			switch(input[++i]) {
				case '\"': result += '\"'; break;
				case '\\': result += '\\'; break;
				case 'b': result += '\b'; break;
				case 'f': result += '\f'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				default: result += input[i]; break;
			}
		} else {
			result += input[i];
		}
		i++;
	}
	return result;
}

static string to_string(const NodeValue& value) {
	if(value.type() == 4) {
		return value.serialized ? (string)value : escape_string((string)value);
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

class NodeParser {
	string s;
	usize i = 0;

	void skipWhitespace() {
		while(i < s.size() && isspace(s[i])) {
			i++;
		}
	}

	string parseString() {
		string result;
		if(s[i] != '"') return result;
		i++;
		while(i < s.size() && s[i] != '"') {
			if(s[i] == '\\' && i + 1 < s.size()) {
				result += s[++i];
			} else {
				result += s[i];
			}
			i++;
		}
		if(i < s.size() && s[i] == '"') i++;
		return unescape_string(result);
	}

	NodeValue parseNumber() {
		usize start = i;
		bool isFloat = false;
		while(i < s.size() && (isdigit(s[i]) || s[i] == '.' || s[i] == '-' || s[i] == '+')) {
			if(s[i] == '.') isFloat = true;
			i++;
		}
		string num = s.substr(start, i-start);
		try {
			return isFloat ? NodeValue(stod(num)) : NodeValue(stoi(num));
		} catch(...) {
			return nullptr;
		}
	}

	NodeValue parseArray() {
		NodeArray array;
		i++;
		while(i < s.size()) {
			skipWhitespace();
			if(s[i] == ']') { i++; break; }
			array.push_back(parseValue());
			skipWhitespace();
			if(s[i] == ',') i++;
			else if(s[i] == ']') { i++; break; }
			else i++;
		}
		return SP(array);
	}

	NodeValue parseNode() {
		Node node;
		i++;
		while(i < s.size()) {
			skipWhitespace();
			if(s[i] == '}') { i++; break; }

			string key = parseString();
			skipWhitespace();
			if(s[i] == ':') i++;
			skipWhitespace();
			node.get(key) = parseValue();
			skipWhitespace();
			if(s[i] == ',') i++;
			else if(s[i] == '}') { i++; break; }
			else i++;
		}
		return SP(node);
	}

	NodeValue parseValue() {
		skipWhitespace();
		if(i >= s.size()) return nullptr;
		char c = s[i];

		if(c == '"') return parseString();
		if(c == '-' || isdigit(c)) return parseNumber();
		if(s.compare(i, 4, "true") == 0) { i += 4; return true; }
		if(s.compare(i, 5, "false") == 0) { i += 5; return false; }
		if(s.compare(i, 4, "null") == 0) { i += 4; return nullptr; }
		if(c == '[') return parseArray();
		if(c == '{') return parseNode();

		i++;
		return nullptr;
	}

public:
	NodeParser(const string& input) : s(input), i(0) {}

	NodeValue parse() {
		return parseValue();
	}
};
#pragma once

#include <string>
#include <unordered_map>

#include "Type.cpp"

using namespace std;

// ----------------------------------------------------------------

struct Primitive;

using PrimitiveRef = shared_ptr<Primitive>;

struct PrimitiveDictionary {
	using Entry = pair<PrimitiveRef, PrimitiveRef>;

	unordered_map<PrimitiveRef, vector<size_t>> kIndexes;	// key -> indexes
	vector<Entry> iEntries;									// index -> entry

	void emplace(const PrimitiveRef& key, const PrimitiveRef& value) {
		kIndexes[key].push_back(size());
		iEntries.push_back(make_pair(key, value));
	}

	PrimitiveRef get(const PrimitiveRef& key) const {
		auto it = kIndexes.find(key);

		if(it != kIndexes.end() && !it->second.empty()) {
			return iEntries[it->second.back()].second;
		}

		return nullptr;
	}

	bool operator==(const PrimitiveDictionary& dictionary) const {
		return false;
	}

	bool operator!=(const PrimitiveDictionary& dictionary) const {
		return !(*this == dictionary);
	}

	auto begin() const {
		return iEntries.begin();
	}

	auto end() const {
		return iEntries.end();
	}

	size_t size() const {
		return iEntries.size();
	}
};

struct Primitive {
	using Type = variant<bool, PrimitiveDictionary, double, int, int, int, string, TypeRef>;

	mutable Type value;

	Primitive(bool v) : value(v) {}
	Primitive(const PrimitiveDictionary& v) : value(v) {}
	Primitive(double v) : value(v) {}
	Primitive(int v) : value(Type(in_place_index<3>, v)) {}
	Primitive(int v, const string& type) {
		if(type == "integer") {
			value = Type(in_place_index<3>, v);
		} else
		if(type == "pointer") {
			value = Type(in_place_index<4>, v);
		} else
		if(type == "reference") {
			value = Type(in_place_index<5>, v);
		} else {
			throw invalid_argument("Unknown type: "+type+" for value: "+to_string(v));
		}
	}
	Primitive(const string& v) : value(v) {}
	Primitive(const TypeRef& v) : value(v ?: throw invalid_argument("Primitive cannot be nil")) {}

	template <typename T>
	T& get() {
		return ::get<T>(value);
	}

	template <typename T>
	const T& get() const {
		return ::get<T>(value);
	}

	template <size_t I>
	auto& get() {
		return ::get<I>(value);
	}

	template <size_t I>
	const auto& get() const {
		return ::get<I>(value);
	}

	operator bool() const {
		switch(value.index()) {
			case 0:		return get<bool>();
			default:	throw bad_variant_access();
		}
	}

	operator PrimitiveDictionary&() const {
		switch(value.index()) {
			case 1:		return ::get<PrimitiveDictionary>(value);
			default:	throw bad_variant_access();
		}
	}

	operator double() const {
		switch(value.index()) {
			case 2:		return get<double>();
			default:	throw bad_variant_access();
		}
	}

	operator int() const {
		switch(value.index()) {
			case 3:		return get<3>();
			case 4:		return get<4>();
			case 5:		return get<5>();
			default:	throw bad_variant_access();
		}
	}

	operator string() const {
		switch(value.index()) {
			case 0:		return get<bool>() ? "true" : "false";
		//	case 1:		return to_string(get<int>());
			case 2:		return to_string(get<double>());
			case 3:		return to_string(get<3>());
			case 4:		return to_string(get<4>());
			case 5:		return to_string(get<5>());
			case 6:		return get<string>();
			case 7:		return get<TypeRef>()->toString();
			default:	return string();
		}
	}

	operator TypeRef() const {
		switch(value.index()) {
			case 7:		return ::get<TypeRef>(value);
			default:	throw bad_variant_access();
		}
	}

	Primitive& operator=(const Primitive& v) {
		value = v.value;

		return *this;
	}

	Primitive operator+(double v) const {
		switch(value.index()) {
			case 0:		return get<bool>()+v;
			case 2:		return get<double>()+v;
			case 3:		return get<3>()+v;
			case 6:		return get<string>()+to_string(v);
			default:	throw invalid_argument("Invalid operand");
		}
	}

	Primitive operator-(double v) const {
		return *this+(-v);
	}

	Primitive operator+(int v) const {
		switch(value.index()) {
			case 0:		return get<bool>()+v;
			case 2:		return get<double>()+v;
			case 3:		return get<3>()+v;
			case 6:		return get<string>()+to_string(v);
			default:	throw invalid_argument("Invalid operand");
		}
	}

	Primitive operator-(int v) const {
		return *this+(-v);
	}

	Primitive operator-() const {
		switch(value.index()) {
			case 0:		return -get<bool>();
			case 2:		return -get<double>();
			case 3:		return -get<3>();
			case 6:		return -stoi(get<string>());
			default:	throw invalid_argument("Invalid operand");
		}
    }

	Primitive operator*(double v) const {
		switch(value.index()) {
			case 0:		return get<bool>()*v;
			case 2:		return get<double>()*v;
			case 3:		return get<3>()*v;
			case 6:		return stod(get<string>())*v;
			default:	throw invalid_argument("Invalid operand");
		}
	}

	Primitive operator*(int v) const {
		switch(value.index()) {
			case 0:		return get<bool>()*v;
			case 2:		return get<double>()*v;
			case 3:		return get<3>()*v;
			case 6:		return stoi(get<string>())*v;
			default:	throw invalid_argument("Invalid operand");
		}
	}

	Primitive operator!() const {
		switch(value.index()) {
			case 0:		return !get<bool>();
			case 2:		return !get<double>();
			case 3:		return !get<3>();
			case 6:		return !stoi(get<string>());
			default:	throw invalid_argument("Invalid operand");
		}
    }

	bool operator==(const Primitive& v) const {
		switch(value.index()) {
			case 0:     return get<bool>() == (bool)v;
			case 1:		return get<PrimitiveDictionary>() == (PrimitiveDictionary)v;
			case 2:		return get<double>() == (double)v;
			case 3:		return get<3>() == (int)v;
			case 4:		return get<4>() == (int)v;
			case 5:		return get<5>() == (int)v;
			case 6:		return get<string>() == (string)v;
			default:	return false;
		}
	}

	string type() const {
		switch(value.index()) {
			case 0:		return "boolean";
			case 1:		return "dictionary";
			case 2:		return "float";
			case 3:		return "integer";
			case 4:		return "pointer";
			case 5:		return "reference";
			case 6:		return "string";
			case 7:		return "type";
			default:	return string();
		}
	}
};
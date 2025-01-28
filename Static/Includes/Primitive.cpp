#pragma once

#include <string>
#include <unordered_map>

#include "Node.cpp"

using namespace std;

// ----------------------------------------------------------------

template <typename T>
struct DictionaryHasher {
	size_t operator()(const optional<T>& value) const;
};

template <typename T>
struct Dictionary {
	using Entry = pair<optional<T>, optional<T>>;

	unordered_map<optional<T>, vector<size_t>, DictionaryHasher<T>, equal_to<optional<T>>> kIndexes;	// key -> indexes
	vector<Entry> iEntries;																				// index -> entry

	void emplace(const optional<T>& key, const optional<T>& value) {
		kIndexes[key].push_back(size());
		iEntries.push_back(make_pair(key, value));
	}

	optional<T> get(const T& key) const {
		auto it = kIndexes.find(key);

		if(it != kIndexes.end() && !it->second.empty()) {
			return iEntries[it->second.back()].second;
		}

		return nullopt;
	}

	bool operator==(const Dictionary<T>& dictionary) const {
		return false;
	}

	bool operator!=(const Dictionary<T>& dictionary) const {
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

struct Primitive;

using PrimitiveDictionary = Dictionary<Primitive>;
using PrimitiveDictionaryHasher = DictionaryHasher<Primitive>;

struct Primitive {
	using Type = variant<bool, PrimitiveDictionary, double, int, int, int, string, NodeRef>;

	mutable Type value;

	Primitive(bool v) : value(Type(in_place_index<0>, v)) {}
	Primitive(const PrimitiveDictionary& v) : value(Type(in_place_index<1>, v)) {}
	Primitive(double v) : value(Type(in_place_index<2>, v)) {}
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
	Primitive(const string& v) : value(Type(in_place_index<6>, v)) {}
	Primitive(NodeRef v) : value(Type(in_place_index<7>, v)) {}

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
			case 7:		return get<NodeRef>() ? to_string(*get<NodeRef>()) : string();
			default:	return string();
		}
	}

	operator NodeRef() const {
		switch(value.index()) {
			case 7:		return get<NodeRef>();
			default:	throw bad_variant_access();
		}
	}

	bool operator==(const Primitive& v) const {
		switch(value.index()) {
			case 0:     return (bool)(*this) == (bool)v;
			case 1:		return (PrimitiveDictionary)(*this) == (PrimitiveDictionary)v;
			case 2:		return (double)(*this) == (double)v;
			case 3:
			case 4:
			case 5:		return (int)(*this) == (int)v;
			case 6:		return (string)(*this) == (string)v;
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

template <>
size_t PrimitiveDictionaryHasher::operator()(const optional<Primitive>& primitive) const {
	if(!primitive) {
		return 0;
	}

	return visit([](const auto& v) {
		using T = decay_t<decltype(v)>;

		if constexpr (is_same_v<T, PrimitiveDictionary>) {
			size_t hash = 0;

			for(const auto& [key, value] : v) {
				hash ^= PrimitiveDictionaryHasher()(key)^(PrimitiveDictionaryHasher()(value) << 1);
			}

			return hash;
		} else {
			return hash<T>()(v);
		}
	}, primitive->value);
}
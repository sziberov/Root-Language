#pragma once

#include <string>
#include <unordered_map>

#include "Node.cpp"

using namespace std;

// ----------------------------------------------------------------

struct Primitive;

struct PrimitiveHasher {
	size_t operator()(const Primitive& primitive) const;
};

struct PrimitiveEqual {
	bool operator()(const Primitive& lhs, const Primitive& rhs) const;
};

using PrimitiveDictionary = unordered_map<Primitive, Primitive, PrimitiveHasher, PrimitiveEqual>;

struct Primitive {
	using Type = variant<bool, PrimitiveDictionary, double, int, int, int, string, NodeRef>;

	Type value;

	Primitive(bool val) : value(Type(in_place_index<0>, val)) {}
	Primitive(const PrimitiveDictionary& val) : value(Type(in_place_index<1>, val)) {}
	Primitive(double val) : value(Type(in_place_index<2>, val)) {}
	Primitive(int val) : value(Type(in_place_index<3>, val)) {}
	Primitive(int val, const string& type) {
		if(type == "integer") {
			value = Type(in_place_index<3>, val);
		} else
		if(type == "pointer") {
			value = Type(in_place_index<4>, val);
		} else
		if(type == "reference") {
			value = Type(in_place_index<5>, val);
		} else {
			throw invalid_argument("Unknown type: "+type+" for value: "+to_string(val));
		}
	}
	Primitive(const string& val) : value(Type(in_place_index<6>, val)) {}
	Primitive(NodeRef val) : value(Type(in_place_index<7>, val)) {}

	operator bool() const {
		switch(value.index()) {
			case 0:		return get<bool>(value);
			default:	throw bad_variant_access();
		}
	}

	operator PrimitiveDictionary() const {
		switch(value.index()) {
			case 1:		return get<PrimitiveDictionary>(value);
			default:	throw bad_variant_access();
		}
	}

	operator double() const {
		switch(value.index()) {
			case 2:		return get<double>(value);
			default:	throw bad_variant_access();
		}
	}

	operator int() const {
		switch(value.index()) {
			case 3:		return get<3>(value);
			case 4:		return get<4>(value);
			case 5:		return get<5>(value);
			default:	throw bad_variant_access();
		}
	}

	operator string() const {
		switch(value.index()) {
			case 0:		return get<bool>(value) ? "true" : "false";
		//	case 1:		return to_string(get<int>(value));
			case 2:		return to_string(get<double>(value));
			case 3:		return to_string(get<3>(value));
			case 4:		return to_string(get<4>(value));
			case 5:		return to_string(get<5>(value));
			case 6:		return get<string>(value);
			case 7:		return get<NodeRef>(value) ? to_string(*get<NodeRef>(value)) : string();
			default:	return string();
		}
	}

	operator NodeRef() const {
		switch(value.index()) {
			case 7:		return get<NodeRef>(value);
			default:	throw bad_variant_access();
		}
	}

	bool operator==(const Primitive& primitive) const {
		return value == primitive.value;
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

size_t PrimitiveHasher::operator()(const Primitive& primitive) const {
	return visit(
		[](const auto& v) {
			using T = decay_t<decltype(v)>;

			if constexpr (is_same_v<T, PrimitiveDictionary>) {
				size_t mapHash = 0;

				for(const auto& pair : v) {
					mapHash ^= PrimitiveHasher()(pair.first)^(PrimitiveHasher()(pair.second) << 1);
				}

				return mapHash;
			} else {
				return hash<T>()(v);
			}
		},
		primitive.value
	);
}

bool PrimitiveEqual::operator()(const Primitive& lhs, const Primitive& rhs) const {
	return lhs == rhs;
}
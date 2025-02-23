#pragma once

#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <cassert>

#include "Std.cpp"

// ----------------------------------------------------------------

struct TypVal;
struct Type;
struct Value;

struct ParenthesizedType;
struct NillableType;
struct DefaultType;
struct UnionType;
struct IntersectionType;

struct PredefinedType;
struct PrimitiveValue;
struct DictionaryValue;
struct CompositeValue;
struct ReferenceType;

struct FunctionType;
struct InoutType;
struct VariadicType;

// ----------------------------------------------------------------

using TypValRef = shared_ptr<TypVal>;
using TypeRef = shared_ptr<Type>;
using ValueRef = shared_ptr<Value>;

using ParenthesizedTypeRef = shared_ptr<ParenthesizedType>;
using NillableTypeRef = shared_ptr<NillableType>;
using DefaultTypeRef = shared_ptr<DefaultType>;
using UnionTypeRef = shared_ptr<UnionType>;
using IntersectionTypeRef = shared_ptr<IntersectionType>;

using PredefinedTypeRef = shared_ptr<PredefinedType>;
using PrimitiveValueRef = shared_ptr<PrimitiveValue>;
using DictionaryValueRef = shared_ptr<DictionaryValue>;
using CompositeValueRef = shared_ptr<CompositeValue>;
using ReferenceTypeRef = shared_ptr<ReferenceType>;

using FunctionTypeRef = shared_ptr<FunctionType>;
using InoutTypeRef = shared_ptr<InoutType>;
using VariadicTypeRef = shared_ptr<VariadicType>;

// ----------------------------------------------------------------

enum class TypValID : uint8_t {
	Undefined,

	Parenthesized,
	Nillable,
	Default,
	Union,
	Intersection,

	Predefined,
	Primitive,
	Array,
	Dictionary,
	Composite,
	Reference,

	Function,
	Inout,
	Variadic
};

enum class PredefinedTypeID : uint8_t {
	// Extremes
	EVoid,
	EAny,

	// Primitives
	PAny,
	PBoolean,
	PDictionary,
	PFloat,
	PInteger,
	PString,
	PType,

	// Composites
	CAny,
	CClass,
	CEnumeration,
	CFunction,
	CNamespace,
	CObject,
	CProtocol,
	CStructure
};

enum class PrimitiveValueID : uint8_t {
	Boolean,
	Float,
	Integer,
	String,
	Type
};

enum class CompositeValueID : uint8_t {
	Class,
	Enumeration,
	Function,
	Namespace,
	Object,
	Protocol,
	Structure
};

// ----------------------------------------------------------------

extern const TypeRef PredefinedEVoidTypeRef;
extern const TypeRef PredefinedEAnyTypeRef;

extern const TypeRef PredefinedPAnyTypeRef;
extern const TypeRef PredefinedPBooleanTypeRef;
extern const TypeRef PredefinedPFloatTypeRef;
extern const TypeRef PredefinedPIntegerTypeRef;
extern const TypeRef PredefinedPStringTypeRef;
extern const TypeRef PredefinedPTypeTypeRef;

extern const TypeRef PredefinedCAnyTypeRef;
extern const TypeRef PredefinedCClassTypeRef;
extern const TypeRef PredefinedCEnumerationTypeRef;
extern const TypeRef PredefinedCFunctionTypeRef;
extern const TypeRef PredefinedCNamespaceTypeRef;
extern const TypeRef PredefinedCObjectTypeRef;
extern const TypeRef PredefinedCProtocolTypeRef;
extern const TypeRef PredefinedCStructureTypeRef;

// ----------------------------------------------------------------

struct TypVal : public enable_shared_from_this<TypVal> {
	const TypValID ID;
	const bool concrete;

	TypVal(TypValID ID = TypValID::Undefined, bool concrete = false) : ID(ID), concrete(concrete) { /*cout << "TypVal created\n";*/ }
	virtual ~TypVal() { /*cout << "TypVal destroyed\n";*/ }

	virtual bool representedBy(const any& value) const { return false; }
	virtual string toString() const { return string(); }
};

struct Type : public TypVal {
	Type(TypValID ID = TypValID::Undefined) : TypVal(ID, false) { /*cout << "Type created\n";*/ }

	virtual bool acceptsA(const TypeRef& type) { return false; }
	virtual bool conformsTo(const TypeRef& type) { return type->acceptsA(static_pointer_cast<Type>(shared_from_this())); }
	virtual TypeRef normalized() { return static_pointer_cast<Type>(shared_from_this()); }

	// Now we are planning to normalize all types for one single time before comparison, or using this/similar to this function at worst case, as this is performant-costly operation in C++
	// One thing that also should be mentioned here is that types can't be normalized without losing their initial string representation at the moment
	static bool acceptsANormalized(const TypeRef& left, const TypeRef& right) {
		return left->normalized()->acceptsA(right->normalized());
	}
};

struct Value : public TypVal {
	Value(TypValID ID = TypValID::Undefined) : TypVal(ID, true) {}

	virtual operator bool() const { return bool(); }
	virtual operator double() const { return double(); }
	virtual operator int() const { return int(); }
	virtual operator string() const { return toString(); }
	virtual TypValRef operator+() { return shared_from_this(); }
	virtual TypValRef operator-() { return shared_from_this(); }
	virtual TypValRef operator+(TypValRef t) const { return Ref<Value>(); }
	virtual TypValRef operator-(TypValRef t) const { return t->concrete ? operator+(static_pointer_cast<Value>(t)->operator-()) : nullptr; }
	virtual TypValRef operator*(TypValRef t) const { return Ref<Value>(); }
	virtual TypValRef operator/(TypValRef t) const { return Ref<Value>(); }
	virtual bool operator!() const { return !static_cast<bool>(*this); }
	virtual bool operator==(const TypeRef& t) const { return false; }
	virtual bool operator!=(const TypeRef& t) const { return !operator==(t); }
};

// ----------------------------------------------------------------

struct ParenthesizedType : Type {
	TypeRef innerType;

	ParenthesizedType(TypeRef type) : Type(TypValID::Parenthesized), innerType(move(type)) { cout << "(" << innerType->toString() << ") type created\n"; }
	~ParenthesizedType() { cout << "(" << innerType->toString() << ") type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return innerType->acceptsA(type);
	}

	bool representedBy(const any& value) const override {
		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		return innerType->normalized();
	}

	string toString() const override {
		return "("+innerType->toString()+")";
	}
};

struct NillableType : Type {
	TypeRef innerType;

	NillableType(TypeRef type) : Type(TypValID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
	~NillableType() { cout << innerType->toString() << "? type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return PredefinedEVoidTypeRef->acceptsA(type) ||
			   type->ID == TypValID::Nillable && innerType->acceptsA(static_pointer_cast<NillableType>(type)->innerType) ||
			   innerType->acceptsA(type);
	}

	bool representedBy(const any& value) const override {
		if(!value.has_value()) {
			return true;
		}

		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		TypeRef normInnerType = innerType->normalized();

		if(normInnerType->ID == TypValID::Nillable) {
			return normInnerType;
		}

		return Ref<NillableType>(normInnerType);
	}

	string toString() const override {
		return innerType->toString()+"?";
	}
};

struct DefaultType : Type {
	TypeRef innerType;

	DefaultType(TypeRef type) : Type(TypValID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
	~DefaultType() { cout << innerType->toString() << "? type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return PredefinedEVoidTypeRef->acceptsA(type) ||
			   type->ID == TypValID::Nillable && innerType->acceptsA(static_pointer_cast<DefaultType>(type)->innerType) ||
			   innerType->acceptsA(type);
	}

	bool representedBy(const any& value) const override {
		if(!value.has_value()) {
			return true;
		}

		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		auto normInnerType = innerType->normalized();

		if(normInnerType->ID == TypValID::Nillable) {
			return normInnerType;
		}

		return Ref<DefaultType>(normInnerType);
	}

	string toString() const override {
		return innerType->toString()+"!";
	}
};

struct UnionType : Type {
	vector<TypValRef> alternatives;

	UnionType(const vector<TypValRef>& alternatives) : Type(TypValID::Union), alternatives(alternatives) {}

	bool acceptsA(const TypeRef& type) override {
		for(const TypeRef& alt : alternatives) {
			if(alt->acceptsA(type)) {
				return true;
			}
		}

		return false;
	}

	bool representedBy(const any& value) const override {
		for(const TypeRef& alt : alternatives) {
			if(alt->representedBy(value)) {
				return true;
			}
		}

		return false;
	}

	TypeRef normalized() override {
		vector<TypValRef> normAlts;

		for(const TypeRef& alt : alternatives) {
			TypValRef normAlt = alt->normalized();

			if(normAlt->ID == TypValID::Union) {
				auto normUnionAlt = static_pointer_cast<UnionType>(normAlt);

				normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
			} else {
				normAlts.push_back(normAlt);
			}
		}

		if(normAlts.size() == 1) {
			return normAlts[0];
		}

		return Ref<UnionType>(normAlts);
	}

	string toString() const override {
		string result;

		for(int i = 0; i < alternatives.size(); i++) {
			if(i > 0) {
				result += " | ";
			}

			result += alternatives[i]->toString();
		}

		return result;
	}
};

struct IntersectionType : Type {
	vector<TypValRef> alternatives;

	IntersectionType(const vector<TypValRef>& alternatives) : Type(TypValID::Intersection), alternatives(alternatives) {}

	bool acceptsA(const TypeRef& type) override {
		for(const TypeRef& alt : alternatives) {
			if(!alt->acceptsA(type)) {
				return false;
			}
		}

		return true;
	}

	bool representedBy(const any& value) const override {
		for(const TypeRef& alt : alternatives) {
			if(!alt->representedBy(value)) {
				return false;
			}
		}

		return true;
	}

	TypeRef normalized() override {
		vector<TypValRef> normAlts;

		for(const TypeRef& alt : alternatives) {
			TypValRef normAlt = alt->normalized();

			if(normAlt->ID == TypValID::Intersection) {
				auto normUnionAlt = static_pointer_cast<IntersectionType>(normAlt);

				normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
			} else {
				normAlts.push_back(normAlt);
			}
		}

		if(normAlts.size() == 1) {
			return normAlts[0];
		}

		return Ref<IntersectionType>(normAlts);
	}

	string toString() const override {
		string result;

		for(int i = 0; i < alternatives.size(); i++) {
			if(i > 0) {
				result += " & ";
			}

			result += alternatives[i]->toString();
		}

		return result;
	}
};

// ----------------------------------------------------------------

struct PredefinedType : Type {
	const PredefinedTypeID subID;
	function<bool(const TypeRef&)> acceptsFn;

	PredefinedType(PredefinedTypeID subID, function<bool(const TypeRef&)> acceptsFn) : Type(TypValID::Predefined), subID(subID), acceptsFn(acceptsFn) {}

	bool acceptsA(const TypeRef& type) override {
		return acceptsFn(type);
	}

	string toString() const override {
		switch(subID) {
			case PredefinedTypeID::EVoid:			return "void";
			case PredefinedTypeID::EAny:			return "_";

			case PredefinedTypeID::PAny:			return "any";
			case PredefinedTypeID::PBoolean:		return "bool";
			case PredefinedTypeID::PDictionary:		return "dict";
			case PredefinedTypeID::PFloat:			return "float";
			case PredefinedTypeID::PInteger:		return "int";
			case PredefinedTypeID::PString:			return "string";
			case PredefinedTypeID::PType:			return "type";

			case PredefinedTypeID::CAny:			return "Any";
			case PredefinedTypeID::CClass:			return "Class";
			case PredefinedTypeID::CEnumeration:	return "Enumeration";
			case PredefinedTypeID::CFunction:		return "Function";
			case PredefinedTypeID::CNamespace:		return "Namespace";
			case PredefinedTypeID::CObject:			return "Object";
			case PredefinedTypeID::CProtocol:		return "Protocol";
			case PredefinedTypeID::CStructure:		return "Structure";
		}

		return string();
	}
};

struct PrimitiveValue : Value {
	const PrimitiveValueID subID;
	const any value;

	PrimitiveValue(const bool& v) : Value(TypValID::Primitive), subID(PrimitiveValueID::Boolean), value(v) { cout << toString() << " type created\n"; }
	PrimitiveValue(const double& v) : Value(TypValID::Primitive), subID(PrimitiveValueID::Float), value(v) { cout << toString() << " type created\n"; }
	PrimitiveValue(const int& v) : Value(TypValID::Primitive), subID(PrimitiveValueID::Integer), value(v) { cout << toString() << " type created\n"; }
	PrimitiveValue(const string& v) : Value(TypValID::Primitive), subID(PrimitiveValueID::String), value(v) { cout << toString() << " type created\n"; }
	PrimitiveValue(const TypeRef& v) : Value(TypValID::Primitive), subID(PrimitiveValueID::Type), value(v ?: throw invalid_argument("Primitive type cannot be represented by nil")) { cout << toString() << " type created\n"; }
	~PrimitiveValue() { cout << toString() << " type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypValID::Primitive) {
			return subID == static_pointer_cast<PrimitiveValue>(type)->subID;
		}

		return false;
	}

	bool representedBy(const any& value) const override {
		try {
			switch(subID) {
				case PrimitiveValueID::Boolean:	return value.type() == typeid(bool);
				case PrimitiveValueID::Float:	return value.type() == typeid(float) ||
													   value.type() == typeid(double);
				case PrimitiveValueID::Integer:	return value.type() == typeid(int);
				case PrimitiveValueID::String:	return value.type() == typeid(string);
				case PrimitiveValueID::Type:	return value.type() == typeid(TypValRef);
			}
		} catch(const bad_any_cast&) {
			return false;
		}

		return false;
	}

	string toString() const override {
		switch(subID) {
			case PrimitiveValueID::Boolean:	return "bool";
			case PrimitiveValueID::Float:	return "float";
			case PrimitiveValueID::Integer:	return "int";
			case PrimitiveValueID::String:	return "string";
			case PrimitiveValueID::Type:	return "type";
		}

		return string();
	}

	operator bool() const override {
		switch(subID) {
			case PrimitiveValueID::Boolean:	return any_cast<bool>(value);
			case PrimitiveValueID::Float:	return any_cast<double>(value) > 0;
			case PrimitiveValueID::Integer:	return any_cast<int>(value) > 0;
			case PrimitiveValueID::String:	return any_cast<string>(value).size() > 0;
			default:						return false;
		}
	}

	operator double() const override {
		return double();
	}

	operator int() const override {
		return int();
	}

	operator string() const override {
		return toString();
	}
};

struct DictionaryValue : Value {
	TypeRef keyType,
			valueType;

	DictionaryValue(const TypeRef& keyType, const TypeRef& valueType) : Value(TypValID::Dictionary), keyType(keyType), valueType(valueType) {}

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypValID::Dictionary) {
			auto dictType = static_pointer_cast<DictionaryValue>(type);

			return keyType->acceptsA(dictType->keyType) && valueType->acceptsA(dictType->valueType);
		}

		return false;
	}

	TypeRef normalized() override {
		return Ref<DictionaryValue>(keyType->normalized(), valueType->normalized());
	}

	string toString() const override {
		return "["+keyType->toString()+": "+valueType->toString()+"]";
	}
};

// Global composite storage is allowed, as it decided by design to have only one Interpreter instance in a single process memory
static vector<CompositeValueRef> composites;

struct CompositeValue : Value {
	int index;
	const CompositeValueID subID;
	string title;
	vector<int> inheritedTypes;
	vector<TypeRef> genericParameterTypes;

	CompositeValue(CompositeValueID subID,
				  const string& title,
				  const vector<int>& inheritedTypes = {},
				  const vector<TypeRef>& genericParameterTypes = {}) : Value(TypValID::Composite),
																	   index(composites.size()),
																	   subID(subID),
																	   title(title),
																	   inheritedTypes(inheritedTypes),
																	   genericParameterTypes(genericParameterTypes)
	{
		composites.push_back(static_pointer_cast<CompositeValue>(shared_from_this()));
	}

	set<int> getFullInheritanceChain() const {
		auto chain = set<int>(inheritedTypes.begin(), inheritedTypes.end());

		for(int parentIndex : inheritedTypes) {
			set<int> parentChain = composites[parentIndex]->getFullInheritanceChain();

			chain.insert(parentChain.begin(), parentChain.end());
		}

		return chain;
	}

	static bool checkConformance(const CompositeValueRef& base, const CompositeValueRef& candidate, const optional<vector<TypeRef>>& candidateGenericArgumentTypes = {}) {
		if(candidate->index != base->index && !candidate->getFullInheritanceChain().contains(base->index)) {
			return false;
		}

		if(candidateGenericArgumentTypes) {
			if(candidate->genericParameterTypes.size() != candidateGenericArgumentTypes->size()) {
				return false;
			}
			for(int i = 0; i < candidate->genericParameterTypes.size(); i++) {
				if(!candidate->genericParameterTypes[i]->acceptsA((*candidateGenericArgumentTypes)[i])) {
					return false;
				}
			}
		}

		return true;
	}

	bool acceptsA(const TypeRef& type) override;

	string toString() const override {
		string result;

		switch(subID) {
			case CompositeValueID::Class:		result += "class";
			case CompositeValueID::Enumeration:	result += "enum";
			case CompositeValueID::Function:	result += "func";
			case CompositeValueID::Namespace:	result += "namespace";
			case CompositeValueID::Object:		result += "object";
			case CompositeValueID::Protocol:	result += "protocol";
			case CompositeValueID::Structure:	result += "structure";
		}

		result += " "+title+"#"+to_string(index);

		if(!genericParameterTypes.empty()) {
			result += "<";

			for(int i = 0; i < genericParameterTypes.size(); i++) {
				if(i > 0) {
					result += ", ";
				}

				result += genericParameterTypes[i]->toString();
			}

			result += ">";
		}

		if(!inheritedTypes.empty()) {
			set<int> chain = getFullInheritanceChain();
			bool first = true;

			result += " : [";

			for(int index : chain) {
				if(!first) {
					result += ", ";
				}

				first = false;
				result += index;
			}

			result += "]";
		}

		return result;
	}
};

struct ReferenceType : Type {
	CompositeValueRef compType;
	optional<vector<TypeRef>> typeArgs;

	ReferenceType(const CompositeValueRef& compType, const optional<vector<TypeRef>>& typeArgs = nullopt) : Type(TypValID::Reference), compType(compType), typeArgs(typeArgs) {}

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypValID::Composite) {
			auto compType = static_pointer_cast<CompositeValue>(type);

			return CompositeValue::checkConformance(compType, compType, typeArgs);
		}
		if(type->ID == TypValID::Reference) {
			auto refType = static_pointer_cast<ReferenceType>(type);

			return CompositeValue::checkConformance(compType, refType->compType, refType->typeArgs);
		}

		return false;
	}

	TypeRef normalized() override {
		if(!typeArgs) {
			return Type::normalized();
		}

		vector<TypeRef> normArgs;

		for(const TypeRef& arg : *typeArgs) {
			normArgs.push_back(arg->normalized());
		}

		return Ref<ReferenceType>(compType, normArgs);
	}

	string toString() const override {
		string argsStr = "";

		if(typeArgs) {
			argsStr += "<";

			for(int i = 0; i < typeArgs->size(); i++) {
				if(i > 0) {
					argsStr += ", ";
				}

				argsStr += (*typeArgs)[i]->toString();
			}

			argsStr += ">";
		}

		return "Reference("+compType->title+"#"+to_string(compType->index)+argsStr+")";
	}
};

bool CompositeValue::acceptsA(const TypeRef& type) {
	if(type->ID == TypValID::Composite) {
		auto compThis = static_pointer_cast<CompositeValue>(shared_from_this());
		auto compType = static_pointer_cast<CompositeValue>(type);

		return checkConformance(compThis, compType);
	}
	if(type->ID == TypValID::Reference) {
		auto compThis = static_pointer_cast<CompositeValue>(shared_from_this());
		auto refType = static_pointer_cast<ReferenceType>(type);

		return checkConformance(compThis, refType->compType, refType->typeArgs);
	}

	return false;
}

// ----------------------------------------------------------------

struct FunctionTypeModifiers {
	optional<bool> inits,
				   deinits,
				   awaits,
				   throws;
};

struct FunctionType : Type {
	vector<TypeRef> genericParameterTypes,
					parameterTypes;
	TypeRef returnType;
	FunctionTypeModifiers modifiers;

	FunctionType(const vector<TypeRef>& genericParameterTypes,
				 const vector<TypeRef>& parameterTypes,
				 const TypeRef& returnType,
				 const FunctionTypeModifiers& modifiers) : genericParameterTypes(genericParameterTypes),
														   parameterTypes(parameterTypes),
														   returnType(returnType),
														   modifiers(modifiers) {}

	static bool matchTypeLists(const vector<TypeRef>& expectedList, const vector<TypeRef>& providedList);

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypValID::Function) {
			auto funcType = static_pointer_cast<FunctionType>(type);

			return FunctionType::matchTypeLists(genericParameterTypes, funcType->genericParameterTypes) &&
				   FunctionType::matchTypeLists(parameterTypes, funcType->parameterTypes) &&
				   (!modifiers.inits || funcType->modifiers.inits == modifiers.inits) &&
				   (!modifiers.deinits || funcType->modifiers.deinits == modifiers.deinits) &&
				   (!modifiers.awaits || funcType->modifiers.awaits == modifiers.awaits) &&
				   (!modifiers.throws || funcType->modifiers.throws == modifiers.throws) &&
				   returnType->acceptsA(funcType->returnType);
		}

		return false;
	}

	TypeRef normalized() override {
		return Ref<FunctionType>(
			transform(genericParameterTypes, [](const TypeRef& v) { return v->normalized(); }),
			transform(parameterTypes, [](const TypeRef& v) { return v->normalized(); }),
			returnType->normalized(),
			modifiers
		);
	}

	string toString() const override {
		string result;

		if(!genericParameterTypes.empty()) {
			result += "<";

			for(int i = 0; i < genericParameterTypes.size(); i++) {
				if(i > 0) {
					result += ", ";
				}

				result += genericParameterTypes[i]->toString();
			}

			result += ">";
		}

		result += "(";

		for(int i = 0; i < parameterTypes.size(); i++) {
			if(i > 0) {
				result += ", ";
			}

			result += parameterTypes[i]->toString();
		}

		result += ")";

		result += !modifiers.inits ? " inits?" : *modifiers.inits ? " inits" : "";
		result += !modifiers.deinits ? " deinits?" : *modifiers.deinits ? " deinits" : "";
		result += !modifiers.awaits ? " awaits?" : *modifiers.awaits ? " awaits" : "";
		result += !modifiers.throws ? " throws?" : *modifiers.throws ? " throws" : "";

		result += " -> "+returnType->toString();

		return result;
	}
};

struct InoutType : Type {
	TypeRef innerType;

	InoutType(const TypeRef& innerType) : Type(TypValID::Inout), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) override {
		return type->ID == TypValID::Inout && innerType->acceptsA(static_pointer_cast<InoutType>(type)->innerType);
	}

	bool representedBy(const any& value) const override {
		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		auto normInner = innerType->normalized();

		if(normInner->ID == TypValID::Inout) {
			return normInner;
		}

		return Ref<InoutType>(normInner);
	}

	string toString() const override {
		return "inout "+innerType->toString();
	}
};

struct VariadicType : Type {
	TypeRef innerType;

	VariadicType(const TypeRef& innerType = nullptr) : Type(TypValID::Variadic), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) override {
		if(!innerType) {
			return true;
		}
		if(type->ID != TypValID::Variadic) {
			return innerType->acceptsA(type);
		}

		auto varType = static_pointer_cast<VariadicType>(type);

		if(!varType->innerType) {
			return innerType->acceptsA(PredefinedEVoidTypeRef);
		}

		return innerType->acceptsA(varType->innerType);
	}

	bool representedBy(const any& value) const override {
		if(!innerType) {
			return true;
		}

		/*
		if(Array.isArray(value)) {
			return value.every(v => this.innerType.representedBy(v));
		}
		*/

		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		if(!innerType) {
			return Type::normalized();
		}

		return Ref<VariadicType>(innerType->normalized());
	}

	string toString() const override {
		return (innerType ? innerType->toString() : "")+"...";
	}
};

bool FunctionType::matchTypeLists(const vector<TypeRef>& expectedList, const vector<TypeRef>& providedList) {
	int expectedSize = expectedList.size(),
		providedSize = providedList.size();

	function<bool(int, int)> matchFrom = [&](int expectedIndex, int providedIndex) {
		if(expectedIndex == expectedSize) {
			return providedIndex == providedSize;
		}

		const TypeRef& expectedType = expectedList[expectedIndex];

		if(expectedType->ID == TypValID::Variadic) {
			auto varExpectedType = static_pointer_cast<VariadicType>(expectedType);

			if(expectedIndex == expectedSize-1) {
				return all_of(providedList.begin()+providedIndex, providedList.end(), [&](const TypeRef& t) {
					return varExpectedType->acceptsA(t);
				});
			}

			if(matchFrom(expectedIndex+1, providedIndex)) {
				return true;
			}

			for(int currentIndex = providedIndex; currentIndex < providedSize; currentIndex++) {
				if(!varExpectedType->acceptsA(providedList[currentIndex])) {
					break;
				}
				if(matchFrom(expectedIndex+1, currentIndex+1)) {
					return true;
				}
			}

			return false;
		}

		if(providedIndex < providedSize && expectedType->acceptsA(providedList[providedIndex])) {
			return matchFrom(expectedIndex+1, providedIndex+1);
		}

		return false;
	};

	return matchFrom(0, 0);
}

// ----------------------------------------------------------------

const TypeRef PredefinedEVoidTypeRef = Ref<PredefinedType>(PredefinedTypeID::EVoid, [](const TypeRef& type) {
	return type->ID == TypValID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::EVoid;
});

const TypeRef PredefinedEAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::EAny, [](const TypeRef& type) {
	return true;
});

const TypeRef PredefinedPAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::PAny, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive;
});

const TypeRef PredefinedPBooleanTypeRef = Ref<PredefinedType>(PredefinedTypeID::PBoolean, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive && static_pointer_cast<PrimitiveValue>(type)->subID == PrimitiveValueID::Boolean;
});

const TypeRef PredefinedPDictionaryValueRef = Ref<PredefinedType>(PredefinedTypeID::PDictionary, [](const TypeRef& type) {
	return type->ID == TypValID::Dictionary;
});

const TypeRef PredefinedPFloatTypeRef = Ref<PredefinedType>(PredefinedTypeID::PFloat, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive && static_pointer_cast<PrimitiveValue>(type)->subID == PrimitiveValueID::Float;
});

const TypeRef PredefinedPIntegerTypeRef = Ref<PredefinedType>(PredefinedTypeID::PInteger, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive && static_pointer_cast<PrimitiveValue>(type)->subID == PrimitiveValueID::Integer;
});

const TypeRef PredefinedPStringTypeRef = Ref<PredefinedType>(PredefinedTypeID::PString, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive && static_pointer_cast<PrimitiveValue>(type)->subID == PrimitiveValueID::String;
});

const TypeRef PredefinedPTypeTypeRef = Ref<PredefinedType>(PredefinedTypeID::PType, [](const TypeRef& type) {
	return type->ID == TypValID::Primitive && static_pointer_cast<PrimitiveValue>(type)->subID == PrimitiveValueID::Type;
});

const TypeRef PredefinedCAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::CAny, [](const TypeRef& type) {
	return type->ID == TypValID::Composite ||
		   type->ID == TypValID::Reference;
});

const TypeRef PredefinedCClassTypeRef = Ref<PredefinedType>(PredefinedTypeID::CClass, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Class ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Class;
});

const TypeRef PredefinedCEnumerationTypeRef = Ref<PredefinedType>(PredefinedTypeID::CEnumeration, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Enumeration ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Enumeration;
});

const TypeRef PredefinedCFunctionTypeRef = Ref<PredefinedType>(PredefinedTypeID::CFunction, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Function ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Function;
});

const TypeRef PredefinedCNamespaceTypeRef = Ref<PredefinedType>(PredefinedTypeID::CNamespace, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Namespace ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Namespace;
});

const TypeRef PredefinedCObjectTypeRef = Ref<PredefinedType>(PredefinedTypeID::CObject, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Object ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Object;
});

const TypeRef PredefinedCProtocolTypeRef = Ref<PredefinedType>(PredefinedTypeID::CProtocol, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Protocol ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Protocol;
});

const TypeRef PredefinedCStructureTypeRef = Ref<PredefinedType>(PredefinedTypeID::CStructure, [](const TypeRef& type) {
	return type->ID == TypValID::Composite && static_pointer_cast<CompositeValue>(type)->subID == CompositeValueID::Structure ||
		   type->ID == TypValID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeValueID::Structure;
});

// ----------------------------------------------------------------

/*
void matchTypeListsTest() {
	// Примитивные типы
	TypValRef intType	= Ref<PrimitiveValue>(PrimitiveValueID::Integer);
	TypValRef floatType  = Ref<PrimitiveValue>(PrimitiveValueID::Float);
	TypValRef stringType = Ref<PrimitiveValue>(PrimitiveValueID::String);
	TypValRef boolType   = Ref<PrimitiveValue>(PrimitiveValueID::Boolean);

	// Variadic с внутренним типом
	TypValRef variadicInt	= Ref<VariadicType>(intType);
	TypValRef variadicString = Ref<VariadicType>(stringType);

	// Variadic без внутреннего типа (принимает любой тип)
	TypValRef variadicAny = Ref<VariadicType>();

	// Тест 1: Точное совпадение без вариативных типов.
	vector<TypValRef> expected_1 = { intType, floatType, stringType, boolType },
					provided_1 = { intType, floatType, stringType, boolType };
	assert(FunctionType::matchTypeLists(expected_1, provided_1));

	// Тест 2: Несовпадение списков (неправильный порядок).
	vector<TypValRef> expected_2 = { intType, floatType, stringType },
					provided_2 = { intType, stringType, floatType };
	assert(!FunctionType::matchTypeLists(expected_2, provided_2));

	// Тест 3: Variadic как последний элемент с пустым списком provided.
	vector<TypValRef> expected_3 = { variadicInt },
					provided_3 = {};
	assert(FunctionType::matchTypeLists(expected_3, provided_3));

	// Тест 4: Variadic как последний элемент с несколькими элементами.
	vector<TypValRef> expected_4 = { variadicInt },
					provided_4 = { intType, intType, intType };
	assert(FunctionType::matchTypeLists(expected_4, provided_4));

	// Тест 5: Variadic в середине, покрывающий 0 элементов.
	vector<TypValRef> expected_5 = { intType, variadicInt, stringType },
					provided_5 = { intType, stringType };
	assert(FunctionType::matchTypeLists(expected_5, provided_5));

	// Тест 6: Variadic в середине, покрывающий 1 элемент.
	vector<TypValRef> expected_6 = { intType, variadicInt, stringType },
					provided_6 = { intType, intType, stringType };
	assert(FunctionType::matchTypeLists(expected_6, provided_6));

	// Тест 7: Variadic в середине, покрывающий несколько элементов.
	vector<TypValRef> expected_7 = { intType, variadicInt, stringType },
					provided_7 = { intType, intType, intType, stringType };
	assert(FunctionType::matchTypeLists(expected_7, provided_7));

	// Тест 8: Variadic не соответствует (в Variadic ожидается int, а получен float).
	vector<TypValRef> expected_8 = { intType, variadicInt },
					provided_8 = { intType, floatType };
	assert(!FunctionType::matchTypeLists(expected_8, provided_8));

	// Тест 9: Несколько Variadic подряд с внутренними типами.
	vector<TypValRef> expected_9 = { variadicInt, variadicString },
					provided_9 = { intType, intType, stringType, stringType };
	assert(FunctionType::matchTypeLists(expected_9, provided_9));

	// Тест 10: Variadic без внутреннего типа (variadicAny) как последний элемент, принимает любые типы.
	vector<TypValRef> expected_10 = { variadicAny },
					provided_10 = { intType, floatType, stringType, boolType };
	assert(FunctionType::matchTypeLists(expected_10, provided_10));

	// Тест 11: Variadic без внутреннего типа (variadicAny) в середине.
	vector<TypValRef> expected_11 = { intType, variadicAny, stringType };
	// Здесь variadicAny может покрыть несколько любых типов.
	vector<TypValRef> provided_11 = { intType, boolType, floatType, stringType };
	assert(FunctionType::matchTypeLists(expected_11, provided_11));

	// Тест 12: Variadic без внутреннего типа (variadicAny) в середине, покрывающий 0 элементов.
	vector<TypValRef> expected_12 = { intType, variadicAny, stringType },
					provided_12 = { intType, stringType };
	assert(FunctionType::matchTypeLists(expected_12, provided_12));

	// Тест 13: Variadic в середине, где первый элемент для вариативного типа не соответствует.
	// Здесь variadicInt (ожидает int) не принимает floatType, поэтому цикл должен прерваться.
	vector<TypValRef> expected_13 = { intType, variadicInt, stringType },
					provided_13 = { intType, floatType, stringType };
	assert(!FunctionType::matchTypeLists(expected_13, provided_13));

	// Тест 14: Variadic в середине, где break происходит не сразу, а после одного успешного совпадения.
	// Порядок обработки:
	// - Сначала проверяется пустой диапазон для Variadic — не подходит.
	// - Затем для currentIndex = 1: intType принимается.
	// - Для currentIndex = 2: floatType не принимается variadicInt, цикл прерывается, сопоставление не удаётся.
	vector<TypValRef> expected_14 = { intType, variadicInt, stringType },
					provided_14 = { intType, intType, floatType, stringType };
	assert(!FunctionType::matchTypeLists(expected_14, provided_14));
}

int main() {
	TypValRef intType	= Ref<PrimitiveValue>(PrimitiveValueID::Integer);
	TypValRef nillableInt = Ref<NillableType>(intType);
	TypValRef parenType = Ref<ParenthesizedType>(nillableInt);
	TypValRef nillableParenType = Ref<NillableType>(parenType);

	cout << "int accepts int: " << intType->acceptsA(intType) << "\n";
	cout << "int? accepts int: " << nillableInt->acceptsA(intType) << "\n";
	cout << "(int?) accepts int: " << parenType->acceptsA(intType) << "\n";
	cout << "(int?)? normalized: " << nillableParenType->normalized()->toString() << "\n";

	matchTypeListsTest();
}
*/
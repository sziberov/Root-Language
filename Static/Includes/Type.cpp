#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <cassert>

using namespace std;

template<typename Container, typename UnaryPredicate>
Container transform(const Container& container, UnaryPredicate predicate) {
	Container result;
	result.reserve(container.size());
	std::transform(container.begin(), container.end(), back_inserter(result), predicate);
	return result;
}

// ----------------------------------------------------------------

struct Type;

struct ParenthesizedType;
struct NillableType;
struct DefaultType;
struct UnionType;
struct IntersectionType;

struct PredefinedType;
struct PrimitiveType;
struct ArrayType;
struct DictionaryType;
struct CompositeType;
struct ReferenceType;

struct FunctionType;
struct InoutType;
struct VariadicType;

// ----------------------------------------------------------------

using TypeRef = shared_ptr<Type>;

using ParenthesizedTypeRef = shared_ptr<ParenthesizedType>;
using NillableTypeRef = shared_ptr<NillableType>;
using DefaultTypeRef = shared_ptr<DefaultType>;
using UnionTypeRef = shared_ptr<UnionType>;
using IntersectionTypeRef = shared_ptr<IntersectionType>;

using PredefinedTypeRef = shared_ptr<PredefinedType>;
using PrimitiveTypeRef = shared_ptr<PrimitiveType>;
using ArrayTypeRef = shared_ptr<ArrayType>;
using DictionaryTypeRef = shared_ptr<DictionaryType>;
using CompositeTypeRef = shared_ptr<CompositeType>;
using ReferenceTypeRef = shared_ptr<ReferenceType>;

using FunctionTypeRef = shared_ptr<FunctionType>;
using InoutTypeRef = shared_ptr<InoutType>;
using VariadicTypeRef = shared_ptr<VariadicType>;

// ----------------------------------------------------------------

enum class TypeID : uint8_t {
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

enum class PrimitiveTypeID : uint8_t {
	Integer,
	Float,
	String,
	Boolean
};

enum class CompositeTypeID : uint8_t {
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

struct Type : enable_shared_from_this<Type> {
	const TypeID ID;

	Type(TypeID ID = TypeID::Undefined) : ID(ID) { /*cout << "Type created\n";*/ }
	virtual ~Type() { /*cout << "Type destroyed\n";*/ }

	virtual bool acceptsA(const TypeRef& type) { return false; }
	virtual bool conformsTo(const TypeRef& type) { return type->acceptsA(shared_from_this()); }
	virtual bool representedBy(const any& value) const { return false; }
	virtual TypeRef unwrapped() { return shared_from_this(); }
	virtual TypeRef normalized() { return shared_from_this(); }
	virtual string toString() const { return string(); }

	// Now we are planning to normalize all types for one single time before comparison, or using this/similar to this function at worst case, as this is performant-costly operation in C++
	// One thing that also should be mentioned here is that types can't be normalized without losing their initial string representation at the moment
	static bool acceptsANormalized(const TypeRef& left, const TypeRef& right) {
		return left->normalized()->acceptsA(right->normalized());
	}
};

// ----------------------------------------------------------------

struct ParenthesizedType : Type {
	TypeRef innerType;

	ParenthesizedType(TypeRef type) : Type(TypeID::Parenthesized), innerType(move(type)) { cout << "(" << innerType->toString() << ") type created\n"; }
	~ParenthesizedType() { cout << "(" << innerType->toString() << ") type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return innerType->acceptsA(type);
	}

	bool representedBy(const any& value) const override {
		return innerType->representedBy(value);
	}

	TypeRef unwrapped() override {
		return innerType->unwrapped();
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

	NillableType(TypeRef type) : Type(TypeID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
	~NillableType() { cout << innerType->toString() << "? type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return PredefinedEVoidTypeRef->acceptsA(type) ||
			   type->ID == TypeID::Nillable && innerType->acceptsA(static_pointer_cast<NillableType>(type)->innerType) ||
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

		if(normInnerType->ID == TypeID::Nillable) {
			return normInnerType;
		}

		return make_shared<NillableType>(normInnerType);
	}

	string toString() const override {
		return innerType->toString()+"?";
	}
};

struct DefaultType : Type {
	TypeRef innerType;

	DefaultType(TypeRef type) : Type(TypeID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
	~DefaultType() { cout << innerType->toString() << "? type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		return PredefinedEVoidTypeRef->acceptsA(type) ||
			   type->ID == TypeID::Nillable && innerType->acceptsA(static_pointer_cast<DefaultType>(type)->innerType) ||
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

		if(normInnerType->ID == TypeID::Nillable) {
			return normInnerType;
		}

		return make_shared<DefaultType>(normInnerType);
	}

	string toString() const override {
		return innerType->toString()+"!";
	}
};

struct UnionType : Type {
	vector<TypeRef> alternatives;

	UnionType(const vector<TypeRef>& alternatives) : Type(TypeID::Union), alternatives(alternatives) {}

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
		vector<TypeRef> normAlts;

		for(const TypeRef& alt : alternatives) {
			TypeRef normAlt = alt->normalized();

			if(normAlt->ID == TypeID::Union) {
				auto normUnionAlt = static_pointer_cast<UnionType>(normAlt);

				normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
			} else {
				normAlts.push_back(normAlt);
			}
		}

		if(normAlts.size() == 1) {
			return normAlts[0];
		}

		return make_shared<UnionType>(normAlts);
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
	vector<TypeRef> alternatives;

	IntersectionType(const vector<TypeRef>& alternatives) : Type(TypeID::Intersection), alternatives(alternatives) {}

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
		vector<TypeRef> normAlts;

		for(const TypeRef& alt : alternatives) {
			TypeRef normAlt = alt->normalized();

			if(normAlt->ID == TypeID::Intersection) {
				auto normUnionAlt = static_pointer_cast<IntersectionType>(normAlt);

				normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
			} else {
				normAlts.push_back(normAlt);
			}
		}

		if(normAlts.size() == 1) {
			return normAlts[0];
		}

		return make_shared<IntersectionType>(normAlts);
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

	PredefinedType(PredefinedTypeID subID, function<bool(const TypeRef&)> acceptsFn) : Type(TypeID::Predefined), subID(subID), acceptsFn(acceptsFn) {}

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

struct PrimitiveType : Type {
	const PrimitiveTypeID subID;

	PrimitiveType(PrimitiveTypeID subID) : Type(TypeID::Primitive), subID(subID) { cout << toString() << " type created\n"; }
	~PrimitiveType() { cout << toString() << " type destroyed\n"; }

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypeID::Predefined) {
			switch(static_pointer_cast<PredefinedType>(type)->subID) {
				case PredefinedTypeID::PInteger:	return subID == PrimitiveTypeID::Integer;
				case PredefinedTypeID::PFloat:		return subID == PrimitiveTypeID::Float;
				case PredefinedTypeID::PString:		return subID == PrimitiveTypeID::String;
				case PredefinedTypeID::PBoolean:	return subID == PrimitiveTypeID::Boolean;
			}
		}
		if(type->ID == TypeID::Primitive) {
			return subID == static_pointer_cast<PrimitiveType>(type)->subID;
		}

		return false;
	}

	bool representedBy(const any& value) const override {
		try {
			switch(subID) {
				case PrimitiveTypeID::Integer:	return value.type() == typeid(int);
				case PrimitiveTypeID::Float:	return value.type() == typeid(float) ||
													   value.type() == typeid(double);
				case PrimitiveTypeID::String:	return value.type() == typeid(string);
				case PrimitiveTypeID::Boolean:	return value.type() == typeid(bool);
			}
		} catch(const bad_any_cast&) {
			return false;
		}

		return false;
	}

	string toString() const override {
		switch(subID) {
			case PrimitiveTypeID::Integer:	return "int";
			case PrimitiveTypeID::Float:	return "float";
			case PrimitiveTypeID::String:	return "string";
			case PrimitiveTypeID::Boolean:	return "bool";
		}

		return string();
	}
};

struct ArrayType : Type {
	TypeRef valueType;

	ArrayType(const TypeRef& valueType) : Type(TypeID::Array), valueType(valueType) {}

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::PDictionary) {
			return true;
		}
		if(type->ID == TypeID::Array) {
			auto arrayType = static_pointer_cast<ArrayType>(type);

			return valueType->acceptsA(arrayType->valueType);
		}

		return false;
	}

	TypeRef normalized() override {
		return make_shared<ArrayType>(valueType->normalized());
	}

	string toString() const override {
		return "["+valueType->toString()+"]";
	}
};

struct DictionaryType : Type {
	TypeRef keyType,
			valueType;

	DictionaryType(const TypeRef& keyType, const TypeRef& valueType) : Type(TypeID::Dictionary), keyType(keyType), valueType(valueType) {}

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::PDictionary) {
			return true;
		}
		if(type->ID == TypeID::Dictionary) {
			auto dictType = static_pointer_cast<DictionaryType>(type);

			return keyType->acceptsA(dictType->keyType) && valueType->acceptsA(dictType->valueType);
		}

		return false;
	}

	TypeRef normalized() override {
		return make_shared<DictionaryType>(keyType->normalized(), valueType->normalized());
	}

	string toString() const override {
		return "["+keyType->toString()+": "+valueType->toString()+"]";
	}
};

vector<CompositeTypeRef> composites;

struct CompositeType : Type {
	int index;
	const CompositeTypeID subID;
	string title;
	vector<int> inherits;
	vector<TypeRef> generics;

	CompositeType(CompositeTypeID subID,
				  const string& title,
				  const vector<int>& inherits = {},
				  const vector<TypeRef>& generics = {}) : Type(TypeID::Composite),
														  index(composites.size()),
														  subID(subID),
														  title(title),
														  inherits(inherits),
														  generics(generics)
	{
		composites.push_back(static_pointer_cast<CompositeType>(shared_from_this()));
	}

	set<int> getFullInheritanceChain() const {
		auto chain = set<int>(inherits.begin(), inherits.end());

		for(int parentIndex : inherits) {
			set<int> parentChain = composites[parentIndex]->getFullInheritanceChain();

			chain.insert(parentChain.begin(), parentChain.end());
		}

		return chain;
	}

	static bool checkConformance(const CompositeTypeRef& base, const CompositeTypeRef& candidate, const optional<vector<TypeRef>>& candidateArgs = {}) {
		if(candidate->index != base->index && !candidate->getFullInheritanceChain().contains(base->index)) {
			return false;
		}

		if(candidateArgs) {
			if(candidate->generics.size() != candidateArgs->size()) {
				return false;
			}
			for(int i = 0; i < candidate->generics.size(); i++) {
				if(!candidate->generics[i]->acceptsA((*candidateArgs)[i])) {
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
			case CompositeTypeID::Class:		result += "class";
			case CompositeTypeID::Enumeration:	result += "enum";
			case CompositeTypeID::Function:		result += "func";
			case CompositeTypeID::Namespace:	result += "namespace";
			case CompositeTypeID::Object:		result += "object";
			case CompositeTypeID::Protocol:		result += "protocol";
			case CompositeTypeID::Structure:	result += "structure";
		}

		result += " "+title+"#"+to_string(index);

		if(!generics.empty()) {
			result += "<";

			for(int i = 0; i < generics.size(); i++) {
				if(i > 0) {
					result += ", ";
				}

				result += generics[i]->toString();
			}

			result += ">";
		}

		if(!inherits.empty()) {
			set<int> chain = getFullInheritanceChain();
			bool first = true;

			result += " inherits [";

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
	CompositeTypeRef compType;
	optional<vector<TypeRef>> typeArgs;

	ReferenceType(const CompositeTypeRef& compType, const optional<vector<TypeRef>>& typeArgs = nullopt) : Type(TypeID::Reference), compType(compType), typeArgs(typeArgs) {}

	bool acceptsA(const TypeRef& type) override {
		if(type->ID == TypeID::Predefined) {
			return compType->acceptsA(type);
		}
		if(type->ID == TypeID::Composite) {
			auto compType = static_pointer_cast<CompositeType>(type);

			return CompositeType::checkConformance(compType, compType, typeArgs);
		}
		if(type->ID == TypeID::Reference) {
			auto refType = static_pointer_cast<ReferenceType>(type);

			return CompositeType::checkConformance(compType, refType->compType, refType->typeArgs);
		}

		return false;
	}

	TypeRef normalized() override {
		if(!typeArgs) {
			return shared_from_this();
		}

		vector<TypeRef> normArgs;

		for(const TypeRef& arg : *typeArgs) {
			normArgs.push_back(arg->normalized());
		}

		return make_shared<ReferenceType>(compType, normArgs);
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

bool CompositeType::acceptsA(const TypeRef& type) {
	if(type->ID == TypeID::Predefined) {
		switch(static_pointer_cast<PredefinedType>(type)->subID) {
			case PredefinedTypeID::CAny:			return true;
			case PredefinedTypeID::CClass:			return subID == CompositeTypeID::Class;
			case PredefinedTypeID::CEnumeration:	return subID == CompositeTypeID::Enumeration;
			case PredefinedTypeID::CFunction:		return subID == CompositeTypeID::Function;
			case PredefinedTypeID::CNamespace:		return subID == CompositeTypeID::Namespace;
			case PredefinedTypeID::CObject:			return subID == CompositeTypeID::Object;
			case PredefinedTypeID::CProtocol:		return subID == CompositeTypeID::Protocol;
			case PredefinedTypeID::CStructure:		return subID == CompositeTypeID::Structure;
		}
	}
	if(type->ID == TypeID::Composite) {
		auto compThis = static_pointer_cast<CompositeType>(shared_from_this());
		auto compType = static_pointer_cast<CompositeType>(type);

		return CompositeType::checkConformance(compThis, compType);
	}
	if(type->ID == TypeID::Reference) {
		auto compThis = static_pointer_cast<CompositeType>(shared_from_this());
		auto refType = static_pointer_cast<ReferenceType>(type);

		return CompositeType::checkConformance(compThis, refType->compType, refType->typeArgs);
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
		if(type->ID == TypeID::Function) {
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
		return make_shared<FunctionType>(
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

	InoutType(const TypeRef& innerType) : Type(TypeID::Inout), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) override {
		return type->ID == TypeID::Inout && innerType->acceptsA(static_pointer_cast<InoutType>(type)->innerType);
	}

	bool representedBy(const any& value) const override {
		return innerType->representedBy(value);
	}

	TypeRef normalized() override {
		auto normInner = innerType->normalized();

		if(normInner->ID == TypeID::Inout) {
			return normInner;
		}

		return make_shared<InoutType>(normInner);
	}

	string toString() const override {
		return "inout "+innerType->toString();
	}
};

struct VariadicType : Type {
	TypeRef innerType;

	VariadicType(const TypeRef& innerType = nullptr) : Type(TypeID::Variadic), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) override {
		if(!innerType) {
			return true;
		}
		if(type->ID != TypeID::Variadic) {
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
			return shared_from_this();
		}

		return make_shared<VariadicType>(innerType->normalized());
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

		if(expectedType->ID == TypeID::Variadic) {
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

const TypeRef PredefinedEVoidTypeRef = make_shared<PredefinedType>(PredefinedTypeID::EVoid, [](const TypeRef& type) {
	return type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::EVoid;
});

const TypeRef PredefinedEAnyTypeRef = make_shared<PredefinedType>(PredefinedTypeID::EAny, [](const TypeRef& type) {
	return true;
});

const TypeRef PredefinedPAnyTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PAny, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive;
});

const TypeRef PredefinedPBooleanTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PBoolean, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Boolean;
});

const TypeRef PredefinedPFloatTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PFloat, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Float;
});

const TypeRef PredefinedPIntegerTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PInteger, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Integer;
});

const TypeRef PredefinedPStringTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PString, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::String;
});

const TypeRef PredefinedPTypeTypeRef = make_shared<PredefinedType>(PredefinedTypeID::PType, [](const TypeRef& type) {
	return type->ID != TypeID::Undefined;
});  // TODO: Values check

const TypeRef PredefinedCAnyTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CAny, [](const TypeRef& type) {
	return type->ID == TypeID::Composite ||
		   type->ID == TypeID::Reference;
});

const TypeRef PredefinedCClassTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CClass, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Class ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Class;
});

const TypeRef PredefinedCEnumerationTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CEnumeration, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Enumeration ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Enumeration;
});

const TypeRef PredefinedCFunctionTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CFunction, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Function ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Function;
});

const TypeRef PredefinedCNamespaceTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CNamespace, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Namespace ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Namespace;
});

const TypeRef PredefinedCObjectTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CObject, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Object ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Object;
});

const TypeRef PredefinedCProtocolTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CProtocol, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Protocol ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Protocol;
});

const TypeRef PredefinedCStructureTypeRef = make_shared<PredefinedType>(PredefinedTypeID::CStructure, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Structure ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Structure;
});

// ----------------------------------------------------------------

/*
void matchTypeListsTest() {
	// Примитивные типы
	TypeRef intType	= make_shared<PrimitiveType>(PrimitiveTypeID::Integer);
	TypeRef floatType  = make_shared<PrimitiveType>(PrimitiveTypeID::Float);
	TypeRef stringType = make_shared<PrimitiveType>(PrimitiveTypeID::String);
	TypeRef boolType   = make_shared<PrimitiveType>(PrimitiveTypeID::Boolean);

	// Variadic с внутренним типом
	TypeRef variadicInt	= make_shared<VariadicType>(intType);
	TypeRef variadicString = make_shared<VariadicType>(stringType);

	// Variadic без внутреннего типа (принимает любой тип)
	TypeRef variadicAny = make_shared<VariadicType>();

	// Тест 1: Точное совпадение без вариативных типов.
	vector<TypeRef> expected_1 = { intType, floatType, stringType, boolType },
					provided_1 = { intType, floatType, stringType, boolType };
	assert(FunctionType::matchTypeLists(expected_1, provided_1));

	// Тест 2: Несовпадение списков (неправильный порядок).
	vector<TypeRef> expected_2 = { intType, floatType, stringType },
					provided_2 = { intType, stringType, floatType };
	assert(!FunctionType::matchTypeLists(expected_2, provided_2));

	// Тест 3: Variadic как последний элемент с пустым списком provided.
	vector<TypeRef> expected_3 = { variadicInt },
					provided_3 = {};
	assert(FunctionType::matchTypeLists(expected_3, provided_3));

	// Тест 4: Variadic как последний элемент с несколькими элементами.
	vector<TypeRef> expected_4 = { variadicInt },
					provided_4 = { intType, intType, intType };
	assert(FunctionType::matchTypeLists(expected_4, provided_4));

	// Тест 5: Variadic в середине, покрывающий 0 элементов.
	vector<TypeRef> expected_5 = { intType, variadicInt, stringType },
					provided_5 = { intType, stringType };
	assert(FunctionType::matchTypeLists(expected_5, provided_5));

	// Тест 6: Variadic в середине, покрывающий 1 элемент.
	vector<TypeRef> expected_6 = { intType, variadicInt, stringType },
					provided_6 = { intType, intType, stringType };
	assert(FunctionType::matchTypeLists(expected_6, provided_6));

	// Тест 7: Variadic в середине, покрывающий несколько элементов.
	vector<TypeRef> expected_7 = { intType, variadicInt, stringType },
					provided_7 = { intType, intType, intType, stringType };
	assert(FunctionType::matchTypeLists(expected_7, provided_7));

	// Тест 8: Variadic не соответствует (в Variadic ожидается int, а получен float).
	vector<TypeRef> expected_8 = { intType, variadicInt },
					provided_8 = { intType, floatType };
	assert(!FunctionType::matchTypeLists(expected_8, provided_8));

	// Тест 9: Несколько Variadic подряд с внутренними типами.
	vector<TypeRef> expected_9 = { variadicInt, variadicString },
					provided_9 = { intType, intType, stringType, stringType };
	assert(FunctionType::matchTypeLists(expected_9, provided_9));

	// Тест 10: Variadic без внутреннего типа (variadicAny) как последний элемент, принимает любые типы.
	vector<TypeRef> expected_10 = { variadicAny },
					provided_10 = { intType, floatType, stringType, boolType };
	assert(FunctionType::matchTypeLists(expected_10, provided_10));

	// Тест 11: Variadic без внутреннего типа (variadicAny) в середине.
	vector<TypeRef> expected_11 = { intType, variadicAny, stringType };
	// Здесь variadicAny может покрыть несколько любых типов.
	vector<TypeRef> provided_11 = { intType, boolType, floatType, stringType };
	assert(FunctionType::matchTypeLists(expected_11, provided_11));

	// Тест 12: Variadic без внутреннего типа (variadicAny) в середине, покрывающий 0 элементов.
	vector<TypeRef> expected_12 = { intType, variadicAny, stringType },
					provided_12 = { intType, stringType };
	assert(FunctionType::matchTypeLists(expected_12, provided_12));

	// Тест 13: Variadic в середине, где первый элемент для вариативного типа не соответствует.
	// Здесь variadicInt (ожидает int) не принимает floatType, поэтому цикл должен прерваться.
	vector<TypeRef> expected_13 = { intType, variadicInt, stringType },
					provided_13 = { intType, floatType, stringType };
	assert(!FunctionType::matchTypeLists(expected_13, provided_13));

	// Тест 14: Variadic в середине, где break происходит не сразу, а после одного успешного совпадения.
	// Порядок обработки:
	// - Сначала проверяется пустой диапазон для Variadic — не подходит.
	// - Затем для currentIndex = 1: intType принимается.
	// - Для currentIndex = 2: floatType не принимается variadicInt, цикл прерывается, сопоставление не удаётся.
	vector<TypeRef> expected_14 = { intType, variadicInt, stringType },
					provided_14 = { intType, intType, floatType, stringType };
	assert(!FunctionType::matchTypeLists(expected_14, provided_14));
}

int main() {
	TypeRef intType	= make_shared<PrimitiveType>(PrimitiveTypeID::Integer);
	TypeRef nillableInt = make_shared<NillableType>(intType);
	TypeRef parenType = make_shared<ParenthesizedType>(nillableInt);
	TypeRef nillableParenType = make_shared<NillableType>(parenType);

	cout << "int accepts int: " << intType->acceptsA(intType) << "\n";
	cout << "int? accepts int: " << nillableInt->acceptsA(intType) << "\n";
	cout << "(int?) accepts int: " << parenType->acceptsA(intType) << "\n";
	cout << "(int?)? normalized: " << nillableParenType->normalized()->toString() << "\n";
	cout << "(int?)? unwrapped: " << nillableParenType->unwrapped()->toString() << "\n";

	matchTypeListsTest();
}
*/
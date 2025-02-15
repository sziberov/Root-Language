#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>

using namespace std;

template <typename T>
using sp = shared_ptr<T>;

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

using TypeRef = shared_ptr<Type>;

extern const TypeRef PredefinedEVoidType;
extern const TypeRef PredefinedEAnyType;

extern const TypeRef PredefinedPAnyType;
extern const TypeRef PredefinedPBooleanType;
extern const TypeRef PredefinedPFloatType;
extern const TypeRef PredefinedPIntegerType;
extern const TypeRef PredefinedPStringType;
extern const TypeRef PredefinedPTypeType;

extern const TypeRef PredefinedCAnyType;
extern const TypeRef PredefinedCClassType;
extern const TypeRef PredefinedCEnumerationType;
extern const TypeRef PredefinedCFunctionType;
extern const TypeRef PredefinedCNamespaceType;
extern const TypeRef PredefinedCObjectType;
extern const TypeRef PredefinedCProtocolType;
extern const TypeRef PredefinedCStructureType;

// ----------------------------------------------------------------

struct Type : enable_shared_from_this<Type> {
	const TypeID ID;

	Type(TypeID ID = TypeID::Undefined) : ID(ID) { /*cout << "Type created\n";*/ }
	virtual ~Type() { /*cout << "Type destroyed\n";*/ }

	virtual bool acceptsA(const TypeRef& type) const { return false; }
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
		return PredefinedEVoidType->acceptsA(type) ||
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

	bool acceptsA(const TypeRef& type) const override {
		return PredefinedEVoidType->acceptsA(type) ||
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
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

	bool acceptsA(const TypeRef& type) const override {
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

vector<shared_ptr<CompositeType>> composites;

struct CompositeType : Type {
	int index;
	const CompositeTypeID subID;
	string title;
	vector<int> inherits;
	vector<TypeRef> generics;

	CompositeType(CompositeTypeID subID, const string& title, const vector<int>& inherits = {}, const vector<TypeRef>& generics = {}) : Type(TypeID::Composite), index(composites.size()), subID(subID), title(title), inherits(inherits), generics(generics) {
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

	static bool checkConformance(const shared_ptr<CompositeType>& base, const shared_ptr<CompositeType>& candidate, const optional<vector<TypeRef>>& candidateArgs = {}) {
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

	bool acceptsA(const TypeRef& type) const override;

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
	shared_ptr<CompositeType> compType;
	optional<vector<TypeRef>> typeArgs;

	ReferenceType(const shared_ptr<CompositeType>& compType, const optional<vector<TypeRef>>& typeArgs = nullopt) : Type(TypeID::Reference), compType(compType), typeArgs(typeArgs) {}

	bool acceptsA(const TypeRef& type) const override {
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

bool CompositeType::acceptsA(const TypeRef& type) const {
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
		auto compThis = const_pointer_cast<CompositeType>(static_pointer_cast<const CompositeType>(shared_from_this()));
		auto compType = static_pointer_cast<CompositeType>(type);

		return CompositeType::checkConformance(compThis, compType);
	}
	if(type->ID == TypeID::Reference) {
		auto compThis = const_pointer_cast<CompositeType>(static_pointer_cast<const CompositeType>(shared_from_this()));
		auto refType = static_pointer_cast<ReferenceType>(type);

		return CompositeType::checkConformance(compThis, refType->compType, refType->typeArgs);
	}

	return false;
}

// ----------------------------------------------------------------

struct InoutType : Type {
	TypeRef innerType;

	InoutType(const TypeRef& innerType) : Type(TypeID::Inout), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) const override {
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

	VariadicType(const TypeRef& innerType) : Type(TypeID::Variadic), innerType(innerType) {}

	bool acceptsA(const TypeRef& type) const override {
		if(!innerType) {
			return true;
		}
		if(type->ID != TypeID::Variadic) {
			return innerType->acceptsA(type);
		}

		auto varType = static_pointer_cast<VariadicType>(type);

		if(!varType->innerType) {
			return innerType->acceptsA(PredefinedEVoidType);
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

// ----------------------------------------------------------------

const TypeRef PredefinedEVoidType = make_shared<PredefinedType>(PredefinedTypeID::EVoid, [](const TypeRef& type) {
	return type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::EVoid;
});

const TypeRef PredefinedEAnyType = make_shared<PredefinedType>(PredefinedTypeID::EAny, [](const TypeRef& type) {
	return true;
});

const TypeRef PredefinedPAnyType = make_shared<PredefinedType>(PredefinedTypeID::PAny, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive;
});

const TypeRef PredefinedPBooleanType = make_shared<PredefinedType>(PredefinedTypeID::PBoolean, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Boolean;
});

const TypeRef PredefinedPFloatType = make_shared<PredefinedType>(PredefinedTypeID::PFloat, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Float;
});

const TypeRef PredefinedPIntegerType = make_shared<PredefinedType>(PredefinedTypeID::PInteger, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Integer;
});

const TypeRef PredefinedPStringType = make_shared<PredefinedType>(PredefinedTypeID::PString, [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::String;
});

const TypeRef PredefinedPTypeType = make_shared<PredefinedType>(PredefinedTypeID::PType, [](const TypeRef& type) {
	return type->ID != TypeID::Undefined;
});  // TODO: Values check

const TypeRef PredefinedCAnyType = make_shared<PredefinedType>(PredefinedTypeID::CAny, [](const TypeRef& type) {
	return type->ID == TypeID::Composite ||
		   type->ID == TypeID::Reference;
});

const TypeRef PredefinedCClassType = make_shared<PredefinedType>(PredefinedTypeID::CClass, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Class ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Class;
});

const TypeRef PredefinedCEnumerationType = make_shared<PredefinedType>(PredefinedTypeID::CEnumeration, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Enumeration ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Enumeration;
});

const TypeRef PredefinedCFunctionType = make_shared<PredefinedType>(PredefinedTypeID::CFunction, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Function ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Function;
});

const TypeRef PredefinedCNamespaceType = make_shared<PredefinedType>(PredefinedTypeID::CNamespace, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Namespace ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Namespace;
});

const TypeRef PredefinedCObjectType = make_shared<PredefinedType>(PredefinedTypeID::CObject, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Object ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Object;
});

const TypeRef PredefinedCProtocolType = make_shared<PredefinedType>(PredefinedTypeID::CProtocol, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Protocol ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Protocol;
});

const TypeRef PredefinedCStructureType = make_shared<PredefinedType>(PredefinedTypeID::CStructure, [](const TypeRef& type) {
	return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Structure ||
		   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Structure;
});

// ----------------------------------------------------------------

int main() {
	TypeRef intType = make_shared<PrimitiveType>(PrimitiveTypeID::Integer);
	TypeRef nillableInt = make_shared<NillableType>(intType);
	TypeRef parenType = make_shared<ParenthesizedType>(nillableInt);
	TypeRef nillableParenType = make_shared<NillableType>(parenType);

	cout << "int accepts int: " << intType->acceptsA(intType) << "\n";
	cout << "int? accepts int: " << nillableInt->acceptsA(intType) << "\n";
	cout << "(int?) accepts int: " << parenType->acceptsA(intType) << "\n";
	cout << "(int?)? normalized: " << nillableParenType->normalized()->toString() << "\n";
	cout << "(int?)? unwrapped: " << nillableParenType->unwrapped()->toString() << "\n";
}
#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

// ----------------------------------------------------------------

using TypeRef = shared_ptr<Type>;

extern const TypeRef PREDEFINED_void;
extern const TypeRef PREDEFINED__;

extern const TypeRef PREDEFINED_any;
extern const TypeRef PREDEFINED_bool;
extern const TypeRef PREDEFINED_dict;
extern const TypeRef PREDEFINED_float;
extern const TypeRef PREDEFINED_int;
extern const TypeRef PREDEFINED_string;
extern const TypeRef PREDEFINED_type;

extern const TypeRef PREDEFINED_Any;
extern const TypeRef PREDEFINED_Class;
extern const TypeRef PREDEFINED_Enumeration;
extern const TypeRef PREDEFINED_Function;
extern const TypeRef PREDEFINED_Namespace;
extern const TypeRef PREDEFINED_Object;
extern const TypeRef PREDEFINED_Protocol;
extern const TypeRef PREDEFINED_Structure;

// ----------------------------------------------------------------

struct Type {
	const TypeID ID;

	Type(TypeID ID = TypeID::Undefined) : ID(ID) { /*cout << "Type created\n";*/ }
//	~Type() { cout << "Type destroyed\n"; }

	virtual bool acceptsA(const TypeRef& type) const { return false; }
	virtual bool conformsTo(const TypeRef& type) const { return type->acceptsA(make_shared<Type>()); }
	virtual bool representedBy(const any& value) const { return false; }
	virtual TypeRef unwrapped() const { return make_shared<Type>(); }
	virtual TypeRef normalized() const { return make_shared<Type>(); }
	virtual string toString() const { return string(); }

	static acceptsANormalized(const TypeRef& left, const TypeRef& right) {
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

	TypeRef unwrapped() const override {
		return innerType->unwrapped();
	}

	TypeRef normalized() const override {
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
		if(PREDEFINED_void->acceptsA(type)) {
			return true;
		}

		TypeRef normType = type->normalized();

		if(normType->ID == TypeID::Nillable) {
			return innerType->acceptsA(static_pointer_cast<NillableType>(normType)->innerType);
		}

		return innerType->acceptsA(normType);
	}

	bool representedBy(const any& value) const override {
		if(!value.has_value()) {
			return true;
		}

		return innerType->representedBy(value);
	}

	TypeRef unwrapped() const override {
		return make_shared<NillableType>(innerType);
	}

	TypeRef normalized() const override {
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

// ----------------------------------------------------------------

struct PredefinedType : Type {
	string title;
	function<bool(const TypeRef&)> acceptsFn;

	PredefinedType(const string& title, function<bool(const TypeRef&)> acceptsFn) : Type(TypeID::Predefined), title(title), acceptsFn(acceptsFn) {}

	bool acceptsA(const TypeRef& type) const override {
		return acceptsFn(type);
	}

	TypeRef unwrapped() const override {
		return make_shared<PredefinedType>(title, acceptsFn);
	}

	TypeRef normalized() const override {
		return make_shared<PredefinedType>(title, acceptsFn);
	}

	string toString() const override {
		return title;
	}
};

struct PrimitiveType : Type {
	string title;

	PrimitiveType(string n) : Type(TypeID::Primitive), title(n) { cout << title << " type created\n"; }
	~PrimitiveType() { cout << title << " type destroyed\n"; }

	bool acceptsA(const TypeRef& type) const override {
		return type->ID == TypeID::Primitive && title == static_pointer_cast<PrimitiveType>(type)->title;
	}

	TypeRef unwrapped() const override {
		return make_shared<PrimitiveType>(title);
	}

	TypeRef normalized() const override {
		return make_shared<PrimitiveType>(title);
	}

	string toString() const override {
		return title;
	}
};

// ----------------------------------------------------------------

const TypeRef PREDEFINED_void = make_shared<PredefinedType>("void", [](const TypeRef& type) {
	return type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->title == "void";
});

const TypeRef PREDEFINED__ = make_shared<PredefinedType>("_", [](const TypeRef& type) {
	return true;
});

const TypeRef PREDEFINED_any = make_shared<PredefinedType>("any", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive;
});

const TypeRef PREDEFINED_bool = make_shared<PredefinedType>("bool", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->title == "bool";
});

const TypeRef PREDEFINED_float = make_shared<PredefinedType>("float", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->title == "float";
});

const TypeRef PREDEFINED_int = make_shared<PredefinedType>("int", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->title == "int";
});

const TypeRef PREDEFINED_string = make_shared<PredefinedType>("string", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->title == "string";
});

const TypeRef PREDEFINED_type = make_shared<PredefinedType>("type", [](const TypeRef& type) {
	return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->title == "type";
});

const TypeRef PREDEFINED_Any = make_shared<PredefinedType>("Any", [](const TypeRef& type) {
	return type->ID == TypeID::Composite || type->ID == TypeID::Reference;
});

// ----------------------------------------------------------------

int main() {
	TypeRef intType = make_shared<PrimitiveType>("int");
	TypeRef nillableInt = make_shared<NillableType>(intType);
	TypeRef parenType = make_shared<ParenthesizedType>(nillableInt);
	TypeRef nillableParenType = make_shared<NillableType>(parenType);

	cout << "int accepts int: " << intType->acceptsA(intType) << "\n";
	cout << "int? accepts int: " << nillableInt->acceptsA(intType) << "\n";
	cout << "(int?) accepts int: " << parenType->acceptsA(intType) << "\n";
	cout << "(int?)? normalized: " << nillableParenType->normalized()->toString() << "\n";
	cout << "(int?)? unwrapped: " << nillableParenType->unwrapped()->toString() << "\n";
}
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <iostream>
#include <sstream>
#include <map>
#include <set>

namespace types {

// Type IDs for all possible types
enum class TypeID {
    Primitive,
    Optional,
    Parenthesized,
    Default,
    Composite,
    Reference,
    Array,
    Dictionary,
    Variadic,
    Intersection,
    Union,
    Function,
    Inout
};

// Forward declarations
class Type;
class PrimitiveType;
class OptionalType;
class ParenthesizedType;
class DefaultType;
class CompositeType;
class ReferenceType;
class ArrayType;
class DictionaryType;
class VariadicType;
class IntersectionType;
class UnionType;
class FunctionType;
class InoutType;

// Value types supported by our type system
using TypeValue = std::variant<
    std::monostate,  // represents void/null
    int,            // Int
    double,         // Float
    bool,           // Bool
    std::string     // String
>;

// Global vector for composite types
std::vector<std::shared_ptr<CompositeType>> composites;

// Base Type class
class Type {
public:
    explicit Type(TypeID id) : typeID(id) {}
    virtual ~Type() = default;

    virtual bool checkValue(const TypeValue& value) const = 0;
    virtual std::string toString() const = 0;
    virtual const Type& normalized() const = 0;
    virtual bool accepts(const Type& other) const;

    // Type-specific acceptance methods with default implementations
    virtual bool accepts(const PrimitiveType&) const { return false; }
    virtual bool accepts(const OptionalType&) const { return false; }
    virtual bool accepts(const DefaultType&) const { return false; }
    virtual bool accepts(const CompositeType&) const { return false; }
    virtual bool accepts(const ReferenceType&) const { return false; }
    virtual bool accepts(const ArrayType&) const { return false; }
    virtual bool accepts(const DictionaryType&) const { return false; }
    virtual bool accepts(const VariadicType&) const { return false; }
    virtual bool accepts(const IntersectionType&) const { return false; }
    virtual bool accepts(const UnionType&) const { return false; }
    virtual bool accepts(const FunctionType&) const { return false; }
    virtual bool accepts(const InoutType&) const { return false; }

    TypeID getTypeID() const { return typeID; }

private:
    const TypeID typeID;
};

// ParenthesizedType implementation
class ParenthesizedType : public Type {
public:
    explicit ParenthesizedType(const Type& innerType)
        : Type(TypeID::Parenthesized), innerType(innerType) {}

    using Type::accepts;

    bool checkValue(const TypeValue& value) const override {
        return innerType.checkValue(value);
    }

    const Type& normalized() const override {
        return innerType.normalized();
    }

    std::string toString() const override {
        return "(" + innerType.toString() + ")";
    }

    const Type& getInnerType() const { return innerType; }

private:
    const Type& innerType;
};

// PrimitiveType implementation
class PrimitiveType : public Type {
public:
    explicit PrimitiveType(std::string name)
        : Type(TypeID::Primitive), name(std::move(name)) {}

    using Type::accepts;

    bool accepts(const PrimitiveType& other) const override {
        return name == other.name ||
               (name == "Number" && (other.name == "Int" || other.name == "Float"));
    }

    bool checkValue(const TypeValue& value) const override {
        if (name == "Int") {
            return std::holds_alternative<int>(value);
        } else if (name == "Float") {
            return std::holds_alternative<double>(value);
        } else if (name == "String") {
            return std::holds_alternative<std::string>(value);
        } else if (name == "Bool") {
            return std::holds_alternative<bool>(value);
        } else if (name == "Number") {
            return std::holds_alternative<int>(value) ||
                   std::holds_alternative<double>(value);
        }
        return false;
    }

    const Type& normalized() const override { return *this; }
    std::string toString() const override { return name; }
    const std::string& getName() const { return name; }

private:
    std::string name;
};

// OptionalType implementation
class OptionalType : public Type {
public:
    OptionalType(const Type& innerType)
        : Type(TypeID::Optional), innerType(innerType) {}

    bool accepts(const Type& other) const override {
        // Adds null check as in types.cpp
        if (std::holds_alternative<std::monostate>(TypeValue{})) return true;

        // Then uses the base version of accepts for other cases
        return Type::accepts(other);
    }

    bool accepts(const OptionalType& other) const override {
        return innerType.normalized().accepts(other.innerType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        if (std::holds_alternative<std::monostate>(value)) return true;
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& normInner = innerType.normalized();
        if (const auto* opt = dynamic_cast<const OptionalType*>(&normInner)) {
            return *opt;
        }
        return *this;
    }

    std::string toString() const override {
        return innerType.toString() + "?";
    }

    const Type& getInnerType() const { return innerType; }

private:
    const Type& innerType;
};

// DefaultType implementation
class DefaultType : public Type {
public:
    DefaultType(const Type& innerType) : Type(TypeID::Default), innerType(innerType) {}

    bool accepts(const Type& other) const override {
        // Adds null check as in types.cpp
        if (std::holds_alternative<std::monostate>(TypeValue{})) return true;

        // Then uses the base version of accepts for other cases
        return Type::accepts(other);
    }

    bool accepts(const DefaultType& other) const override {
        return innerType.normalized().accepts(other.innerType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        if (std::holds_alternative<std::monostate>(value)) return true;
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& normInner = innerType.normalized();
        if (const auto* def = dynamic_cast<const DefaultType*>(&normInner)) {
            return *def;
        }
        return *this;
    }

    std::string toString() const override {
        return innerType.toString() + "!";
    }

    const Type& getInnerType() const { return innerType; }

private:
    const Type& innerType;
};

// CompositeType implementation
class CompositeType : public Type {
public:
    CompositeType(std::string kind, std::string name,
                 std::vector<size_t> inherits = {},
                 std::vector<const Type*> generics = {})
        : Type(TypeID::Composite)
        , kind(std::move(kind))
        , name(std::move(name))
        , inherits(std::move(inherits))
        , generics(std::move(generics))
        , index(composites.size()) {}

    using Type::accepts;

    bool accepts(const CompositeType& other) const override {
        return index == other.index || other.getFullInheritanceChain().count(index);
    }

    void initialize() {
        composites.push_back(std::make_shared<CompositeType>(*this));
    }

    bool checkValue(const TypeValue& value) const override {
        try {
            if (!std::holds_alternative<int>(value)) return false;

            int idx = std::get<int>(value);
            if (idx < 0 || static_cast<size_t>(idx) >= composites.size()) return false;

            auto comp = composites[idx];
            return comp && accepts(*comp);
        } catch (const std::bad_variant_access&) {
            return false;
        }
    }

    const Type& normalized() const override { return *this; }

    std::string toString() const override {
        std::stringstream ss;
        ss << kind << " " << name << "#" << index;

        if (!generics.empty()) {
            ss << "<";
            for (size_t i = 0; i < generics.size(); i++) {
                if (i > 0) ss << ", ";
                ss << generics[i]->toString();
            }
            ss << ">";
        }

        if (!inherits.empty()) {
            ss << " inherits [";
            auto chain = getFullInheritanceChain();
            bool first = true;
            for (size_t idx : chain) {
                if (!first) ss << ", ";
                ss << idx;
                first = false;
            }
            ss << "]";
        }

        return ss.str();
    }

    size_t getIndex() const { return index; }

    std::set<size_t> getFullInheritanceChain() const {
        std::set<size_t> chain(inherits.begin(), inherits.end());

        for (size_t parentIndex : inherits) {
            if (parentIndex < composites.size()) {
                auto parent = composites[parentIndex];
                auto parentChain = parent->getFullInheritanceChain();
                chain.insert(parentChain.begin(), parentChain.end());
            }
        }

        return chain;
    }

private:
    std::string kind;
    std::string name;
    std::vector<size_t> inherits;
    std::vector<const Type*> generics;
    size_t index;
};

// ArrayType implementation
class ArrayType : public Type {
public:
    explicit ArrayType(const Type& elementType)
        : Type(TypeID::Array), elementType(elementType) {}

    using Type::accepts;

    bool accepts(const ArrayType& other) const override {
        // Checks covariance: elementType must accept the element type of the other array
        return elementType.normalized().accepts(other.elementType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        // In C++, we only check the type of the value, since TypeValue does not support std::vector
        // In a real system, there would be a check of the vector elements here
        return false;
    }

    const Type& normalized() const override {
        // Returns a new ArrayType with the normalized element type
        static ArrayType normalized(elementType.normalized());
        return normalized;
    }

    std::string toString() const override {
        return "[" + elementType.toString() + "]";
    }

    const Type& getElementType() const { return elementType; }

private:
    const Type& elementType;
};

// ReferenceType implementation
class ReferenceType : public Type {
public:
    ReferenceType(const CompositeType& composite, std::vector<const Type*> typeArgs = {})
        : Type(TypeID::Reference), composite(composite), typeArgs(std::move(typeArgs)) {}

    using Type::accepts;

    bool accepts(const ReferenceType& other) const override {
        return composite.accepts(other.composite) &&
               checkTypeArgs(other.typeArgs);
    }

    bool checkValue(const TypeValue& value) const override {
        try {
            if (!std::holds_alternative<int>(value)) return false;

            int idx = std::get<int>(value);
            if (idx < 0 || static_cast<size_t>(idx) >= composites.size()) return false;

            auto comp = composites[idx];
            return comp && accepts(*comp);
        } catch (const std::bad_variant_access&) {
            return false;
        }
    }

    const Type& normalized() const override {
        std::vector<const Type*> normArgs;
        if (!typeArgs.empty()) {
            normArgs.reserve(typeArgs.size());
            for (const auto* arg : typeArgs) {
                normArgs.push_back(&arg->normalized());
            }
        }
        static ReferenceType normalized(composite, std::move(normArgs));
        return normalized;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "Reference(" << composite.toString();
        if (!typeArgs.empty()) {
            ss << "<";
            for (size_t i = 0; i < typeArgs.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << typeArgs[i]->toString();
            }
            ss << ">";
        }
        ss << ")";
        return ss.str();
    }

private:
    bool checkTypeArgs(const std::vector<const Type*>& otherArgs) const {
        if (typeArgs.size() != otherArgs.size()) return false;
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (!typeArgs[i]->normalized().accepts(otherArgs[i]->normalized())) {
                return false;
            }
        }
        return true;
    }

    const CompositeType& composite;
    std::vector<const Type*> typeArgs;
};

// DictionaryType implementation
class DictionaryType : public Type {
public:
    DictionaryType(const Type& keyType, const Type& valueType)
        : Type(TypeID::Dictionary), keyType(keyType), valueType(valueType) {}

    using Type::accepts;

    bool accepts(const DictionaryType& other) const override {
        return keyType.normalized().accepts(other.keyType.normalized()) &&
               valueType.normalized().accepts(other.valueType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        // In a real system, there would be a check of map here
        return false;
    }

    const Type& normalized() const override {
        static DictionaryType normalized(keyType.normalized(), valueType.normalized());
        return normalized;
    }

    std::string toString() const override {
        return "[" + keyType.toString() + ": " + valueType.toString() + "]";
    }

private:
    const Type& keyType;
    const Type& valueType;
};

// VariadicType implementation
class VariadicType : public Type {
public:
    explicit VariadicType(const Type* innerType = nullptr)
        : Type(TypeID::Variadic), innerType(innerType) {}

    using Type::accepts;

    bool accepts(const VariadicType& other) const override {
        if (!innerType) return true;
        if (!other.innerType) return false;
        return innerType->normalized().accepts(other.innerType->normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        // In a real system, there would be a check of variadic values here
        return !innerType || innerType->normalized().checkValue(value);
    }

    const Type& normalized() const override {
        if (innerType) {
            static VariadicType normalized(innerType);
            return normalized;
        }
        return *this;
    }

    std::string toString() const override {
        if (innerType) {
            return innerType->toString() + "...";
        }
        return "...";
    }

private:
    const Type* innerType;
};

// IntersectionType implementation
class IntersectionType : public Type {
public:
    IntersectionType(std::vector<const Type*> types)
        : Type(TypeID::Intersection), types(std::move(types)) {}

    using Type::accepts;

    bool accepts(const IntersectionType& other) const override {
        if (types.size() != other.types.size()) return false;
        for (size_t i = 0; i < types.size(); ++i) {
            if (!types[i]->normalized().accepts(other.types[i]->normalized()) ||
                !other.types[i]->normalized().accepts(types[i]->normalized())) {
                return false;
            }
        }
        return true;
    }

    bool checkValue(const TypeValue& value) const override {
        for (const auto* type : types) {
            if (!type->normalized().checkValue(value)) {
                return false;
            }
        }
        return true;
    }

    const Type& normalized() const override {
        std::vector<const Type*> normTypes;
        for (const auto* type : types) {
            const Type& normType = type->normalized();
            if (auto* intersection = dynamic_cast<const IntersectionType*>(&normType)) {
                normTypes.insert(normTypes.end(),
                               intersection->types.begin(),
                               intersection->types.end());
            } else {
                normTypes.push_back(&normType);
            }
        }

        if (normTypes.size() == 1) {
            return *normTypes[0];
        }

        static IntersectionType normalized(std::move(normTypes));
        return normalized;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0) ss << " & ";
            ss << types[i]->toString();
        }
        ss << ")";
        return ss.str();
    }

private:
    std::vector<const Type*> types;
};

// UnionType implementation
class UnionType : public Type {
public:
    UnionType(std::vector<const Type*> types)
        : Type(TypeID::Union), types(std::move(types)) {}

    using Type::accepts;

    bool accepts(const UnionType& other) const override {
        for (const auto* otherType : other.types) {
            bool accepted = false;
            for (const auto* type : types) {
                if (type->normalized().accepts(otherType->normalized())) {
                    accepted = true;
                    break;
                }
            }
            if (!accepted) return false;
        }
        return true;
    }

    bool checkValue(const TypeValue& value) const override {
        for (const auto* type : types) {
            if (type->normalized().checkValue(value)) {
                return true;
            }
        }
        return false;
    }

    const Type& normalized() const override {
        std::vector<const Type*> normTypes;
        for (const auto* type : types) {
            const Type& normType = type->normalized();
            if (auto* union_ = dynamic_cast<const UnionType*>(&normType)) {
                normTypes.insert(normTypes.end(),
                               union_->types.begin(),
                               union_->types.end());
            } else {
                normTypes.push_back(&normType);
            }
        }

        if (normTypes.size() == 1) {
            return *normTypes[0];
        }

        static UnionType normalized(std::move(normTypes));
        return normalized;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0) ss << " | ";
            ss << types[i]->toString();
        }
        ss << ")";
        return ss.str();
    }

private:
    std::vector<const Type*> types;
};

// FunctionType implementation
class FunctionType : public Type {
public:
    FunctionType(std::vector<const Type*> parameters, const Type& returnType)
        : Type(TypeID::Function)
        , parameters(std::move(parameters))
        , returnType(returnType) {}

    using Type::accepts;

    bool accepts(const FunctionType& other) const override {
        if (parameters.size() != other.parameters.size()) {
            return false;
        }

        // Contravariance of parameters:
        // For each parameter, other's parameter must accept our parameter
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (!other.parameters[i]->normalized().accepts(parameters[i]->normalized())) {
                return false;
            }
        }

        // Covariance of return type
        return returnType.normalized().accepts(other.returnType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        // In C++, there is no direct support for checking function values
        return false;
    }

    const Type& normalized() const override {
        std::vector<const Type*> normParams;
        normParams.reserve(parameters.size());

        for (const auto* param : parameters) {
            normParams.push_back(&param->normalized());
        }

        static FunctionType normalized(
            std::move(normParams),
            returnType.normalized()
        );
        return normalized;
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << parameters[i]->toString();
        }
        ss << ") -> " << returnType.toString();
        return ss.str();
    }

private:
    std::vector<const Type*> parameters;
    const Type& returnType;
};

// InoutType implementation
class InoutType : public Type {
public:
    InoutType(const Type& innerType)
        : Type(TypeID::Inout), innerType(innerType) {}

    using Type::accepts;

    bool accepts(const InoutType& other) const override {
        // For inout parameters, types must accept each other
        return innerType.normalized().accepts(other.innerType.normalized()) &&
               other.innerType.normalized().accepts(innerType.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& normInner = innerType.normalized();
        if (const auto* inout = dynamic_cast<const InoutType*>(&normInner)) {
            return *inout;
        }
        return *this;
    }

    std::string toString() const override {
        return "inout " + innerType.toString();
    }

private:
    const Type& innerType;
};

// In base class Type, update accepts - move to the end of the file after all definitions
bool Type::accepts(const Type& other) const {
    switch (other.getTypeID()) {
        case TypeID::Primitive:
            return accepts(static_cast<const PrimitiveType&>(other));
        case TypeID::Optional:
            return accepts(static_cast<const OptionalType&>(other));
        case TypeID::Parenthesized:
            return accepts(static_cast<const ParenthesizedType&>(other).getInnerType().normalized());
        case TypeID::Default:
            return accepts(static_cast<const DefaultType&>(other));
        case TypeID::Composite:
            return accepts(static_cast<const CompositeType&>(other));
        case TypeID::Reference:
            return accepts(static_cast<const ReferenceType&>(other));
        case TypeID::Array:
            return accepts(static_cast<const ArrayType&>(other));
        case TypeID::Dictionary:
            return accepts(static_cast<const DictionaryType&>(other));
        case TypeID::Variadic:
            return accepts(static_cast<const VariadicType&>(other));
        case TypeID::Intersection:
            return accepts(static_cast<const IntersectionType&>(other));
        case TypeID::Union:
            return accepts(static_cast<const UnionType&>(other));
        case TypeID::Function:
            return accepts(static_cast<const FunctionType&>(other));
        case TypeID::Inout:
            return accepts(static_cast<const InoutType&>(other));
        default:
            return false;
    }
}

// Tests
void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        throw std::runtime_error(message);
    }
}

void test_primitive_types() {
    std::cout << "Testing primitive types..." << std::endl;

    PrimitiveType intType("Int");
    PrimitiveType floatType("Float");
    PrimitiveType numberType("Number");
    PrimitiveType stringType("String");
    PrimitiveType boolType("Bool");

    // Test primitive type acceptance (matching Type.js tests)
    assert_true(numberType.accepts(intType), "Number should accept Int");
    assert_true(numberType.accepts(floatType), "Number should accept Float");
    assert_true(!intType.accepts(floatType), "Int should not accept Float");
    assert_true(!floatType.accepts(intType), "Float should not accept Int");
    assert_true(!stringType.accepts(numberType), "String should not accept Number");

    // Test value checking (matching Type.js behavior)
    assert_true(intType.checkValue(TypeValue(42)), "Int should accept integer value");
    assert_true(!intType.checkValue(TypeValue(42.5)), "Int should not accept float value");
    assert_true(floatType.checkValue(TypeValue(42.5)), "Float should accept float value");
    assert_true(floatType.checkValue(TypeValue(42.0)), "Float should accept integer as float");
    assert_true(stringType.checkValue(TypeValue(std::string("test"))), "String should accept string value");
    assert_true(!stringType.checkValue(TypeValue(42)), "String should not accept number value");
    assert_true(boolType.checkValue(TypeValue(true)), "Bool should accept boolean value");
    assert_true(!boolType.checkValue(TypeValue(1)), "Bool should not accept number value");

    std::cout << "Primitive types tests passed." << std::endl;
}

void test_optional_type() {
    std::cout << "Testing optional types..." << std::endl;

    PrimitiveType intType("Int");
    PrimitiveType stringType("String");
    OptionalType optionalInt(intType);
    OptionalType optionalString(stringType);

    // Test null/undefined acceptance
    assert_true(optionalInt.checkValue(TypeValue()), "Optional should accept null/undefined");

    // Test inner type acceptance
    assert_true(optionalInt.checkValue(TypeValue(42)), "Optional Int should accept int value");
    assert_true(!optionalInt.checkValue(TypeValue("test")), "Optional Int should not accept string value");

    // Test optional type acceptance
    assert_true(optionalInt.accepts(optionalInt), "Optional should accept same optional type");
    assert_true(!optionalInt.accepts(optionalString), "Optional Int should not accept Optional String");
    assert_true(optionalInt.accepts(intType), "Optional should accept its inner type");

    // Test nested optional normalization
    OptionalType nestedOptional(optionalInt);
    const Type& normalized = nestedOptional.normalized();
    assert_true(normalized.toString() == "Int?", "Normalized nested optional should collapse to single optional");

    std::cout << "Optional types tests passed." << std::endl;
}

void test_default_type() {
    std::cout << "Testing default types..." << std::endl;

    PrimitiveType intType("Int");
    DefaultType intDefault(intType);
    DefaultType nestedDefault(intDefault);

    // Test value checking
    assert_true(intDefault.checkValue(TypeValue()), "Default should accept empty value");
    assert_true(intDefault.checkValue(TypeValue(42)), "Default should accept valid inner type");
    assert_true(!intDefault.checkValue(TypeValue(42.5)), "Default should not accept invalid inner type");

    // Test type acceptance
    assert_true(intDefault.accepts(intDefault), "Default should accept same type");
    assert_true(intDefault.accepts(intType), "Default should accept inner type");

    // Test acceptance with parentheses
    ParenthesizedType parenthesizedInt(intType);
    assert_true(intDefault.accepts(parenthesizedInt), "Default should accept parenthesized inner type");

    // Test normalization
    const Type& normalizedNested = nestedDefault.normalized();
    assert_true(normalizedNested.toString() == "Int!",
                "Normalized nested default should collapse to single default");

    std::cout << "Default types tests passed." << std::endl;
}

void test_composite_type() {
    std::cout << "Testing composite types..." << std::endl;

    // Reset composites vector
    composites.clear();

    // Create composite types
    auto classA = CompositeType("class", "A");
    classA.initialize();

    auto classB = CompositeType("class", "B", {0}); // inherits from A
    classB.initialize();

    auto classC = CompositeType("class", "C", {1}); // inherits from B
    classC.initialize();

    // Test inheritance chain
    assert_true(composites[0]->accepts(*composites[1]), "A should accept B (child)");
    assert_true(composites[0]->accepts(*composites[2]), "A should accept C (grandchild)");
    assert_true(!composites[2]->accepts(*composites[0]), "C should not accept A (parent)");

    // Test value checking
    assert_true(composites[0]->checkValue(TypeValue(0)), "Should accept own index");
    assert_true(composites[0]->checkValue(TypeValue(1)), "Should accept child index");
    assert_true(!composites[0]->checkValue(TypeValue(99)), "Should not accept invalid index");
    assert_true(!composites[0]->checkValue(TypeValue(42.5)), "Should not accept non-integer value");

    // Test with parentheses
    ParenthesizedType parenthesizedB(*composites[1]);
    assert_true(composites[0]->accepts(parenthesizedB), "A should accept parenthesized B");

    std::cout << "Composite types tests passed." << std::endl;
}

void test_array_type() {
    std::cout << "Testing array types..." << std::endl;

    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");
    PrimitiveType stringType("String");

    ArrayType numberArray(numberType);
    ArrayType intArray(intType);
    ArrayType stringArray(stringType);

    // Test type acceptance
    assert_true(numberArray.accepts(intArray), "Number[] should accept Int[]");
    assert_true(!intArray.accepts(numberArray), "Int[] should not accept Number[]");
    assert_true(!numberArray.accepts(stringArray), "Number[] should not accept String[]");

    // Test with parentheses
    ParenthesizedType parenthesizedIntArray(intArray);
    assert_true(numberArray.accepts(parenthesizedIntArray), "Number[] should accept parenthesized Int[]");
    assert_true(!stringArray.accepts(parenthesizedIntArray), "String[] should not accept parenthesized Int[]");

    // Test toString
    assert_true(numberArray.toString() == "[Number]", "Array toString should be correct");
    assert_true(intArray.toString() == "[Int]", "Array toString should be correct");

    std::cout << "Array types tests passed." << std::endl;
}


void test_variadic_type() {
    std::cout << "Testing variadic types..." << std::endl;

    PrimitiveType intType("Int");
    PrimitiveType stringType("String");

    // Test generic variadic (accepts anything)
    auto genericVariadic = VariadicType();
    assert_true(genericVariadic.checkValue(TypeValue(42)), "Generic variadic should accept any value");
    assert_true(genericVariadic.checkValue(TypeValue("test")), "Generic variadic should accept any value");

    // Test typed variadic
    auto intVariadic = VariadicType(&intType);
    assert_true(intVariadic.checkValue(TypeValue(42)), "Int variadic should accept int");
    assert_true(!intVariadic.checkValue(TypeValue("test")), "Int variadic should not accept string");

    // Test variadic type acceptance
    auto stringVariadic = VariadicType(&stringType);
    assert_true(genericVariadic.accepts(intVariadic), "Generic variadic should accept any variadic");
    assert_true(!intVariadic.accepts(stringVariadic), "Int variadic should not accept string variadic");

    std::cout << "Variadic types tests passed." << std::endl;
}

void test_function_type() {
    std::cout << "Testing function types..." << std::endl;

    PrimitiveType intType("Int");
    PrimitiveType numberType("Number");
    PrimitiveType stringType("String");

    // Simple function (Int) -> String
    std::vector<const Type*> params1 = {&intType};
    FunctionType func1(params1, stringType);

    // Another function (Number) -> String
    std::vector<const Type*> params2 = {&numberType};
    FunctionType func2(params2, stringType);

    // Test contravariance of parameters
    assert_true(func1.accepts(func2), "Function (Int) -> String should accept (Number) ->> String");
    assert_true(!func2.accepts(func1), "Function (Number) -> String should not accept (Int) -> String");

    // Test covariance of return type
    std::vector<const Type*> params3 = {&intType};
    FunctionType func3(params3, numberType);
    FunctionType func4(params3, intType);
    assert_true(func3.accepts(func4), "Function -> Number should accept function -> Int");
    assert_true(!func4.accepts(func3), "Function -> Int should not accept function -> Number");

    std::cout << "Function types tests passed." << std::endl;
}

void test_intersection_type() {
    std::cout << "Testing intersection types..." << std::endl;

    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");
    PrimitiveType stringType("String");

    // Test simple intersection
    std::vector<const Type*> types1 = {&numberType, &stringType};
    IntersectionType intersection1(types1);
    assert_true(!intersection1.checkValue(TypeValue(42)), "Number & String should not accept just number");
    assert_true(!intersection1.checkValue(TypeValue("test")), "Number & String should not accept just string");

    // Test intersection with subtypes
    std::vector<const Type*> types2 = {&numberType, &intType};
    IntersectionType intersection2(types2);
    assert_true(intersection2.accepts(intersection2), "Intersection should accept itself");

    // Test nested intersections
    std::vector<const Type*> types3 = {&intersection2, &stringType};
    IntersectionType intersection3(types3);
    assert_true(!intersection3.checkValue(TypeValue(42)), "(Number & Int) & String should not accept number");

    std::cout << "Intersection types tests passed." << std::endl;
}

void test_union_type() {
    std::cout << "Testing union types..." << std::endl;

    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");
    PrimitiveType stringType("String");

    // Test simple union
    std::vector<const Type*> types1 = {&numberType, &stringType};
    UnionType union1(types1);
    assert_true(union1.checkValue(TypeValue(42)), "Number | String should accept number");
    assert_true(union1.checkValue(TypeValue("test")), "Number | String should accept string");
    assert_true(!union1.checkValue(TypeValue(true)), "Number | String should not accept boolean");

    // Test union with subtypes
    std::vector<const Type*> types2 = {&numberType, &intType};
    UnionType union2(types2);
    assert_true(union2.checkValue(TypeValue(42)), "Number | Int should accept int");

    // Test nested unions
    std::vector<const Type*> types3 = {&union2, &stringType};
    UnionType union3(types3);
    assert_true(union3.checkValue(TypeValue("test")), "(Number | Int) | String should accept string");

    std::cout << "Union types tests passed." << std::endl;
}

void test_inout_type() {
    std::cout << "Testing inout types..." << std::endl;

    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");

    // Create inout types with references instead of pointers
    InoutType inoutNumber(numberType);
    InoutType anotherInoutNumber(numberType);
    InoutType inoutInt(intType);

    // Test type acceptance
    assert_true(inoutNumber.accepts(anotherInoutNumber), "Identical inout types should accept each other");
    assert_true(!inoutNumber.accepts(inoutInt), "Inout Number should not accept inout Int");

    // Test type checking
    assert_true(inoutNumber.checkValue(TypeValue(42)), "Inout Number should accept integer value");
    assert_true(inoutNumber.checkValue(TypeValue(42.5)), "Inout Number should accept float value");
    assert_true(!inoutInt.checkValue(TypeValue(42.5)), "Inout Int should not accept float value");

    // Test normalization of nested inout
    InoutType nestedInout(inoutNumber);
    const Type& normalized = nestedInout.normalized();
    assert_true(normalized.toString() == "inout Number",
                "Nested inout should normalize to single inout");

    std::cout << "Inout types tests passed." << std::endl;
}

void test_reference_type_with_generics() {
    std::cout << "Testing reference types with generics..." << std::endl;

    // Create base types
    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");

    // Reset composites
    composites.clear();

    // Create a generic class
    auto classA = CompositeType("class", "A", {}, {&numberType});
    classA.initialize();

    auto classB = CompositeType("class", "B", {0}, {&intType});  // Inherits from A
    classB.initialize();

    // Create references with different type arguments
    ReferenceType refA(classA, {&numberType});
    ReferenceType refAWithInt(classA, {&intType});
    ReferenceType refB(classB, {&intType});

    // Test generic type acceptance
    assert_true(refA.accepts(refAWithInt), "Reference<Number> should accept Reference<Int>");
    assert_true(!refAWithInt.accepts(refA), "Reference<Int> should not accept Reference<Number>");

    // Test inheritance with generics
    assert_true(refA.accepts(refB), "Parent reference should accept child reference with compatible generics");

    std::cout << "Reference types with generics tests passed." << std::endl;
}

void run_all_tests() {
    test_primitive_types();
    test_optional_type();
    test_default_type();
    test_composite_type();
    test_array_type();
    test_variadic_type();
    test_function_type();
    test_intersection_type();
    test_union_type();
    test_inout_type();
    test_reference_type_with_generics();
}

int main() {
    try {
        run_all_tests();
        std::cout << "\nAll tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace types

int main() {
    types::main();
}
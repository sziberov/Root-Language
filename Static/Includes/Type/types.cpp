#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <stdexcept>
#include <any>
#include <set>
#include <sstream>
#include <cassert>
#include <algorithm> // Added for std::all_of

namespace types {

// Forward declarations
class Type;
using TypePtr = std::shared_ptr<Type>;

// Global vector for composite types
std::vector<std::shared_ptr<class CompositeType>> composites;

class Type : public std::enable_shared_from_this<Type> {
public:
    virtual ~Type() = default;
    virtual bool acceptsA(const TypePtr& other) = 0;
    virtual bool checkValue(const std::any& value) = 0;
    virtual TypePtr normalized() = 0;
    virtual std::string toString() const = 0;

    virtual bool conformsTo(const TypePtr& other) {
        return other->acceptsA(shared_from_this());
    }

    virtual TypePtr unwrap() {
        return shared_from_this();
    }
};

class PrimitiveType : public Type {
public:
    explicit PrimitiveType(std::string name) : name(std::move(name)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();
        auto primitiveOther = std::dynamic_pointer_cast<PrimitiveType>(unwrapped);

        if (primitiveOther) {
            return name == primitiveOther->name ||
                   (name == "Number" && (primitiveOther->name == "Int" || primitiveOther->name == "Float"));
        }
        return false;
    }

    bool checkValue(const std::any& value) override {
        try {
            if (name == "Int" || name == "Number") {
                return value.type() == typeid(int);
            } else if (name == "Float") {
                return value.type() == typeid(float) || value.type() == typeid(double);
            } else if (name == "String") {
                return value.type() == typeid(std::string);
            } else if (name == "Bool") {
                return value.type() == typeid(bool);
            }
        } catch (const std::bad_any_cast&) {
            return false;
        }
        return false;
    }

    TypePtr normalized() override { return shared_from_this(); }
    std::string toString() const override { return name; }

private:
    std::string name;
};

class OptionalType : public Type {
public:
    explicit OptionalType(TypePtr innerType) : innerType(std::move(innerType)) {}

    bool acceptsA(const TypePtr& other) override {
        auto normOther = other->normalized();
        if (!normOther || !normOther->checkValue(std::any())) return true;

        if (auto optOther = std::dynamic_pointer_cast<OptionalType>(other)) {
            return innerType->normalized()->acceptsA(optOther->innerType->normalized());
        }
        return innerType->normalized()->acceptsA(normOther);
    }

    bool checkValue(const std::any& value) override {
        if (!value.has_value()) return true;
        try {
            if (value.type() == typeid(std::nullptr_t)) return true;
        } catch (const std::bad_any_cast&) {}
        return innerType->normalized()->checkValue(value);
    }

    TypePtr normalized() override {
        auto innerNorm = innerType->normalized();
        if (auto opt = std::dynamic_pointer_cast<OptionalType>(innerNorm)) {
            return opt;
        }
        return shared_from_this();
    }

    std::string toString() const override {
        return innerType->toString() + "?";
    }

private:
    TypePtr innerType;
};

class DefaultType : public Type {
public:
    explicit DefaultType(TypePtr innerType) : innerType(std::move(innerType)) {}

    bool acceptsA(const TypePtr& other) override {
        auto normOther = other->normalized();
        if (!normOther || !normOther->checkValue(std::any())) return true;

        if (auto defaultOther = std::dynamic_pointer_cast<DefaultType>(other)) {
            return innerType->normalized()->acceptsA(defaultOther->innerType->normalized());
        }
        return innerType->normalized()->acceptsA(normOther);
    }

    bool checkValue(const std::any& value) override {
        if (!value.has_value()) return true;
        try {
            if (value.type() == typeid(std::nullptr_t)) return true;
        } catch (const std::bad_any_cast&) {}
        return innerType->normalized()->checkValue(value);
    }

    TypePtr normalized() override {
        auto innerNorm = innerType->normalized();
        if (auto def = std::dynamic_pointer_cast<DefaultType>(innerNorm)) {
            return def;
        }
        return shared_from_this();
    }

    std::string toString() const override {
        return innerType->toString() + "!";
    }

private:
    TypePtr innerType;
};

class ParenthesizedType : public Type {
public:
    explicit ParenthesizedType(TypePtr innerType) : innerType(std::move(innerType)) {}

    bool acceptsA(const TypePtr& other) override {
        return innerType->normalized()->acceptsA(other->normalized());
    }

    bool checkValue(const std::any& value) override {
        return innerType->checkValue(value);
    }

    TypePtr normalized() override {
        return innerType->normalized();
    }

    TypePtr unwrap() override {
        return innerType->unwrap();
    }

    std::string toString() const override {
        return "(" + innerType->toString() + ")";
    }

private:
    TypePtr innerType;
};

class CompositeType : public Type {
public:
    CompositeType(std::string kind, std::string name,
                  std::vector<size_t> inherits = {},
                  std::vector<TypePtr> generics = {})
        : kind(std::move(kind))
        , name(std::move(name))
        , inherits(std::move(inherits))
        , generics(std::move(generics))
        , index(composites.size()) {}

    void initialize() {
        composites.push_back(std::static_pointer_cast<CompositeType>(shared_from_this()));
    }

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

    static bool checkConformance(const std::shared_ptr<CompositeType>& base,
                              const std::shared_ptr<CompositeType>& candidate,
                              const std::vector<TypePtr>& candidateArgs = {}) {
        if (!(candidate->index == base->index ||
              candidate->getFullInheritanceChain().count(base->index))) {
            return false;
        }

        if (!base->generics.empty()) {
            std::vector<TypePtr> actualArgs = candidateArgs.empty() ? base->generics : candidateArgs;

            if (actualArgs.size() != base->generics.size()) {
                return false;
            }

            for (size_t i = 0; i < base->generics.size(); i++) {
                if (!base->generics[i]->normalized()->acceptsA(actualArgs[i]->normalized())) {
                    return false;
                }
            }
        }

        return true;
    }

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();
        if (auto compOther = std::dynamic_pointer_cast<CompositeType>(unwrapped)) {
            return checkConformance(
                std::static_pointer_cast<CompositeType>(shared_from_this()),
                compOther
            );
        }
        return false;
    }

    bool checkValue(const std::any& value) override {
        try {
            auto idx = std::any_cast<size_t>(value);
            if (idx >= composites.size()) return false;
            auto comp = composites[idx];
            return comp && this->acceptsA(comp->shared_from_this());
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    TypePtr normalized() override { return shared_from_this(); }

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

private:
    std::string kind;
    std::string name;
    std::vector<size_t> inherits;
    std::vector<TypePtr> generics;
    size_t index;
};

class ReferenceType : public Type {
public:
    ReferenceType(std::shared_ptr<CompositeType> composite, std::vector<TypePtr> typeArgs = {})
        : composite(std::move(composite)), typeArgs(std::move(typeArgs)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();

        if (auto compOther = std::dynamic_pointer_cast<CompositeType>(unwrapped)) {
            return CompositeType::checkConformance(composite, compOther, typeArgs);
        }

        if (auto refOther = std::dynamic_pointer_cast<ReferenceType>(unwrapped)) {
            return CompositeType::checkConformance(composite, refOther->composite, typeArgs);
        }

        return false;
    }

    bool checkValue(const std::any& value) override {
        try {
            auto index = std::any_cast<size_t>(value);
            if (index >= composites.size()) return false;

            auto comp = composites[index];
            if (!comp) return false;

            return this->acceptsA(comp);
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    TypePtr normalized() override {
        std::vector<TypePtr> normArgs;
        if (!typeArgs.empty()) {
            normArgs.reserve(typeArgs.size());
            for (const auto& arg : typeArgs) {
                normArgs.push_back(arg->normalized());
            }
        }
        return std::make_shared<ReferenceType>(composite, std::move(normArgs));
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "Reference(" << composite->toString();
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
    std::shared_ptr<CompositeType> composite;
    std::vector<TypePtr> typeArgs;
};

class ArrayType : public Type {
public:
    explicit ArrayType(TypePtr elementType) : elementType(std::move(elementType)) {}

    bool acceptsA(const TypePtr& other) override {
        // First unwrap in case other is parenthesized
        auto candidate = other->unwrap();

        // If candidate is not an ArrayType, comparison fails
        auto arrayOther = std::dynamic_pointer_cast<ArrayType>(candidate);
        if (!arrayOther) return false;

        // Check covariance: expected element must accept provided element
        return elementType->normalized()->acceptsA(arrayOther->elementType->normalized());
    }

    bool checkValue(const std::any& value) override {
        try {
            // In C++, we'll check if the value is a vector of any type
            // This is a basic implementation - in a real system you might want more sophisticated checks
            return value.type() == typeid(std::vector<std::any>);
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    TypePtr normalized() override {
        return std::make_shared<ArrayType>(elementType->normalized());
    }

    std::string toString() const override {
        return "[" + elementType->toString() + "]";
    }

private:
    TypePtr elementType;
};

class DictionaryType : public Type {
public:
    DictionaryType(TypePtr keyType, TypePtr valueType)
        : keyType(std::move(keyType)), valueType(std::move(valueType)) {}

    bool acceptsA(const TypePtr& other) override {
        // First unwrap in case other is parenthesized
        auto candidate = other->unwrap();

        // If candidate is not a DictionaryType, comparison fails
        auto dictOther = std::dynamic_pointer_cast<DictionaryType>(candidate);
        if (!dictOther) return false;

        // Check covariance for both key and value types
        bool keyOk = keyType->normalized()->acceptsA(dictOther->keyType->normalized());
        bool valueOk = valueType->normalized()->acceptsA(dictOther->valueType->normalized());
        return keyOk && valueOk;
    }

    bool checkValue(const std::any& value) override {
        try {
            // In C++, we'll check if the value is a map type
            // This is a basic implementation - in a real system you might want more sophisticated checks
            return value.type() == typeid(std::map<std::any, std::any>);
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    TypePtr normalized() override {
        return std::make_shared<DictionaryType>(
            keyType->normalized(),
            valueType->normalized()
        );
    }

    std::string toString() const override {
        return "[" + keyType->toString() + ": " + valueType->toString() + "]";
    }

    TypePtr getKeyType() const { return keyType; }
    TypePtr getValueType() const { return valueType; }

private:
    TypePtr keyType;
    TypePtr valueType;
};

class VariadicType : public Type {
public:
    explicit VariadicType(TypePtr innerType = nullptr) : innerType(std::move(innerType)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();
        auto variadicOther = std::dynamic_pointer_cast<VariadicType>(unwrapped);

        // If other is not a VariadicType
        if (!variadicOther) {
            // If this variadic has no innerType, accept any type
            if (!innerType) return true;
            // Compare innerType with other directly
            return innerType->normalized()->acceptsA(unwrapped->normalized());
        }

        // If this variadic has no innerType, accept any variadic
        if (!innerType) return true;

        // If provided VariadicType has no innerType, do not accept it
        if (!variadicOther->innerType) return false;

        // Both have innerType, compare them
        return innerType->normalized()->acceptsA(variadicOther->innerType->normalized());
    }

    bool checkValue(const std::any& value) override {
        try {
            // Value can be either a vector (for multiple values) or a single value
            if (value.type() == typeid(std::vector<std::any>)) {
                if (!innerType) return true;
                auto& vec = std::any_cast<const std::vector<std::any>&>(value);
                return std::all_of(vec.begin(), vec.end(),
                    [this](const std::any& v) { return innerType->normalized()->checkValue(v); });
            } else {
                if (!innerType) return true;
                return innerType->normalized()->checkValue(value);
            }
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    TypePtr normalized() override {
        if (innerType) {
            return std::make_shared<VariadicType>(innerType->normalized());
        }
        return shared_from_this();
    }

    std::string toString() const override {
        if (innerType) {
            return innerType->toString() + "...";
        }
        return "...";
    }

private:
    TypePtr innerType;
};

class IntersectionType : public Type {
public:
    explicit IntersectionType(std::vector<TypePtr> alternatives)
        : alternatives(std::move(alternatives)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();

        // If comparing with another intersection type
        if (auto otherIntersection = std::dynamic_pointer_cast<IntersectionType>(unwrapped)) {
            // For same intersection type, compare alternatives
            if (alternatives.size() == otherIntersection->alternatives.size()) {
                for (size_t i = 0; i < alternatives.size(); ++i) {
                    // Each alternative should be equivalent (accept each other)
                    if (!alternatives[i]->normalized()->acceptsA(otherIntersection->alternatives[i]->normalized()) ||
                        !otherIntersection->alternatives[i]->normalized()->acceptsA(alternatives[i]->normalized())) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        // For non-intersection types, check if the type satisfies all alternatives
        for (const auto& alt : alternatives) {
            if (!alt->normalized()->acceptsA(unwrapped->normalized())) {
                return false;
            }
        }
        return true;
    }

    bool checkValue(const std::any& value) override {
        // Value conforms to IntersectionType if it satisfies all alternatives
        for (const auto& alt : alternatives) {
            if (!alt->normalized()->checkValue(value)) {
                return false;
            }
        }
        return true;
    }

    TypePtr normalized() override {
        // Normalize each alternative and "flatten" nested IntersectionTypes
        std::vector<TypePtr> normAlts;
        for (const auto& alt : alternatives) {
            auto normAlt = alt->normalized();
            if (auto intersectionAlt = std::dynamic_pointer_cast<IntersectionType>(normAlt)) {
                // If alternative is itself an intersection, add its alternatives
                normAlts.insert(normAlts.end(),
                             intersectionAlt->alternatives.begin(),
                             intersectionAlt->alternatives.end());
            } else {
                normAlts.push_back(normAlt);
            }
        }

        // If after normalization only one alternative remains, return it directly
        if (normAlts.size() == 1) {
            return normAlts[0];
        }
        return std::make_shared<IntersectionType>(std::move(normAlts));
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < alternatives.size(); ++i) {
            if (i > 0) ss << " & ";
            ss << alternatives[i]->toString();
        }
        ss << ")";
        return ss.str();
    }

private:
    std::vector<TypePtr> alternatives;
};


// InoutType implementation
class InoutType : public Type {
public:
    explicit InoutType(TypePtr innerType) : innerType(std::move(innerType)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();

        if (auto inoutOther = std::dynamic_pointer_cast<InoutType>(unwrapped)) {
            // Both types must accept each other for inout parameters
            return innerType->normalized()->acceptsA(inoutOther->innerType->normalized()) &&
                   inoutOther->innerType->normalized()->acceptsA(innerType->normalized());
        }

        return false;
    }

    bool checkValue(const std::any& value) override {
        // Delegate to inner type's value checking
        return innerType->normalized()->checkValue(value);
    }

    TypePtr normalized() override {
        auto normInner = innerType->normalized();
        if (auto inout = std::dynamic_pointer_cast<InoutType>(normInner)) {
            return inout;
        }
        return shared_from_this();
    }

    std::string toString() const override {
        return "inout " + innerType->toString();
    }

private:
    TypePtr innerType;
};

// Add UnionType implementation
class UnionType : public Type {
public:
    explicit UnionType(std::vector<TypePtr> alternatives)
        : alternatives(std::move(alternatives)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();

        // If comparing with another union type
        if (auto otherUnion = std::dynamic_pointer_cast<UnionType>(unwrapped)) {
            // Each alternative in other must be accepted by at least one of our alternatives
            for (const auto& otherAlt : otherUnion->alternatives) {
                bool accepted = false;
                for (const auto& alt : alternatives) {
                    if (alt->normalized()->acceptsA(otherAlt->normalized())) {
                        accepted = true;
                        break;
                    }
                }
                if (!accepted) return false;
            }
            return true;
        }

        // For non-union types, check if any alternative accepts it
        for (const auto& alt : alternatives) {
            if (alt->normalized()->acceptsA(unwrapped->normalized())) {
                return true;
            }
        }
        return false;
    }

    bool checkValue(const std::any& value) override {
        // Value conforms to UnionType if it satisfies any alternative
        for (const auto& alt : alternatives) {
            if (alt->normalized()->checkValue(value)) {
                return true;
            }
        }
        return false;
    }

    TypePtr normalized() override {
        // Normalize each alternative and "flatten" nested UnionTypes
        std::vector<TypePtr> normAlts;
        for (const auto& alt : alternatives) {
            auto normAlt = alt->normalized();
            if (auto unionAlt = std::dynamic_pointer_cast<UnionType>(normAlt)) {
                // If alternative is itself a union, add its alternatives
                normAlts.insert(normAlts.end(),
                             unionAlt->alternatives.begin(),
                             unionAlt->alternatives.end());
            } else {
                normAlts.push_back(normAlt);
            }
        }

        // If after normalization only one alternative remains, return it directly
        if (normAlts.size() == 1) {
            return normAlts[0];
        }
        return std::make_shared<UnionType>(std::move(normAlts));
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < alternatives.size(); ++i) {
            if (i > 0) ss << " | ";
            ss << alternatives[i]->toString();
        }
        ss << ")";
        return ss.str();
    }

private:
    std::vector<TypePtr> alternatives;
};

class FunctionType : public Type {
public:
    FunctionType(std::vector<TypePtr> parameters, TypePtr returnType)
        : parameters(std::move(parameters)), returnType(std::move(returnType)) {}

    bool acceptsA(const TypePtr& other) override {
        auto unwrapped = other->unwrap();

        // Compare with another function type
        if (auto funcOther = std::dynamic_pointer_cast<FunctionType>(unwrapped)) {
            // Check parameter count match
            if (parameters.size() != funcOther->parameters.size()) {
                return false;
            }

            // Check parameter types (contravariant)
            for (size_t i = 0; i < parameters.size(); ++i) {
                if (!parameters[i]->normalized()->acceptsA(funcOther->parameters[i]->normalized())) {
                    return false;
                }
            }

            // Check return type (covariant)
            return returnType->normalized()->acceptsA(funcOther->returnType->normalized());
        }

        return false;
    }

    bool checkValue(const std::any& value) override {
        // In C++, we can't directly check if a value is a function
        // This would need runtime function type information
        return false;
    }

    TypePtr normalized() override {
        std::vector<TypePtr> normParams;
        normParams.reserve(parameters.size());

        for (const auto& param : parameters) {
            normParams.push_back(param->normalized());
        }

        return std::make_shared<FunctionType>(
            std::move(normParams),
            returnType->normalized()
        );
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << parameters[i]->toString();
        }
        ss << ") -> " << returnType->toString();
        return ss.str();
    }

private:
    std::vector<TypePtr> parameters;
    TypePtr returnType;
};


// Testing framework
void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        throw std::runtime_error(message);
    }
}

void test_primitive_types() {
    std::cout << "Testing primitive types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto floatType = std::make_shared<PrimitiveType>("Float");
    auto numberType = std::make_shared<PrimitiveType>("Number");

    // Test value checking
    assert_true(intType->checkValue(std::any(42)), "Int should accept integer value");
    assert_true(!intType->checkValue(std::any(42.5)), "Int should not accept float value");

    // Test type acceptance
    assert_true(numberType->acceptsA(intType), "Number should accept Int");
    assert_true(numberType->acceptsA(floatType), "Number should accept Float");
    assert_true(!intType->acceptsA(floatType), "Int should not accept Float");

    std::cout << "Primitive types tests passed." << std::endl;
}

void test_optional_type() {
    std::cout << "Testing optional types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto intOptional = std::make_shared<OptionalType>(intType);
    auto nestedOptional = std::make_shared<OptionalType>(intOptional);

    // Test value checking
    assert_true(intOptional->checkValue(std::any()), "Optional should accept empty value");
    assert_true(intOptional->checkValue(std::any(nullptr)), "Optional should accept nullptr");
    assert_true(intOptional->checkValue(std::any(42)), "Optional should accept valid inner type");
    assert_true(!intOptional->checkValue(std::any(42.5)), "Optional should not accept invalid inner type");

    // Test type acceptance
    assert_true(intOptional->acceptsA(intOptional), "Optional should accept same type");
    assert_true(intOptional->acceptsA(intType), "Optional should accept inner type");

    // Test normalization
    auto normalizedNested = nestedOptional->normalized();
    assert_true(normalizedNested->toString() == "Int?",
                "Normalized nested optional should collapse to single optional");

    std::cout << "Optional types tests passed." << std::endl;
}

void test_default_type() {
    std::cout << "Testing default types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto intDefault = std::make_shared<DefaultType>(intType);
    auto nestedDefault = std::make_shared<DefaultType>(intDefault);

    // Test value checking
    assert_true(intDefault->checkValue(std::any()), "Default should accept empty value");
    assert_true(intDefault->checkValue(std::any(nullptr)), "Default should accept nullptr");
    assert_true(intDefault->checkValue(std::any(42)), "Default should accept valid inner type");
    assert_true(!intDefault->checkValue(std::any(42.5)), "Default should not accept invalid inner type");

    // Test type acceptance
    assert_true(intDefault->acceptsA(intDefault), "Default should accept same type");
    assert_true(intDefault->acceptsA(intType), "Default should accept inner type");

    // Test normalization
    auto normalizedNested = nestedDefault->normalized();
    assert_true(normalizedNested->toString() == "Int!",
                "Normalized nested default should collapse to single default");

    // Test toString
    assert_true(intDefault->toString() == "Int!", "Default toString should include !");

    std::cout << "Default types tests passed." << std::endl;
}

void test_parenthesized_type() {
    std::cout << "Testing parenthesized types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto intParenth = std::make_shared<ParenthesizedType>(intType);
    auto intParenthParenth = std::make_shared<ParenthesizedType>(intParenth);
    auto floatType = std::make_shared<PrimitiveType>("Float");
    auto floatParenth = std::make_shared<ParenthesizedType>(floatType);

    // Test type acceptance
    assert_true(intParenth->acceptsA(intType), "Parenthesized should accept inner type");
    assert_true(!intParenth->acceptsA(floatType), "Parenthesized should not accept different type");
    assert_true(intParenthParenth->acceptsA(intType), "Nested parenthesized should accept inner type");
    assert_true(!intParenthParenth->acceptsA(floatParenth), "Parenthesized should not accept different type");

    // Test value checking
    assert_true(intParenth->checkValue(std::any(42)), "Parenthesized should accept valid inner value");
    assert_true(!intParenth->checkValue(std::any(42.5f)), "Parenthesized should not accept invalid inner value");

    // Test unwrap
    assert_true(intParenth->unwrap() == intType, "Unwrap should return inner type");
    assert_true(intParenthParenth->unwrap() == intType, "Nested unwrap should return innermost type");

    // Test toString
    assert_true(intParenth->toString() == "(Int)", "ToString should show parentheses");
    assert_true(intParenthParenth->toString() == "((Int))", "Nested toString should show nested parentheses");

    auto normalizedParenth = intParenthParenth->normalized();
    assert_true(normalizedParenth == intType, "Normalized should return innermost type");

    std::cout << "Parenthesized types tests passed." << std::endl;
}

void test_composite_and_reference_types() {
    std::cout << "Testing composite and reference types..." << std::endl;

    // Reset composites vector
    composites.clear();

    // Create composite types
    auto numberType = std::make_shared<PrimitiveType>("Number");

    auto compA = std::make_shared<CompositeType>("class", "A");
    compA->initialize();

    auto compB = std::make_shared<CompositeType>("object", "B", std::vector<size_t>{compA->getIndex()});
    compB->initialize();

    auto compC = std::make_shared<CompositeType>("object", "C", std::vector<size_t>{compB->getIndex()});
    compC->initialize();

    auto compD = std::make_shared<CompositeType>("object", "D", std::vector<size_t>{compC->getIndex()});
    compD->initialize();

    auto compE = std::make_shared<CompositeType>("object", "E",
                                               std::vector<size_t>{compB->getIndex()},
                                               std::vector<TypePtr>{numberType});
    compE->initialize();

    // Test inheritance chain
    assert_true(compB->acceptsA(compB), "B should accept B (self)");
    assert_true(compB->acceptsA(compC), "B should accept C (direct child)");
    assert_true(compB->acceptsA(compD), "B should accept D (indirect child)");
    assert_true(!compC->acceptsA(compB), "C should not accept B (parent)");

    // Test reference type
    auto basicRef = std::make_shared<ReferenceType>(compA);
    auto genericRef = std::make_shared<ReferenceType>(compE, std::vector<TypePtr>{numberType});

    assert_true(basicRef->acceptsA(compA), "Reference should accept its composite type");
    assert_true(basicRef->acceptsA(compB), "Reference should accept child type");
    assert_true(basicRef->acceptsA(compC), "Reference should accept grandchild type");
    assert_true(!basicRef->acceptsA(std::make_shared<PrimitiveType>("Int")), "Reference should not accept unrelated type");

    assert_true(genericRef->acceptsA(compE), "Generic reference should accept its composite type");
    assert_true(!genericRef->acceptsA(compD), "Generic reference should not accept unrelated type");

    // Test value checking
    assert_true(basicRef->checkValue(std::any(size_t(0))), "Reference should accept valid composite index");
    assert_true(!basicRef->checkValue(std::any(size_t(99))), "Reference should not accept invalid index");
    assert_true(!basicRef->checkValue(std::any(42)), "Reference should not accept non-size_t value");

    std::cout << "Composite and reference types tests passed." << std::endl;
}

void test_array_type() {
    std::cout << "Testing array types..." << std::endl;

    // Create array types
    auto intType = std::make_shared<PrimitiveType>("Int");
    auto intArray = std::make_shared<ArrayType>(intType);
    auto floatType = std::make_shared<PrimitiveType>("Float");
    auto floatArray = std::make_shared<ArrayType>(floatType);
    auto nestedIntArray = std::make_shared<ArrayType>(intArray);

    // Test type acceptance
    assert_true(intArray->acceptsA(intArray), "Same type");
    assert_true(!intArray->acceptsA(floatArray), "Different element type");
    assert_true(!intArray->acceptsA(intType), "Not an array");

    // Test nested arrays
    auto nestedArray = std::make_shared<ArrayType>(intArray);
    assert_true(nestedArray->acceptsA(nestedArray), "Same nested type");
    assert_true(!nestedArray->acceptsA(intArray), "Different nesting level");

    // Test with parenthesized types
    auto parenthIntArray = std::make_shared<ParenthesizedType>(intArray);
    assert_true(intArray->acceptsA(parenthIntArray), "Parenthesized array");

    // Test value checking
    std::vector<std::any> validVector;
    assert_true(intArray->checkValue(std::any(validVector)), "Vector value");
    assert_true(!intArray->checkValue(std::any(42)), "Not a vector");

    // Test toString
    assert_true(intArray->toString() == "[Int]", "Array toString");
    assert_true(nestedIntArray->toString() == "[[Int]]", "Nested array toString");

    // Test normalization
    auto normalizedArray = intArray->normalized();
    assert_true(std::dynamic_pointer_cast<ArrayType>(normalizedArray) != nullptr, "Normalized type");
    assert_true(normalizedArray->toString() == intArray->toString(), "Normalized toString");

    std::cout << "Array types tests passed." << std::endl;
}

void test_dictionary_type() {
    std::cout << "Testing dictionary types..." << std::endl;

    // Create dictionary types
    auto stringType = std::make_shared<PrimitiveType>("String");
    auto intType = std::make_shared<PrimitiveType>("Int");
    auto boolType = std::make_shared<PrimitiveType>("Bool");
    auto stringIntDict = std::make_shared<DictionaryType>(stringType, intType);
    auto stringFloatDict = std::make_shared<DictionaryType>(stringType, std::make_shared<PrimitiveType>("Float"));
    auto intBoolDict = std::make_shared<DictionaryType>(intType, boolType);

    // Test type acceptance
    assert_true(stringIntDict->acceptsA(stringIntDict), "Same type");
    assert_true(!stringIntDict->acceptsA(stringFloatDict), "Different value type");
    assert_true(!stringIntDict->acceptsA(intBoolDict), "Different key and value types");
    assert_true(!stringIntDict->acceptsA(intType), "Not a dictionary");

    // Test with parenthesized types
    auto parenthDict = std::make_shared<ParenthesizedType>(stringIntDict);
    assert_true(stringIntDict->acceptsA(parenthDict), "Parenthesized dictionary");

    // Test value checking
    std::map<std::any, std::any> validMap;
    assert_true(stringIntDict->checkValue(std::any(validMap)), "Map value");
    assert_true(!stringIntDict->checkValue(std::any(42)), "Not a map");

    // Test toString
    assert_true(stringIntDict->toString() == "[String: Int]", "Dictionary toString");
    assert_true(intBoolDict->toString() == "[Int: Bool]", "Different dictionary toString");

    // Test normalization
    auto normalizedDict = stringIntDict->normalized();
    assert_true(std::dynamic_pointer_cast<DictionaryType>(normalizedDict) != nullptr, "Normalized type");
    assert_true(normalizedDict->toString() == stringIntDict->toString(), "Normalized toString");

    // Test type getters
    assert_true(stringIntDict->getKeyType() == stringType, "Key type getter");
    assert_true(stringIntDict->getValueType() == intType, "Value type getter");

    std::cout << "Dictionary types tests passed." << std::endl;
}

void test_variadic_type() {
    std::cout << "Testing variadic types..." << std::endl;

    // Create variadic types
    auto emptyVariadic = std::make_shared<VariadicType>();
    auto intType = std::make_shared<PrimitiveType>("Int");
    auto intVariadic = std::make_shared<VariadicType>(intType);
    auto floatType = std::make_shared<PrimitiveType>("Float");
    auto floatVariadic = std::make_shared<VariadicType>(floatType);

    // Test type acceptance
    assert_true(emptyVariadic->acceptsA(emptyVariadic), "Empty accepts empty");
    assert_true(emptyVariadic->acceptsA(intVariadic), "Empty accepts any");
    assert_true(intVariadic->acceptsA(intVariadic), "Same type");
    assert_true(!intVariadic->acceptsA(floatVariadic), "Different types");
    assert_true(intVariadic->acceptsA(intType), "Direct type");
    assert_true(!intVariadic->acceptsA(floatType), "Wrong direct type");

    // Test value checking
    std::vector<std::any> intValues{std::any(42), std::any(23)};
    assert_true(intVariadic->checkValue(std::any(intValues)), "Array of correct type");
    assert_true(intVariadic->checkValue(std::any(42)), "Single value of correct type");
    assert_true(!intVariadic->checkValue(std::any(42.5f)), "Wrong type");

    // Test empty variadic value checking
    assert_true(emptyVariadic->checkValue(std::any(42)), "Accepts any single value");
    assert_true(emptyVariadic->checkValue(std::any(intValues)), "Accepts any array");

    // Test toString
    assert_true(emptyVariadic->toString() == "...", "Empty variadic toString");
    assert_true(intVariadic->toString() == "Int...", "Int variadic toString");
    assert_true(floatVariadic->toString() == "Float...", "Float variadic toString");

    // Test normalization
    auto normalizedVariadic = intVariadic->normalized();
    assert_true(std::dynamic_pointer_cast<VariadicType>(normalizedVariadic) != nullptr, "Normalized type");
    assert_true(normalizedVariadic->toString() == intVariadic->toString(), "Normalized toString");

    // Test void variadic acceptance
    auto voidVariadic = std::make_shared<VariadicType>();
    assert_true(!intVariadic->acceptsA(voidVariadic), "Non-empty variadic should not accept void variadic");

    std::cout << "Variadic types tests passed." << std::endl;
}

void test_intersection_type() {
    std::cout << "Testing intersection types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto stringType = std::make_shared<PrimitiveType>("String");
    auto floatType = std::make_shared<PrimitiveType>("Float");

    // Create intersection types
    auto intAndString = std::make_shared<IntersectionType>(std::vector<TypePtr>{intType, stringType});
    auto stringAndFloat = std::make_shared<IntersectionType>(std::vector<TypePtr>{stringType, floatType});

    // Test type acceptance
    assert_true(intAndString->acceptsA(intAndString), "Same intersection type");
    assert_true(!intAndString->acceptsA(stringAndFloat), "Different intersection");
    assert_true(!intAndString->acceptsA(intType), "Single type");

    // Test with nested intersections
    auto nestedIntersection = std::make_shared<IntersectionType>(
        std::vector<TypePtr>{intAndString, floatType}
    );
    assert_true(nestedIntersection->acceptsA(nestedIntersection), "Same nested intersection");
    assert_true(!nestedIntersection->acceptsA(intAndString), "Subset intersection");

    // Test value checking
    std::any intValue(42);
    assert_true(!intAndString->checkValue(intValue), "Value should satisfy all types");

    // Test toString
    assert_true(intAndString->toString() == "(Int & String)", "Intersection toString");
    assert_true(stringAndFloat->toString() == "(String & Float)", "Different intersection toString");

    // Test normalization
    auto normalizedIntersection = nestedIntersection->normalized();
    assert_true(normalizedIntersection->toString() == "(Int & String & Float)",
                "Normalized nested intersection");

    std::cout << "Intersection types tests passed." << std::endl;
}

void test_function_type() {
    std::cout << "Testing function types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto stringType = std::make_shared<PrimitiveType>("String");
    auto boolType = std::make_shared<PrimitiveType>("Bool");

    // Create function types
    auto func1 = std::make_shared<FunctionType>(
        std::vector<TypePtr>{intType}, stringType
    );
    auto func2 = std::make_shared<FunctionType>(
        std::vector<TypePtr>{intType}, stringType
    );
    auto func3 = std::make_shared<FunctionType>(
        std::vector<TypePtr>{stringType}, boolType
    );

    // Test type acceptance
    assert_true(func1->acceptsA(func2), "Same function type");
    assert_true(!func1->acceptsA(func3), "Different function type");
    assert_true(!func1->acceptsA(intType), "Non-function type");

    // Test with different parameter counts
    auto func4 = std::make_shared<FunctionType>(
        std::vector<TypePtr>{intType, stringType}, boolType
    );
    assert_true(!func1->acceptsA(func4), "Different parameter count");

    // Test toString
    assert_true(func1->toString() == "(Int) -> String", "Function toString");
    assert_true(func4->toString() == "(Int, String) -> Bool", "Complex function toString");

    // Test normalization
    auto normalizedFunc = func1->normalized();
    assert_true(std::dynamic_pointer_cast<FunctionType>(normalizedFunc) != nullptr, "Normalized type");
    assert_true(normalizedFunc->toString() == func1->toString(), "Normalized toString");

    std::cout << "Function types tests passed." << std::endl;
}

void test_inout_type() {
    std::cout << "Testing inout types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto stringType = std::make_shared<PrimitiveType>("String");

    // Create inout types
    auto inoutInt = std::make_shared<InoutType>(intType);
    auto inoutInt2 = std::make_shared<InoutType>(intType);
    auto inoutString = std::make_shared<InoutType>(stringType);

    // Test type acceptance
    assert_true(inoutInt->acceptsA(inoutInt2), "Same inout type");
    assert_true(!inoutInt->acceptsA(inoutString), "Different inout type");
    assert_true(!inoutInt->acceptsA(intType), "Not an inout type");

    // Test value checking
    assert_true(inoutInt->checkValue(std::any(42)), "Valid inner value");
    assert_true(!inoutInt->checkValue(std::any(42.5)), "Invalid inner value");

    // Test toString
    assert_true(inoutInt->toString() == "inout Int", "Inout toString");
    assert_true(inoutString->toString() == "inout String", "Different inout toString");

    // Test normalization
    auto nestedInout = std::make_shared<InoutType>(inoutInt);
    auto normalizedInout = nestedInout->normalized();
    assert_true(normalizedInout->toString() == "inout Int", "Normalized nested inout");

    std::cout << "Inout types tests passed." << std::endl;
}

void test_union_type() {
    std::cout << "Testing union types..." << std::endl;

    auto intType = std::make_shared<PrimitiveType>("Int");
    auto stringType = std::make_shared<PrimitiveType>("String");
    auto floatType = std::make_shared<PrimitiveType>("Float");

    // Create union types
    auto intOrString = std::make_shared<UnionType>(std::vector<TypePtr>{intType, stringType});
    auto stringOrFloat = std::make_shared<UnionType>(std::vector<TypePtr>{stringType, floatType});

    // Test type acceptance
    assert_true(intOrString->acceptsA(intOrString), "Same union type");
    assert_true(!intOrString->acceptsA(stringOrFloat), "Different union");
    assert_true(intOrString->acceptsA(intType), "Single type from union");
    assert_true(!intOrString->acceptsA(floatType), "Type not in union");

    // Test with nested unions
    auto nestedUnion = std::make_shared<UnionType>(
        std::vector<TypePtr>{intOrString, floatType}
    );
    assert_true(nestedUnion->acceptsA(nestedUnion), "Same nested union");
    assert_true(nestedUnion->acceptsA(intOrString), "Subset union");

    // Test value checking
    assert_true(intOrString->checkValue(std::any(42)), "Int value in union");
    assert_true(!intOrString->checkValue(std::any(42.5)), "Value not in union");

    // Test toString
    assert_true(intOrString->toString() == "(Int | String)", "Union toString");
    assert_true(stringOrFloat->toString() == "(String | Float)", "Different union toString");

    // Test normalization
    auto normalizedUnion = nestedUnion->normalized();
    assert_true(normalizedUnion->toString() == "(Int | String | Float)",
                "Normalized nested union");

    std::cout << "Union types tests passed." << std::endl;
}

int main() {
    try {
        test_primitive_types();
        test_optional_type();
        test_default_type();
        test_parenthesized_type();
        test_composite_and_reference_types();
        test_array_type();
        test_dictionary_type();
        test_variadic_type();
        test_intersection_type();
        test_function_type();
        test_inout_type();
        test_union_type();
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace types

int main() {
    return types::main();
}
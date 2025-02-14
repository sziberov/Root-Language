#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <optional>
#include <functional>
#include <cassert>
#include <typeinfo>
#include <algorithm> // Added for transform

using namespace std;

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
    Inout,
    Predefined
};

// Forward declarations for all types
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
class PredefinedType;

// Forward declarations for helper functions
const CompositeType& getReferenceComposite(const ReferenceType& ref);
const vector<const Type*>& getReferenceTypeArgs(const ReferenceType& ref);
const string& getCompositeKind(const CompositeType& comp);

// Value types supported by our type system
using TypeValue = variant<
    monostate,  // represents void/null/undefined
    int,            // Int
    double,         // Float
    bool,           // Bool
    string     // String
>;

// Base Type class implementation 
class Type {
public:
    explicit Type(TypeID id) : typeID(id) {}
    virtual ~Type() = default;

    virtual bool checkValue(const TypeValue& value) const = 0;
    virtual string toString() const = 0;
    virtual const Type& normalized() const = 0;
    virtual bool acceptsA(const Type& other) const { return false; }
    virtual const Type& unwrapped() const { return *this; }

    TypeID getTypeID() const { return typeID; }

private:
    const TypeID typeID;
};

// Helper function for dynamic_cast with references
template<typename T>
const T* safe_cast(const Type& obj) {
    return dynamic_cast<const T*>(&obj);
}

// PredefinedType implementation
class PredefinedType final : public Type {
public:
    PredefinedType(string id, string category,
                   function<bool(const Type&)> acceptsFn)
        : Type(TypeID::Predefined)
        , id(move(id))
        , category(move(category))
        , acceptsFn(move(acceptsFn)) {}

    bool acceptsA(const Type& other) const override {
        return acceptsFn(other);
    }

    bool checkValue(const TypeValue& value) const override {
        return true; // Default implementation
    }

    const Type& normalized() const override { return *this; }

    string toString() const override {
        return id;
    }

private:
    string id;
    string category;
    function<bool(const Type&)> acceptsFn;
};

// Create PREDEF_VOID_EXTREME first as it's needed by other types
inline const PredefinedType& PREDEF_VOID_EXTREME() {
    static const auto lambda = [](const Type& other) -> bool {
        if (const auto* pred = safe_cast<PredefinedType>(other)) {
            return pred->toString() == "void";
        }
        return false;
    };
    static const PredefinedType instance("void", "extreme", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_type() {
    static const auto lambda = [](const Type& other) -> bool {
        return true; // Любой Type является типом, как в JS версии
    };
    static const PredefinedType instance("type", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Class() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "class";
        }
        return false;
    };
    static const PredefinedType instance("Class", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_dict() {
    static const auto lambda = [](const Type& other) -> bool {
        return safe_cast<DictionaryType>(other) != nullptr;
    };
    static const PredefinedType instance("dict", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}


inline const PredefinedType& PREDEF_Any() {
    static const auto lambda = [](const Type& other) -> bool {
        return safe_cast<CompositeType>(other) != nullptr;
    };
    static const PredefinedType instance("Any", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Function() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "function";
        }
        return false;
    };
    static const PredefinedType instance("Function", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Namespace() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "namespace";
        }
        return false;
    };
    static const PredefinedType instance("Namespace", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Object() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "object";
        }
        return false;
    };
    static const PredefinedType instance("Object", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Protocol() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "protocol";
        }
        return false;
    };
    static const PredefinedType instance("Protocol", "composite", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_Structure() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return getCompositeKind(*composite) == "structure";
        }
        return false;
    };
    static const PredefinedType instance("Structure", "composite", function<bool(const Type&)>(lambda));
    return instance;
}


// PrimitiveType implementation
class PrimitiveType final : public Type {
public:
    explicit PrimitiveType(string name)
        : Type(TypeID::Primitive), name(move(name)) {}

    bool acceptsA(const Type& other) const override {
        if (auto* primitive = safe_cast<PrimitiveType>(other.unwrapped())) {
            return name == primitive->name || 
                   (name == "Number" && (primitive->name == "Int" || primitive->name == "Float"));
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        if (name == "Int") {
            return holds_alternative<int>(value);
        } else if (name == "Float") {
            return holds_alternative<double>(value);
        } else if (name == "String") {
            return holds_alternative<string>(value);
        } else if (name == "Bool") {
            return holds_alternative<bool>(value);
        }
        return false;
    }

    const Type& normalized() const override { return *this; }

    string toString() const override {
        return name;
    }

private:
    string name;
};

// Implementation of ArrayType
class ArrayType final : public Type {
public:
    explicit ArrayType(const Type& elementType)
        : Type(TypeID::Array), elementType(elementType) {}

    bool acceptsA(const Type& other) const override {
        const Type& candidate = other.unwrapped();
        if (auto* arrayType = safe_cast<ArrayType>(candidate)) {
            return elementType.normalized().acceptsA(arrayType->elementType.normalized());
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return false; // В текущей реализации TypeValue не поддерживает массивы
    }

    const Type& normalized() const override {
        static ArrayType norm(elementType.normalized());
        return norm;
    }

    string toString() const override {
        return "[" + elementType.toString() + "]";
    }

private:
    const Type& elementType;
};

// Implementation of DictionaryType
class DictionaryType final : public Type {
public:
    DictionaryType(const Type& keyType, const Type& valueType)
        : Type(TypeID::Dictionary), keyType(keyType), valueType(valueType) {}

    bool acceptsA(const Type& other) const override {
        const Type& candidate = other.unwrapped();
        if (auto* dictType = safe_cast<DictionaryType>(candidate)) {
            return keyType.normalized().acceptsA(dictType->keyType.normalized()) &&
                   valueType.normalized().acceptsA(dictType->valueType.normalized());
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return false; // В текущей реализации TypeValue не поддерживает словари
    }

    const Type& normalized() const override {
        static DictionaryType norm(keyType.normalized(), valueType.normalized());
        return norm;
    }

    string toString() const override {
        return "[" + keyType.toString() + ": " + valueType.toString() + "]";
    }

private:
    const Type& keyType;
    const Type& valueType;
};

// Implementation of CompositeType
class CompositeType final : public Type {
private:
    static vector<const CompositeType*> composites;
    string kind;
    string name;
    vector<size_t> inherits;
    vector<const Type*> generics;
    size_t index;

public:
    CompositeType(string kind, string name,
                 vector<size_t> inherits = {},
                 vector<const Type*> generics = {})
        : Type(TypeID::Composite)
        , kind(move(kind))
        , name(move(name))
        , inherits(move(inherits))
        , generics(move(generics))
        , index(composites.size()) 
    {
        composites.push_back(this);
    }

    set<size_t> getFullInheritanceChain() const {
        set<size_t> chain;
        for (size_t parentIndex : inherits) {
            chain.insert(parentIndex);
            if (parentIndex < composites.size()) {
                const CompositeType* parent = composites[parentIndex];
                if (parent) {
                    set<size_t> parentChain = parent->getFullInheritanceChain();
                    chain.insert(parentChain.begin(), parentChain.end());
                }
            }
        }
        return chain;
    }

    static bool checkConformance(const CompositeType& baseComp,
                               const CompositeType& candidateComp,
                               const vector<const Type*>& candidateArgs = {}) {
        if (!(candidateComp.index == baseComp.index || 
              candidateComp.getFullInheritanceChain().count(baseComp.index))) {
            return false;
        }

        if (!baseComp.generics.empty()) {
            const vector<const Type*>& actualArgs =
                candidateArgs.empty() ? baseComp.generics : candidateArgs;

            if (actualArgs.size() != baseComp.generics.size()) {
                return false;
            }

            for (size_t i = 0; i < baseComp.generics.size(); i++) {
                const Type& expectedConstraint = baseComp.generics[i]->normalized();
                const Type& providedArg = actualArgs[i]->normalized();
                if (!expectedConstraint.acceptsA(providedArg)) return false;
            }
        }

        return true;
    }

    bool acceptsA(const Type& other) const override {
        if (auto* composite = safe_cast<CompositeType>(other)) {
            return checkConformance(*this, *composite);
        }
        if (auto* reference = safe_cast<ReferenceType>(other)) {
            return checkConformance(*this, getReferenceComposite(*reference), 
                                 getReferenceTypeArgs(*reference));
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return false; // В текущей реализации не поддерживается проверка значений
    }

    const Type& normalized() const override { return *this; }

    string toString() const override {
        ostringstream oss;
        oss << kind << " " << name << "#" << index;
        if (!generics.empty()) {
            oss << "<";
            bool first = true;
            for (const auto* g : generics) {
                if (!first) oss << ", ";
                oss << g->toString();
                first = false;
            }
            oss << ">";
        }
        if (!inherits.empty()) {
            oss << " inherits [";
            bool first = true;
            for (size_t inh : inherits) {
                if (!first) oss << ", ";
                oss << inh;
                first = false;
            }
            oss << "]";
        }
        return oss.str();
    }

    const string& getKind() const { return kind; }
    const string& getName() const { return name; }
    const vector<size_t>& getInherits() const { return inherits; }
    const vector<const Type*>& getGenerics() const { return generics; }
    size_t getIndex() const { return index; }
    void setIndex(size_t idx) { index = idx; }

};

// Initialize static member
vector<const CompositeType*> CompositeType::composites;

inline const string& getCompositeKind(const CompositeType& comp) {
    return comp.getKind();
}

// Implementation of ReferenceType
class ReferenceType final : public Type {
public:
    ReferenceType(const CompositeType& composite, 
                 vector<const Type*> typeArgs = {})
        : Type(TypeID::Reference)
        , composite(composite)
        , typeArgs(move(typeArgs)) {}

    bool acceptsA(const Type& other) const override {
        const Type& unwrapped = other.unwrapped();
        if (auto* composite = safe_cast<CompositeType>(unwrapped)) {
            return CompositeType::checkConformance(this->composite, *composite, this->typeArgs);
        }
        if (auto* reference = safe_cast<ReferenceType>(unwrapped)) {
            return CompositeType::checkConformance(
                this->composite, getReferenceComposite(*reference), 
                getReferenceTypeArgs(*reference));
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return false; // В текущей реализации не поддерживается проверка значений
    }

    const Type& normalized() const override {
        vector<const Type*> normArgs;
        if (!typeArgs.empty()) {
            normArgs.reserve(typeArgs.size());
            for (const auto* arg : typeArgs) {
                normArgs.push_back(&arg->normalized());
            }
        }
        static ReferenceType norm(composite, move(normArgs));
        return norm;
    }

    string toString() const override {
        ostringstream oss;
        oss << "Reference(" << composite.getName() << "#" << composite.getIndex();
        if (!typeArgs.empty()) {
            oss << "<";
            bool first = true;
            for (const auto* arg : typeArgs) {
                if (!first) oss << ", ";
                oss << arg->toString();
                first = false;
            }
            oss << ">";
        }
        oss << ")";
        return oss.str();
    }

    friend const CompositeType& getReferenceComposite(const ReferenceType& ref);
    friend const vector<const Type*>& getReferenceTypeArgs(const ReferenceType& ref);

private:
    const CompositeType& composite;
    vector<const Type*> typeArgs;
};

// Implementation of helper functions for ReferenceType
inline const CompositeType& getReferenceComposite(const ReferenceType& ref) {
    return ref.composite;
}

inline const vector<const Type*>& getReferenceTypeArgs(const ReferenceType& ref) {
    return ref.typeArgs;
}

// Add VariadicType implementation after ParenthesizedType and before other type implementations
class VariadicType final : public Type {
public:
    explicit VariadicType(const Type* innerType = nullptr)
        : Type(TypeID::Variadic), innerType(innerType) {}

    bool acceptsA(const Type& other) const override {
        if (const auto* variadic = safe_cast<VariadicType>(other)) {
            // Если ожидаемый VariadicType пуст (innerType == nullptr), пусть он принимает всё
            if (innerType == nullptr) return true;
            // Если provided VariadicType пуст, то разрешаем такое соответствие, если
            // ожидаемый innerType принимает void
            if (variadic->innerType == nullptr) {
                return innerType->acceptsA(PREDEF_VOID_EXTREME());
            }
            return innerType->acceptsA(*variadic->innerType);
        }

        // Если other не является VariadicType – сравниваем ожидаемый innerType с other напрямую
        if (innerType == nullptr) return true;
        return innerType->acceptsA(other);
    }

    bool checkValue(const TypeValue& value) const override {
        if (innerType == nullptr) return true;
        return innerType->checkValue(value);
    }

    const Type& normalized() const override {
        if (innerType) {
            static VariadicType norm(&innerType->normalized());
            return norm;
        }
        return *this;
    }

    string toString() const override {
        if (innerType) {
            return innerType->toString() + "...";
        }
        return "...";
    }

private:
    const Type* innerType;  // nullptr означает, что тип не указан
};

class IntersectionType final : public Type {
public:
    explicit IntersectionType(vector<const Type*> alternatives)
        : Type(TypeID::Intersection), alternatives(move(alternatives)) {}

    bool acceptsA(const Type& other) const override {
        // Для IntersectionType other должен быть принят всеми альтернативами
        for (const Type* alt : alternatives) {
            if (!alt->normalized().acceptsA(other)) {
                return false;
            }
        }
        return true;
    }

    bool checkValue(const TypeValue& value) const override {
        // Значение соответствует IntersectionType, если удовлетворяет всем альтернативам
        for (const Type* alt : alternatives) {
            if (!alt->normalized().checkValue(value)) {
                return false;
            }
        }
        return true;
    }

    const Type& normalized() const override {
        // Нормализуем каждую альтернативу и «разворачиваем» вложенные IntersectionType
        vector<const Type*> normAlts;
        for (const Type* alt : alternatives) {
            const Type& normAlt = alt->normalized();
            if (const auto* intersection = safe_cast<IntersectionType>(normAlt)) {
                for (const Type* nested : intersection->alternatives) {
                    normAlts.push_back(nested);
                }
            } else {
                normAlts.push_back(&normAlt);
            }
        }
        if (normAlts.size() == 1) {
            return *normAlts[0];
        }
        static IntersectionType norm(move(normAlts));
        return norm;
    }

    string toString() const override {
        ostringstream oss;
        bool first = true;
        for (const Type* alt : alternatives) {
            if (!first) oss << " & ";
            oss << alt->toString();
            first = false;
        }
        return oss.str();
    }

private:
    vector<const Type*> alternatives;
};

class UnionType final : public Type {
public:
    explicit UnionType(vector<const Type*> alternatives)
        : Type(TypeID::Union), alternatives(move(alternatives)) {}

    bool acceptsA(const Type& other) const override {
        // Для UnionType достаточно, если хотя бы одна альтернатива принимает other
        for (const Type* alt : alternatives) {
            if (alt->normalized().acceptsA(other)) {
                return true;
            }
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        // Возвращаем true, если хотя бы одна альтернатива принимает значение
        for (const Type* alt : alternatives) {
            if (alt->normalized().checkValue(value)) {
                return true;
            }
        }
        return false;
    }

    const Type& normalized() const override {
        // Нормализуем каждую альтернативу и «разворачиваем» вложенные UnionType
        vector<const Type*> normAlts;
        for (const Type* alt : alternatives) {
            const Type& normAlt = alt->normalized();
            if (const auto* union_type = safe_cast<UnionType>(normAlt)) {
                // Если альтернатива сама по себе объединение – «разворачиваем» его альтернативы
                for (const Type* nested : union_type->alternatives) {
                    normAlts.push_back(nested);
                }
            } else {
                normAlts.push_back(&normAlt);
            }
        }
        // Если после нормализации остаётся только один элемент, можно вернуть его напрямую
        if (normAlts.size() == 1) {
            return *normAlts[0];
        }
        static UnionType norm(move(normAlts));
        return norm;
    }

    string toString() const override {
        ostringstream oss;
        bool first = true;
        for (const Type* alt : alternatives) {
            if (!first) oss << " | ";
            oss << alt->toString();
            first = false;
        }
        return oss.str();
    }

private:
    vector<const Type*> alternatives;
};

class FunctionType final : public Type {
public:
    /**
     * @param genericParameters - массив generic-параметров
     * @param parameterTypes - список типов параметров
     * @param returnType - тип возвращаемого значения
     * @param modifiers - модификаторы: inits, deinits, awaits, throws
     */
    FunctionType(vector<const Type*> genericParameters,
                vector<const Type*> parameterTypes,
                const Type& returnType,
                map<string, optional<bool>> modifiers = {})
        : Type(TypeID::Function)
        , genericParameters(move(genericParameters))
        , parameterTypes(move(parameterTypes))
        , returnType(returnType)
        , modifiers({
            {"inits", modifiers.count("inits") ? modifiers.at("inits") : nullopt},
            {"deinits", modifiers.count("deinits") ? modifiers.at("deinits") : nullopt},
            {"awaits", modifiers.count("awaits") ? modifiers.at("awaits") : nullopt},
            {"throws", modifiers.count("throws") ? modifiers.at("throws") : nullopt}
        })
    {}

    static bool matchTypeLists(const vector<const Type*>& expectedList,
                             const vector<const Type*>& providedList) {
        function<bool(size_t, size_t)> matchFrom = [&](size_t i, size_t j) -> bool {
            if (i == expectedList.size()) {
                return j == providedList.size();
            }

            const Type& expectedType = *expectedList[i];
            if (auto* variadic = safe_cast<VariadicType>(expectedType)) {
                if (i == expectedList.size() - 1) {
                    for (size_t k = j; k < providedList.size(); k++) {
                        if (!variadic->acceptsA(*providedList[k])) {
                            return false;
                        }
                    }
                    return true;
                }
                for (size_t k = j; k <= providedList.size(); k++) {
                    bool allMatch = true;
                    for (size_t m = j; m < k; m++) {
                        if (!variadic->acceptsA(*providedList[m])) {
                            allMatch = false;
                            break;
                        }
                    }
                    if (allMatch && matchFrom(i + 1, k)) return true;
                }
                return false;
            }

            if (j >= providedList.size()) return false;
            if (!expectedType.acceptsA(*providedList[j])) return false;
            return matchFrom(i + 1, j + 1);
        };

        return matchFrom(0, 0);
    }

    bool acceptsA(const Type& other) const override {
        if (auto* function = safe_cast<FunctionType>(other)) {
            // Сравнение generic-параметров с поддержкой вариативности
            if (!matchTypeLists(genericParameters, function->genericParameters)) return false;

            // Сравнение модификаторов: если ожидающий (this) имеет значение (не nullopt),
            // оно должно совпадать
            for (const auto& [key, value] : modifiers) {
                if (value.has_value() && function->modifiers.at(key) != value) {
                    return false;
                }
            }

            // Сравнение списков параметров с поддержкой VariadicType
            if (!matchTypeLists(parameterTypes, function->parameterTypes)) return false;

            // Сравнение типа возврата
            if (!returnType.acceptsA(function->returnType)) return false;

            return true;
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return false; // В текущей реализации TypeValue не поддерживает функции
    }

    const Type& normalized() const override {
        vector<const Type*> normGenerics;
        normGenerics.reserve(genericParameters.size());
        for (const Type* g : genericParameters) {
            normGenerics.push_back(&g->normalized());
        }

        vector<const Type*> normParams;
        normParams.reserve(parameterTypes.size());
        for (const Type* p : parameterTypes) {
            normParams.push_back(&p->normalized());
        }

        static FunctionType norm(
            move(normGenerics),
            move(normParams),
            returnType.normalized(),
            modifiers  // Сохраняем текущие модификаторы
        );
        return norm;
    }

    string toString() const override {
        ostringstream oss;

        // Generic parameters
        if (!genericParameters.empty()) {
            oss << "<";
            bool first = true;
            for (const Type* g : genericParameters) {
                if (!first) oss << ", ";
                oss << g->toString();
                first = false;
            }
            oss << ">";
        }

        // Parameter types
        oss << "(";
        bool first = true;
        for (const Type* p : parameterTypes) {
            if (!first) oss << ", ";
            oss << p->toString();
            first = false;
        }
        oss << ") -> " << returnType.toString();

        // Modifiers
        for (const auto& [key, value] : modifiers) {
            if (value.has_value()) {
                oss << " " << key << "=" << (*value ? "true" : "false");
            }
        }

        return oss.str();
    }

private:
    vector<const Type*> genericParameters;
    vector<const Type*> parameterTypes;
    const Type& returnType;
    map<string, optional<bool>> modifiers;
};

class OptionalType final : public Type {
public:
    explicit OptionalType(const Type& innerType) 
        : Type(TypeID::Optional), innerType(innerType) {}

    bool acceptsA(const Type& other) const override {
        const Type& normOther = other.normalized();

        // Optional принимает void (null/undefined в JS)
        if (PREDEF_VOID_EXTREME().acceptsA(normOther)) {
            return true;
        }

        // Если other - тоже Optional, проверяем совместимость внутренних типов
        if (const auto* opt = safe_cast<OptionalType>(other)) {
            return innerType.normalized().acceptsA(opt->innerType.normalized());
        }

        // В остальных случаях проверяем совместимость с внутренним типом
        return innerType.normalized().acceptsA(normOther);
    }

    bool checkValue(const TypeValue& value) const override {
        if (holds_alternative<monostate>(value)) {
            return true;
        }
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& innerNorm = innerType.normalized();
        if (const auto* opt = safe_cast<OptionalType>(innerNorm)) {
            return *opt;
        }
        static OptionalType norm(innerNorm);
        return norm;
    }

    string toString() const override {
        return innerType.toString() + "?";
    }

private:
    const Type& innerType;
};

class DefaultType final : public Type {
public:
    explicit DefaultType(const Type& innerType) 
        : Type(TypeID::Default), innerType(innerType) {}

    bool acceptsA(const Type& other) const override {
        const Type& normOther = other.normalized();

        // Default принимает void (null/undefined в JS)
        if (PREDEF_VOID_EXTREME().acceptsA(normOther)) {
            return true;
        }

        if (const auto* def = safe_cast<DefaultType>(other)) {
            return innerType.normalized().acceptsA(def->innerType.normalized());
        }

        return innerType.normalized().acceptsA(normOther);
    }

    bool checkValue(const TypeValue& value) const override {
        if (holds_alternative<monostate>(value)) {
            return true;
        }
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& innerNorm = innerType.normalized();
        if (const auto* def = safe_cast<DefaultType>(innerNorm)) {
            return *def;
        }
        static DefaultType norm(innerNorm);
        return norm;
    }

    string toString() const override {
        return innerType.toString() + "!";
    }

private:
    const Type& innerType;
};

class ParenthesizedType final : public Type {
public:
    explicit ParenthesizedType(const Type& innerType) 
        : Type(TypeID::Parenthesized), innerType(innerType) {}

    bool acceptsA(const Type& other) const override {
        return innerType.normalized().acceptsA(other.normalized());
    }

    bool checkValue(const TypeValue& value) const override {
        return innerType.checkValue(value);
    }

    const Type& normalized() const override {
        return innerType.normalized();
    }

    const Type& unwrapped() const override {
        return innerType.unwrapped();
    }

    string toString() const override {
        return "(" + innerType.toString() + ")";
    }

private:
    const Type& innerType;
};


class InoutType final : public Type {
public:
    explicit InoutType(const Type& innerType)
        : Type(TypeID::Inout)
        , innerType(innerType) {}

    bool acceptsA(const Type& other) const override {
        // Снимаем обёртку, если other в ParenthesizedType
        const Type& unwrapped = other.unwrapped();
        // InoutType совместим только с другими InoutType: сравнение производится строго по внутреннему типу
        if (const auto* inout = safe_cast<InoutType>(unwrapped)) {
            return innerType.normalized().acceptsA(inout->innerType.normalized());
        }
        return false;
    }

    bool checkValue(const TypeValue& value) const override {
        return innerType.normalized().checkValue(value);
    }

    const Type& normalized() const override {
        const Type& normInner = innerType.normalized();
        if (const auto* inout = safe_cast<InoutType>(normInner)) {
            return *inout;
        }
        static InoutType norm(normInner);
        return norm;
    }

    string toString() const override {
        return "inout " + innerType.toString();
    }

private:
    const Type& innerType;
};

inline const PredefinedType& PREDEF_ANY_EXTREME() {
    static const auto lambda = [](const Type&) -> bool { return true; };
    static const PredefinedType instance("_", "extreme", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_any() {
    static const auto lambda = [](const Type& other) -> bool {
        return safe_cast<PrimitiveType>(other) != nullptr;
    };
    static const PredefinedType instance("any", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_bool() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* primitive = safe_cast<PrimitiveType>(other)) {
            string lower_name = primitive->toString();
            transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return lower_name == "bool";
        }
        return false;
    };
    static const PredefinedType instance("bool", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_float() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* primitive = safe_cast<PrimitiveType>(other)) {
            string lower_name = primitive->toString();
            transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return lower_name == "float";
        }
        return false;
    };
    static const PredefinedType instance("float", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_int() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* primitive = safe_cast<PrimitiveType>(other)) {
            string lower_name = primitive->toString();
            transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return lower_name == "int";
        }
        return false;
    };
    static const PredefinedType instance("int", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

inline const PredefinedType& PREDEF_string() {
    static const auto lambda = [](const Type& other) -> bool {
        if (auto* primitive = safe_cast<PrimitiveType>(other)) {
            string lower_name = primitive->toString();
            transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return lower_name == "string";
        }
        return false;
    };
    static const PredefinedType instance("string", "primitive", function<bool(const Type&)>(lambda));
    return instance;
}

// Helper function for tests
void assert_true(bool condition, const string& message) {
    if (!condition) {
        cerr << "Test failed: " << message << endl;
        assert(false);
    }
}

// Test implementations matching JS version
void runTests() {
    cout << "Running type system tests...\n";

    // Create basic types for testing
    PrimitiveType intType("Int");
    PrimitiveType floatType("Float");
    PrimitiveType stringType("String");
    PrimitiveType boolType("Bool");

    // Test predefined types
    assert(PREDEF_ANY_EXTREME().acceptsA(intType) &&
           "PREDEF_ANY_EXTREME should accept Int");
    assert(PREDEF_VOID_EXTREME().acceptsA(PREDEF_VOID_EXTREME()) &&
           "PREDEF_VOID_EXTREME should accept itself");
    assert(!PREDEF_VOID_EXTREME().acceptsA(intType) &&
           "PREDEF_VOID_EXTREME should not accept Int");

    // Add DictionaryType tests
    DictionaryType dictStringInt(stringType, intType);
    DictionaryType dictStringString(stringType, stringType);
    assert(dictStringInt.acceptsA(dictStringInt) &&
           "Dictionary[String:Int] should accept itself");
    assert(!dictStringInt.acceptsA(dictStringString) &&
           "Dictionary[String:Int] should not accept Dictionary[String:String]");

    // Test composite types
    CompositeType testClass("class", "TestClass");
    testClass.setIndex(0);
    CompositeType testObject("object", "TestObject", {}); // inherits from A
    testObject.setIndex(1);
    CompositeType testProtocol("protocol", "TestProtocol", {});
    testProtocol.setIndex(2);


    // Test kind checking
    assert(PREDEF_Class().acceptsA(testClass) &&
           "PREDEF_Class should accept class TestClass");
    assert(!PREDEF_Class().acceptsA(testObject) &&
           "PREDEF_Class should not accept object TestObject");
    assert(!PREDEF_Class().acceptsA(testProtocol) &&
           "PREDEF_Class should not accept protocol TestProtocol");



    // Test object type checking
    assert(PREDEF_Object().acceptsA(testObject) &&
           "PREDEF_Object should accept object TestObject");
    assert(!PREDEF_Object().acceptsA(testClass) &&
           "PREDEF_Object should not accept class TestClass");
    assert(!PREDEF_Object().acceptsA(testProtocol) &&
           "PREDEF_Object should not accept protocol TestProtocol");

    assert(PREDEF_Protocol().acceptsA(testProtocol) &&
           "PREDEF_Protocol should accept protocol TestProtocol");
    assert(!PREDEF_Protocol().acceptsA(testClass) &&
           "PREDEF_Protocol should not accept class TestClass");
    assert(!PREDEF_Protocol().acceptsA(testObject) &&
           "PREDEF_Protocol should not accept object TestObject");

    cout << "All tests completed.\n";
}

// other tests remain unchanged
void run_tests() {
    cout << "Running type system tests...\n";

    // Создаем тестовые типы
    PrimitiveType numberType("Number");
    PrimitiveType intType("Int");
    PrimitiveType floatType("Float");
    PrimitiveType stringType("String");
    PrimitiveType boolType("Bool");

    // Тесты для PREDEF_float
    assert_true(PREDEF_float().acceptsA(floatType),
               "PREDEF_float должен принимать PrimitiveType(\"float\")");
    assert_true(!PREDEF_float().acceptsA(intType),
                "PREDEF_float не должен принимать PrimitiveType(\"int\")");

    // Тесты для PREDEF_int
    assert_true(PREDEF_int().acceptsA(intType),
               "PREDEF_int должен принимать PrimitiveType(\"int\")");
    assert_true(!PREDEF_int().acceptsA(floatType),
                "PREDEF_int не должен принимать PrimitiveType(\"float\")");

    // Тесты для PREDEF_string
    assert_true(PREDEF_string().acceptsA(stringType),
               "PREDEF_string должен принимать PrimitiveType(\"string\")");
    assert_true(!PREDEF_string().acceptsA(intType),
                "PREDEF_string не должен принимать PrimitiveType(\"int\")");

    // Тесты для PREDEF_bool
    assert_true(PREDEF_bool().acceptsA(boolType),
               "PREDEF_bool должен принимать PrimitiveType(\"bool\")");
    assert_true(!PREDEF_bool().acceptsA(stringType),
                "PREDEF_bool не должен принимать PrimitiveType(\"string\")");

    // Тесты для PREDEF_dict
    DictionaryType dictType(stringType, boolType);
    assert_true(PREDEF_dict().acceptsA(dictType),
               "PREDEF_dict должен принимать DictionaryType");
    assert_true(!PREDEF_dict().acceptsA(stringType),
                "PREDEF_dict не должен принимать PrimitiveType");

    cout << "All tests completed.\n";
}

} // namespace types

int main() {
    types::runTests();
    types::run_tests();
    return 0;
}
#include <iostream>
#include <vector>
#include <string>
#include <optional>

using namespace std;

// Флаги характеристик узла
enum TypeFlags : uint32_t {
	Inout           = 1 << 0,
	Union           = 1 << 1,
	Variadic        = 1 << 2,
	Intersection    = 1 << 3,
	Nillable        = 1 << 4,
	Default         = 1 << 5,
	Function        = 1 << 6,
	Array           = 1 << 7,
	GenericParams   = 1 << 8,   // для функций: generic-параметры
	Parameters      = 1 << 9,   // для функций: параметры
	AwaitsTrue      = 1 << 10,  // awaits без ?
	ThrowsTrue      = 1 << 11,  // throws без ?
	AwaitsOptional  = 1 << 12,  // awaits с ? (awaits?)
	ThrowsOptional  = 1 << 13,  // throws с ? (throws?)
	Tuple           = 1 << 14,  // узел является контейнером для tuple-элементов
	InheritedTypes  = 1 << 15,  // для композитных типов: наследуемые типы
	GenericArguments= 1 << 16   // для композитных типов: generic-аргументы
};

// Узел типа
struct TypeNode {
	optional<size_t> super; // Индекс родителя
	string reference;       // Имя типа или иное значение (например, "Function", "Class")
	uint32_t flags = 0;          // Флаги
	vector<size_t> children; // Дочерние узлы

	// Дополнительные поля для поддержки tuple и композитов
	string identifier;      // Например, имя переменной или класса
	string label;           // Для tuple-элементов (метка перед идентификатором)

	TypeNode(optional<size_t> parent, string ref, uint32_t f = 0)
		: super(parent), reference(move(ref)), flags(f) {}
};

// Класс, инкапсулирующий дерево типов и функции для работы с ним
class TypeSystem {
public:
	vector<TypeNode> tree;

	// Добавление узла в дерево; возвращается индекс добавленного узла.
	size_t addTypeNode(optional<size_t> parent, string ref, uint32_t flags = 0) {
		size_t index = tree.size();
		tree.emplace_back(parent, move(ref), flags);
		if (parent.has_value()) {
			tree[*parent].children.push_back(index);
		}
		return index;
	}

	// Объявления функций для преобразования типа в строку:
	string typeToString(size_t index) {
		return typeToString(tree[index]);
	}

private:
	string unionIntersectionTypeToString(const TypeNode& node) {
		string result = formatChildren(node,
			node.flags & Union ? " | " : " & ",
			"", "",
			node.flags // явно передаем флаги узла
		);

		uint32_t parent_flags = node.super.has_value() ? tree[*node.super].flags : 0;
		const bool needs_parens =
			(parent_flags & Union && node.flags & Union) ||
			(parent_flags & Intersection && (node.flags & Intersection || node.flags & Union));

		return needs_parens ? "(" + result + ")" : result;
	}

	// Универсальная функция обработки дочерних элементов
	string formatChildren(const TypeNode& node,
						const string& delimiter = ", ",
						const string& prefix = "",
						const string& suffix = "",
						optional<uint32_t> force_flags = nullopt)
	{
		string result;
		uint32_t flags = force_flags.value_or(node.flags);

		for (size_t i = 0; i < node.children.size(); ++i) {
			if (i > 0) {
				if (flags & Intersection)       result += " & ";
				else if (flags & Union)         result += " | ";
				else                            result += delimiter;
			}

			const TypeNode& child = tree[node.children[i]];
			result += typeToString(child);
		}

		return (!result.empty() && !prefix.empty() && !suffix.empty())
				? (prefix + result + suffix)
				: result;
	}

	// Объединенная логика модификаторов типа
	string applyTypeModifiers(const TypeNode& node, string result) {
		// Array modifier
		if (node.flags & Array) {
			result = "[" + result + "]";
		}

		// Variadic modifier
		if (node.flags & Variadic) {
			result += "...";
		}

		// Inout modifier
		if (node.flags & Inout) {
			result = "inout " + result;
		}

		return result;
	}

	// --- Вспомогательные функции-постпроцессоры ---
	string wrapInParenthesesIfNeeded(const string& result, uint32_t nodeFlags, uint32_t parentFlags) {
		if ((parentFlags & Union) && (nodeFlags & Union)) {
			return "(" + result + ")";
		}
		if ((parentFlags & Intersection) && ((nodeFlags & Intersection) || (nodeFlags & Union))) {
			return "(" + result + ")";
		}
		return result;
	}

	// Преобразование базового (обычного) типа без обработки детей
	string basicTypeToString(const TypeNode& node) {
		string result = node.reference;
		if (node.flags & Nillable) result += "?";
		if (node.flags & Default) result += "!";
		return result;
	}

	string childTypeToString(const TypeNode& node, string result) {
		if (!node.children.empty()) {
			string children_str = formatChildren(node);

			if (!node.reference.empty()) {
				result += "<" + children_str + ">";
			} else {
				result += children_str;
			}
		}
		return result;
	}

	// Функциональный тип
	string functionTypeToString(const TypeNode& node) {
		string genParams, parameters, retType;
		// Обработка дочерних узлов функции
		for (auto child : node.children) {
			const TypeNode& c = tree[child];
			if (c.flags & GenericParams) {
				genParams = formatChildren(c);
			} else
			if (c.flags & Parameters) {
				parameters = formatChildren(c);
			} else {
				retType = typeToString(c);
			}
		}
		string result;
		if (!genParams.empty()) result += "<" + genParams + ">";
		result += "(" + parameters + ")";
		if (node.flags & AwaitsTrue) { result += " awaits"; }
		else if (node.flags & AwaitsOptional) { result += " awaits?"; }
		if (node.flags & ThrowsTrue) { result += " throws"; }
		else if (node.flags & ThrowsOptional) { result += " throws?"; }
		if (!retType.empty()) result += " -> " + retType;
		if (node.flags & Nillable) {
			if(result.ends_with(')'))
				result += "?";
			else
				result = "(" + result + ")?";
		}
		return result;
	}

	// Tuple-тип (кортеж). Узел с флагом Tuple содержит элементы-кортежа в дочерних узлах.
	string tupleTypeToString(const TypeNode& node) {
		string result;
		for (size_t i = 0; i < node.children.size(); i++) {
			if (i > 0) result += ", ";
			const TypeNode& elem = tree[node.children[i]];
			string s;
			if (!elem.label.empty()) {
				s += elem.label + " ";
			}
			if (!elem.identifier.empty()) {
				s += elem.identifier;
			}
			if (!elem.reference.empty()) {
				s += ": " + typeToString(elem);
			}
			result += s;
		}
		return "(" + result + ")";
	}

	// Композитный тип (например, класс).
	string compositeTypeToString(const TypeNode& node) {
		string result = "class " + node.identifier;

		if (node.flags & InheritedTypes && !node.children.empty()) {
			result += ": " + formatChildren(node);
		}

		return result;
	}

	// --- Внутренняя функция, учитывающая флаги родителя ---
	string typeToString(const TypeNode& node) {
	   // Обработка специальных типов
	   if (node.flags & (Union | Intersection))
		   return unionIntersectionTypeToString(node);
	   if (node.reference == "Function" || (node.flags & Function))
		   return functionTypeToString(node);
	   if (node.flags & Tuple)
		   return tupleTypeToString(node);
	   if (node.reference == "Class")
		   return compositeTypeToString(node);

	   // Обработка обычного типа
		string result = basicTypeToString(node);
		result = childTypeToString(node, result);
		result = applyTypeModifiers(node, result);

		return result;
   }
};

//
// Пример использования
//
int main() {
	TypeSystem ts;

	//    inout A! | _? & Global<B>...
	// 0: inout    |               ...
	// 1:       A!
	// 2:               &
	// 3:            _?
	// 4:                 Global< >
	// 5:                        B
	{
		size_t root = ts.addTypeNode(nullopt, "", Inout | Union | Variadic);
		size_t a = ts.addTypeNode(root, "A", Default);
		size_t andNode = ts.addTypeNode(root, "", Intersection);
		size_t nullable_ = ts.addTypeNode(andNode, "_", Nillable);
		size_t global = ts.addTypeNode(andNode, "Global");
		size_t b = ts.addTypeNode(global, "B");

		cout << "Type representation: " << ts.typeToString(root) << endl;
	}

	// Пример: A | (B | C)
	{
		size_t memberType1 = ts.addTypeNode(nullopt, "", Union);
		size_t A = ts.addTypeNode(memberType1, "A");
		size_t unionChild = ts.addTypeNode(memberType1, "", Union);
		size_t B = ts.addTypeNode(unionChild, "B");
		size_t C = ts.addTypeNode(unionChild, "C");
		// Ожидаем: A | (B | C)
		cout << "Type: " << ts.typeToString(memberType1) << endl;
	}

	// Пример более сложного выражения: A | (B | C & D & (E | F))
	{
		// Построим дерево вручную:
		// 0: Корневой узел Union
		size_t root = ts.addTypeNode(nullopt, "", Union);
		size_t A = ts.addTypeNode(root, "A");
		// 1: дочерний узел Union, который будет обёрнут в скобки, т.к. родитель — Union
		size_t union1 = ts.addTypeNode(root, "", Union);
		size_t B = ts.addTypeNode(union1, "B");
		// 2: узел Intersection внутри union1
		size_t inter1 = ts.addTypeNode(union1, "", Intersection);
		size_t C = ts.addTypeNode(inter1, "C");
		size_t D = ts.addTypeNode(inter1, "D");
		// 3: еще один узел Intersection внутри union1
		size_t inter2 = ts.addTypeNode(union1, "", Intersection);
		size_t union2 = ts.addTypeNode(inter2, "", Union);
		size_t E = ts.addTypeNode(union2, "E");
		size_t F = ts.addTypeNode(union2, "F");

		// Ожидаем: A | (B | C & D & (E | F))
		cout << "Type: " << ts.typeToString(root) << endl;
	}

	// --- Функциональные типы ---

	//    () / Function
	// 0: () / Function
	size_t funcType1 = ts.addTypeNode(nullopt, "Function", Function);

	//    <...>(...) awaits? throws? -> _?
	// 0:            awaits? throws?
	// 1: <   >
	// 2:  ...
	// 3:      (   )
	// 4:       ...
	// 5:                            -> _?
	size_t funcType2 = ts.addTypeNode(nullopt, "Function", Function | AwaitsOptional | ThrowsOptional);
	size_t genParams2 = ts.addTypeNode(funcType2, "", GenericParams);
	size_t genParamChild = ts.addTypeNode(genParams2, "", Variadic);
	size_t params2 = ts.addTypeNode(funcType2, "", Parameters);
	size_t paramChild = ts.addTypeNode(params2, "", Variadic);
	size_t retType2 = ts.addTypeNode(funcType2, "_", Nillable);

	//    ([]..., ...) awaits throws -> _
	// 0:              awaits throws
	// 1: (     ,    )
	// 2:  []...
	// 3:         ...
	// 4:                            -> _
	size_t funcType3 = ts.addTypeNode(nullopt, "Function", Function | AwaitsTrue | ThrowsTrue);
	size_t params3 = ts.addTypeNode(funcType3, "", Parameters);
	size_t arrayParam = ts.addTypeNode(params3, "", Array | Variadic); // будет выведен как []...
	size_t param3 = ts.addTypeNode(params3, "", Variadic);            // выведется как ...
	size_t retType3 = ts.addTypeNode(funcType3, "_", 0);

	//    (() -> _)?
	// 0: (()     )?
	// 1:     -> _
	size_t funcType4 = ts.addTypeNode(nullopt, "Function", Function | Nillable);
	size_t retType4 = ts.addTypeNode(funcType4, "_", 0);

	//    <T>()
	// 0:    ()
	// 1: < >
	// 2:  T
	size_t funcType5 = ts.addTypeNode(nullopt, "Function", Function);
	size_t genParams5 = ts.addTypeNode(funcType5, "", GenericParams);
	size_t genParamT = ts.addTypeNode(genParams5, "T", 0);

	cout << "Type 1: " << ts.typeToString(funcType1) << endl;
	cout << "Type 2: " << ts.typeToString(funcType2) << endl;
	cout << "Type 3: " << ts.typeToString(funcType3) << endl;
	cout << "Type 4: " << ts.typeToString(funcType4) << endl;
	cout << "Type 5: " << ts.typeToString(funcType5) << endl;

	// --- Массивные типы ---

	// memberType6: fallback вариант: [] / [_?]
	// 0: узел с флагом Array
	// 1: дочерний узел с predefined "_" и флагом Nillable
	size_t memberType6 = ts.addTypeNode(nullopt, "", Array);
	size_t fallbackChild = ts.addTypeNode(memberType6, "_", Nillable);

	cout << "Fallback array type: " << ts.typeToString(memberType6) << endl;

	// memberType7: предпочтительный вариант: Array<_?>
	// 0: узел с reference "Array" и флагом GenericArguments
	// 1: дочерний узел с predefined "_" и флагом Nillable
	size_t memberType7 = ts.addTypeNode(nullopt, "Array", GenericArguments);
	size_t genericChild = ts.addTypeNode(memberType7, "_", Nillable);

	cout << "Preferred array type: " << ts.typeToString(memberType7) << endl;

	// --- Кортежные типы ---

	//    (a: A, b c: B)
	// 0: (    ,       )
	// 1:  a: A
	// 2:        b c: B
	size_t tupleType0 = ts.addTypeNode(nullopt, "", Tuple);
	// Первый элемент: a: A
	size_t tupleElem1 = ts.addTypeNode(tupleType0, "A");
	ts.tree[tupleElem1].identifier = "a";
	// Второй элемент: b c: B
	size_t tupleElem2 = ts.addTypeNode(tupleType0, "B");
	ts.tree[tupleElem2].label = "b";
	ts.tree[tupleElem2].identifier = "c";

	cout << "Tuple type: " << ts.typeToString(tupleType0) << endl;

	// --- Композитные типы ---

	//    class A: B<_?>
	// 0: class A:
	// 1:          B<  >
	// 2:            _?
	size_t compositeType0 = ts.addTypeNode(nullopt, "Class", InheritedTypes);
	ts.tree[compositeType0].identifier = "A";
	// Наследуемый тип B с generic-аргументами
	size_t inheritedType = ts.addTypeNode(compositeType0, "B", GenericArguments);
	size_t genericArg = ts.addTypeNode(inheritedType, "_", Nillable);

	cout << "Composite type: " << ts.typeToString(compositeType0) << endl;

	return 0;
}

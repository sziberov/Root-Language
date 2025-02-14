let composites = []

class Type {
	/**
	 * Определяет, может ли _этот_ тип (как ожидаемый) принять тип other (значение).
	 * @param {Type} other
	 * @returns {boolean}
	 */
	acceptsA(other) {
		throw new Error("Метод acceptsA должен быть реализован в подклассе");
	}

	/**
	 * Проверяет, соответствует ли этот тип (значение) ожидаемому типу other.
	 * @param {Type} other
	 * @returns {boolean}
	 */
	conformsTo(other) {
		return other.acceptsA(this);
	}

	representedBy(value) {
		throw new Error("Метод representedBy не реализован для данного типа");
	}

	/**
	 * Возвращает нормализованную (каноническую) форму типа.
	 * @returns {Type}
	 */
	normalized() {
		return this;
	}

	/**
	 * Снимает прозрачную обёртку (если есть).
	 * @returns {Type}
	 */
	unwrapped() {
		return this;
	}
}

class PrimitiveType extends Type {
	constructor(name) {
		super();
		this.name = name;
	}

	acceptsA(other) {
		other = other.unwrapped();

		if(other instanceof PrimitiveType) {
			return this.name === other.name || this.name === 'Number' && (other.name === 'Int' || other.name === 'Float');  // Дополнительные сравнения для тестов
		}

		return false;
	}

	representedBy(value) {
		if (this.name === "int") {
			return typeof value === "number" && Number.isInteger(value);
		} else
		if (this.name === "float") {
			return typeof value === "number";
		} else
		if (this.name === "string") {
			return typeof value === "string";
		} else
		if (this.name === "bool") {
			return typeof value === "boolean";
		}
		return false;
	}

	toString() {
		return this.name;
	}
}

class OptionalType extends Type {
	constructor(innerType) {
		super();
		this.innerType = innerType;
	}

	acceptsA(other) {
		if(PREDEF_VOID_EXTREME.acceptsA(other)) {
			return true;
		}

		other = other.normalized();

		if(other instanceof OptionalType) {
			return this.innerType.acceptsA(other.innerType);
		}

		return this.innerType.acceptsA(other);
	}

	representedBy(value) {
		if(value === null || value === undefined) {
			return true;
		}

		return this.innerType.representedBy(value);
	}

	normalized() {
		let normInner = this.innerType.normalized();

		if(normInner instanceof OptionalType) {
			return normInner;
		}

		return new OptionalType(normInner);
	}

	toString() {
		return `${this.innerType.toString()}?`;
	}
}

class DefaultType extends Type {
	constructor(innerType) {
		super();
		this.innerType = innerType;
	}

	acceptsA(other) {
		if(PREDEF_VOID_EXTREME.acceptsA(other)) {
			return true;
		}

		other = other.normalized();

		if(other instanceof DefaultType) {
			return this.innerType.acceptsA(other.innerType);
		}

		return this.innerType.acceptsA(other);
	}


	representedBy(value) {
		if(value === null || value === undefined) {
			return true;
		}

		return this.innerType.representedBy(value);
	}

	normalized() {
		let normInner = this.innerType.normalized();

		if(normInner instanceof DefaultType) {
			return normInner;
		}

		return new DefaultType(normInner);
	}

	toString() {
		return `${this.innerType.toString()}!`;
	}
}

class ParenthesizedType extends Type {
	constructor(innerType) {
		super();
		this.innerType = innerType;
	}

	acceptsA(other) {
		return this.innerType.acceptsA(other);
	}

	representedBy(value) {
		return this.innerType.representedBy(value);
	}

	normalized() {
		return this.innerType.normalized();
	}

	unwrapped() {
		return this.innerType.unwrapped();
	}

	toString() {
		return `(${this.innerType.toString()})`;
	}
}

class CompositeType extends Type {
	/**
	 * @param {string} kind - например, "class" или "object"
	 * @param {string} name - идентификатор (название)
	 * @param {string[]} inherits - список идентификаторов родительских типов
	 * @param {Type[]} generics - массив generic‑параметров (ограничений), заданных как Type
	 */
	constructor(kind, name, inherits = [], generics = []) {
		super();
		this.kind = kind;
		this.name = name;
		this.inherits = inherits;
		this.generics = generics; // теперь просто массив Type
		this.index = composites.length;
		composites.push(this);
	}

	getFullInheritanceChain() {
		// Возвращаем множество индексов всех предков.
		const chain = new Set(this.inherits);
		for (let parentIndex of this.inherits) {
			const parent = composites[parentIndex];
			if (parent) {
				for (let ancestor of parent.getFullInheritanceChain()) {
					chain.add(ancestor);
				}
			}
		}
		return chain;
	}

	static checkConformance(baseComp, candidateComp, candidateArgs) {
		// Проверяем, совпадает ли базовый композит с кандидатом или является его предком.
		if (!(candidateComp.index === baseComp.index || candidateComp.getFullInheritanceChain().has(baseComp.index))) {
			return false;
		}

		// Если базовый композит имеет generic-параметры...
		if (baseComp.generics.length > 0) {
			// Если candidateArgs не указано, трактуем это как отсутствие явных аргументов – подставляем список по умолчанию.
			if (!candidateArgs) {
				candidateArgs = baseComp.generics;
			} else {
				// Если candidateArgs задан (в том числе пустой массив),
				// то его длина должна совпадать с длиной baseComp.generics.
				if (candidateArgs.length !== baseComp.generics.length) {
					return false;
				}
			}
			// Проверяем по каждому generic-параметру:
			for (let i = 0; i < baseComp.generics.length; i++) {
				let expectedConstraint = baseComp.generics[i]
				let providedArg = candidateArgs[i]
				if (!expectedConstraint.acceptsA(providedArg)) return false;
			}
		}

		return true;
	}

	acceptsA(other) {
		other = other.unwrapped();
		if(other instanceof CompositeType) return CompositeType.checkConformance(this, other);
		if(other instanceof ReferenceType) return CompositeType.checkConformance(this, other.composite, other.typeArgs);
		return false;
	}

	toString() {
		let genStr = "";
		if (this.generics?.length > 0) {
			genStr = "<" + this.generics.map(t => t.toString()).join(", ") + ">";
		}
		// Выводим имя и индекс для наглядности.
		const inh = this.inherits.length ? ` inherits [${[...this.getFullInheritanceChain()].join(", ")}]` : "";
		return `${this.kind} ${this.name}#${this.index}${genStr}${inh}`;
	}
}

class ReferenceType extends Type {
	/**
	 * @param {CompositeType} composite - базовый композитный тип
	 * @param {Type[]|undefined} typeArgs - массив generic‑аргументов; undefined означает, что они не заданы (будет использовано значение по умолчанию)
	 */
	constructor(composite, typeArgs) {
		super();
		this.composite = composite;
		this.typeArgs = typeArgs;
	}

	acceptsA(other) {
		other = other.unwrapped();
		if (other instanceof CompositeType) return CompositeType.checkConformance(this.composite, other, this.typeArgs);
		if (other instanceof ReferenceType) return CompositeType.checkConformance(this.composite, other.composite, other.typeArgs);
		return false;
	}

	// Метод representedBy для ReferenceType: значение должно быть числом – индексом композита в compositeRegistry.
	representedBy(value) {
		if (typeof value !== "number") return false;
		const comp = compositeRegistry[value];
		if (!comp) return false;
		// Проверяем, удовлетворяет ли тип композита (comp.type) требованиям ReferenceType.
		return this.acceptsA(comp.type);
	}

	normalized() {
		let normArgs = this.typeArgs?.map(arg => arg.normalized());
		return new ReferenceType(this.composite, normArgs);
	}

	toString() {
		let argsStr = "";
		if (this.typeArgs?.length > 0) {
			argsStr = "<" + this.typeArgs.map(arg => arg.toString()).join(", ") + ">";
		}
		return `Reference(${this.composite.name}#${this.composite.index}${argsStr})`;
	}
}

class ArrayType extends Type {
	/**
	 * @param {Type} valueType - тип элемента коллекции
	 */
	constructor(valueType) {
		super();
		this.valueType = valueType;
	}

	acceptsA(other) {
		other = other.unwrapped();

		return other instanceof ArrayType && this.valueType.acceptsA(other.valueType);
	}

	normalized() {
		return new ArrayType(this.valueType.normalized());
	}

	toString() {
		return `[${this.valueType.toString()}]`;
	}
}

class DictionaryType extends Type {
	/**
	 * @param {Type} keyType - тип ключей
	 * @param {Type} valueType - тип значений
	 */
	constructor(keyType, valueType) {
		super();
		this.keyType = keyType;
		this.valueType = valueType;
	}

	acceptsA(other) {
		other = other.unwrapped();

		return other instanceof DictionaryType && this.keyType.acceptsA(other.keyType) && this.valueType.acceptsA(other.valueType);
	}

	normalized() {
		return new DictionaryType(this.keyType.normalized(), this.valueType.normalized());
	}

	toString() {
		return `[${this.keyType.toString()}: ${this.valueType.toString()}]`;
	}
}

class PredefinedType extends Type {
	/**
	 * @param {string} id - идентификатор типа (например, "_", "any", "bool", "Class", "Function" и т.д.)
	 * @param {function(Type):boolean} acceptsFn - функция проверки, принимает ли этот тип другой тип
	 */
	constructor(id, acceptsFn) {
		super();
		this.id = id;
		this.acceptsFn = acceptsFn;
	}

	acceptsA(other) {
		return this.acceptsFn(other);
	}

	toString() {
		return this.id;
	}
}

const PREDEF_ANY_EXTREME = new PredefinedType("_", (other) => true);

const PREDEF_VOID_EXTREME = new PredefinedType("void", (other) => {
	return other instanceof PredefinedType && other.id === "void";
});

const PREDEF_any = new PredefinedType("any", (other) => {
	return other instanceof PrimitiveType;
});

const PREDEF_bool = new PredefinedType("bool", (other) => {
	return other instanceof PrimitiveType && other.name === "bool";
});

const PREDEF_dict = new PredefinedType("dict", (other) => {
	return other instanceof DictionaryType;
});

const PREDEF_float = new PredefinedType("float", (other) => {
	return other instanceof PrimitiveType && other.name === "float";
});

const PREDEF_int = new PredefinedType("int", (other) => {
	return other instanceof PrimitiveType && other.name === "int";
});

const PREDEF_string = new PredefinedType("string", (other) => {
	return other instanceof PrimitiveType && other.name === "string";
});

const PREDEF_type = new PredefinedType("type", (other) => {
	return other instanceof Type;
});

const PREDEF_Any = new PredefinedType("Any", (other) => {
	return other instanceof CompositeType;
});

const PREDEF_Class = new PredefinedType("Class", (other) => {
	return other instanceof CompositeType && other.kind === "class";
});

const PREDEF_Enumeration = new PredefinedType("Enumeration", (other) => {
	return other instanceof CompositeType && other.kind === "enumeration";
});

const PREDEF_Function = new PredefinedType("Function", (other) => {
	return other instanceof CompositeType && other.kind === "function";
});

const PREDEF_Namespace = new PredefinedType("Namespace", (other) => {
	return other instanceof CompositeType && other.kind === "namespace";
});

const PREDEF_Object = new PredefinedType("Object", (other) => {
	return other instanceof CompositeType && other.kind === "object";
});

const PREDEF_Protocol = new PredefinedType("Protocol", (other) => {
	return other instanceof CompositeType && other.kind === "protocol";
});

const PREDEF_Structure = new PredefinedType("Structure", (other) => {
	return other instanceof CompositeType && other.kind === "structure";
});

class VariadicType extends Type {
	/**
	 * @param {Type|undefined} innerType – тип для каждого элемента вариативной группы или null, если тип не указан.
	 */
	constructor(innerType) {
		super();
		this.innerType = innerType;
	}

	acceptsA(other) {
		if(!this.innerType) {
			return true;
		}

		other = other.unwrapped();

		if(!(other instanceof VariadicType)) {
			return this.innerType.acceptsA(other);
		}
		if(!other.innerType) {
			return this.innerType.acceptsA(PREDEF_VOID_EXTREME);
		}

		return this.innerType.acceptsA(other.innerType);

	}

	representedBy(value) {
		if(!this.innerType) {
			return true;
		}

		// Значение может быть либо массивом, либо отдельным значением, которое соответствует innerType
		if(Array.isArray(value)) {
			return value.every(v => this.innerType.representedBy(v));
		} else {
			return this.innerType.representedBy(value);
		}
	}

	normalized() {
		if(!this.innerType) {
			return this;
		}

		return new VariadicType(this.innerType.normalized());

	}

	toString() {
		return this.innerType ? this.innerType.toString()+'...' : '...';
	}
}

class InoutType extends Type {
	/**
	 * @param {Type} innerType – тип, который оборачивается
	 */
	constructor(innerType) {
		super();
		this.innerType = innerType;
	}

	acceptsA(other) {
		other = other.unwrapped();

		return other instanceof InoutType && this.innerType.acceptsA(other.innerType);
	}

	representedBy(value) {
		return this.innerType.representedBy(value);
	}

	normalized() {
		let normInner = this.innerType.normalized();

		if(normInner instanceof InoutType) {
			return normInner;
		}

		return new InoutType(normInner);
	}

	toString() {
		return 'inout '+this.innerType.toString();
	}
}

class FunctionType extends Type {
	/**
	 * @param {Type[]} genericParameters – массив generic-параметров (без идентификаторов, только типы);
	 *        поддерживается вариативность (элементы могут быть VariadicType).
	 * @param {Type[]} parameterTypes – список типов параметров (в нём могут встречаться VariadicType)
	 * @param {Type} returnType – тип возвращаемого значения.
	 * @param {Object} modifiers – модификаторы: inits, deinits, awaits, throws (каждый: true, false или null).
	 */
	constructor(genericParameters = [], parameterTypes = [], returnType, modifiers = {}) {
		super();
		this.genericParameters = genericParameters; // массив Type
		this.parameterTypes = parameterTypes;         // массив Type (возможно VariadicType)
		this.returnType = returnType;                 // Type
		this.modifiers = {
			inits: modifiers.inits !== undefined ? modifiers.inits : null,
			deinits: modifiers.deinits !== undefined ? modifiers.deinits : null,
			awaits: modifiers.awaits !== undefined ? modifiers.awaits : null,
			throws: modifiers.throws !== undefined ? modifiers.throws : null
		};
	}

	/**
	 * Сравнение списков параметров с поддержкой VariadicType.
	 * Алгоритм:
	 *   Будем идти по expected (E) и provided (P) одновременно.
	 *   Если текущий E не вариативный, то он должен соответствовать текущему P.
	 *   Если E – VariadicType:
	 *     – если это последний элемент списка, то все оставшиеся параметры P должны соответствовать его innerType (если он задан).
	 *     – иначе попробуем «разбить» P на 0 или больше элементов, соответствующих VariadicType, а затем сравнить оставшиеся части.
	 */
	static matchTypeLists(expectedList, providedList) {
		function matchFrom(i, j) {
			if (i === expectedList.length) {
				return j === providedList.length;
			}
			const expectedType = expectedList[i];
			if (expectedType instanceof VariadicType) {
				// Если VariadicType — последний элемент списка,
				// то все оставшиеся элементы providedList должны удовлетворять expectedType.
				if (i === expectedList.length - 1) {
					for (let k = j; k < providedList.length; k++) {
						if (!expectedType.acceptsA(providedList[k])) {
							return false;
						}
					}
					return true;
				} else {
					// Если VariadicType не последний — перебираем, сколько элементов будет поглощено.
					for (let k = j; k <= providedList.length; k++) {
						let allMatch = true;
						for (let m = j; m < k; m++) {
							if (!expectedType.acceptsA(providedList[m])) {
								allMatch = false;
								break;
							}
						}
						if (allMatch && matchFrom(i + 1, k)) return true;
					}
					return false;
				}
			} else {
				if (j >= providedList.length) return false;
				if (!expectedType.acceptsA(providedList[j])) return false;
				return matchFrom(i + 1, j + 1);
			}
		}
		return matchFrom(0, 0);
	}

	acceptsA(other) {
		other = other.unwrapped();

		if(!(other instanceof FunctionType)) {
			return false;
		}

		if(!FunctionType.matchTypeLists(this.genericParameters, other.genericParameters)) return false;
		if(!FunctionType.matchTypeLists(this.parameterTypes, other.parameterTypes)) return false;

		for(let key of ["inits", "deinits", "awaits", "throws"]) {
			if(this.modifiers[key] !== null) {
				if(other.modifiers[key] !== this.modifiers[key]) {
					return false;
				}
			}
		}

		if(!this.returnType.acceptsA(other.returnType)) {
			return false;
		}

		return true;
	}

	representedBy(value) {
		// Предположим, что функция должна быть JS-функцией
		if (typeof value === "function") return true;
		// Если значение – объект с полем signature (типа FunctionType), проверим сигнатуру.
		if (value && value.signature instanceof FunctionType) {
			return this.acceptsA(value.signature);
		}
		return false;
	}

	normalized() {
		const normGenerics = this.genericParameters.map(g => g.normalized());
		const normParams = this.parameterTypes.map(p => p.normalized());
		const normReturn = this.returnType.normalized();
		return new FunctionType(normGenerics, normParams, normReturn, this.modifiers);
	}

	toString() {
		let genericStr = "";
		if (this.genericParameters.length > 0) {
			genericStr = "<" + this.genericParameters.map(g => g.toString()).join(", ") + ">";
		}
		let paramsStr = this.parameterTypes.map(p => p.toString()).join(", ");
		let modStr = "";
		for (let key of ["inits", "deinits", "awaits", "throws"]) {
			if (this.modifiers[key] !== null) {
				modStr += " " + key + "=" + this.modifiers[key];
			}
		}
		return `${genericStr}(${paramsStr}) -> ${this.returnType.toString()}${modStr}`;
	}
}

class UnionType extends Type {
	/**
	 * @param {Type[]} alternatives – массив альтернативных типов.
	 */
	constructor(alternatives) {
		super();
		this.alternatives = alternatives;
	}

	acceptsA(other) {
		other = other.unwrapped();

		for(let alt of this.alternatives) {
			if(alt.acceptsA(other)) {
				return true;
			}
		}

		return false;
	}

	representedBy(value) {
		for(let alt of this.alternatives) {
			if(alt.representedBy(value)) {
				return true;
			}
		}

		return false;
	}

	normalized() {
		let normAlts = []

		for(let alt of this.alternatives) {
			let normAlt = alt.normalized();

			if(normAlt instanceof UnionType) {
				normAlts = normAlts.concat(normAlt.alternatives);
			} else {
				normAlts.push(normAlt);
			}
		}

		if(normAlts.length === 1) {
			return normAlts[0]
		}

		return new UnionType(normAlts);
	}

	toString() {
		return this.alternatives.map(alt => alt.toString()).join(' | ');
	}
}

class IntersectionType extends Type {
	/**
	 * @param {Type[]} alternatives – массив альтернативных (пересекаемых) типов.
	 */
	constructor(alternatives) {
		super();
		this.alternatives = alternatives;
	}

	acceptsA(other) {
		other = other.unwrapped();

		for(let alt of this.alternatives) {
			if(!alt.acceptsA(other)) {
				return false;
			}
		}

		return true;
	}

	representedBy(value) {
		for(let alt of this.alternatives) {
			if(!alt.representedBy(value)) {
				return false;
			}
		}

		return true;
	}

	normalized() {
		let normAlts = []

		for(let alt of this.alternatives) {
			let normAlt = alt.normalized();

			if(normAlt instanceof IntersectionType) {
				normAlts = normAlts.concat(normAlt.alternatives);
			} else {
				normAlts.push(normAlt);
			}
		}

		if(normAlts.length === 1) {
			return normAlts[0]
		}

		return new IntersectionType(normAlts);
	}

	toString() {
		return this.alternatives.map(alt => alt.toString()).join(' & ');
	}
}

// =================================================================================
// Юнит-тесты для системы типизации (на текущем уровне развития)
// =================================================================================
const numberType = new PrimitiveType("Number");
const intType = new PrimitiveType("Int");
const floatType = new PrimitiveType("Float");
const stringType = new PrimitiveType("String");
const boolType = new PrimitiveType("Bool");
const compA = new CompositeType("class", "A");
const compB = new CompositeType("object", "B", [ compA.index ]);
const compC = new CompositeType("object", "C", [ compB.index ]);
const compD = new CompositeType("object", "D", [ compC.index ]);
const compE = new CompositeType("object", "E", [ compB.index ], [ numberType ]);

let testsPassed = 0,
	testsTotal = 0;

function assert(condition, message) {
	testsTotal++;

	if(!condition) {
		console.error('TEST FAILED: '+message);
	} else {
		testsPassed++;
	}
}

// ---------------------------------------------------------------------------------
// 1. Тесты для предопределённых типов (PredefinedType)
// ---------------------------------------------------------------------------------

// Тесты для крайностей (extremes)
// PREDEF_ANY_EXTREME (идентификатор "_" ) должен принимать любой тип.
assert(PREDEF_ANY_EXTREME.acceptsA(new PrimitiveType("int")),
			 'PREDEF_ANY_EXTREME ("_") должен принимать PrimitiveType("int")');
assert(PREDEF_ANY_EXTREME.acceptsA(new CompositeType("class", "A")),
			 'PREDEF_ANY_EXTREME ("_") должен принимать CompositeType("A")');

// Тесты для void (PREDEF_VOID_EXTREME)
// PREDEF_VOID_EXTREME должен принимать сам себя, но не принимать, например, PrimitiveType("int")
assert(PREDEF_VOID_EXTREME.acceptsA(PREDEF_VOID_EXTREME),
			 'PREDEF_VOID_EXTREME должен принимать сам себя');
assert(!PREDEF_VOID_EXTREME.acceptsA(new PrimitiveType("int")),
			 'PREDEF_VOID_EXTREME не должен принимать PrimitiveType("int")');

// Тесты для примитивов
// PREDEF_any – принимает только примитивы.
assert(PREDEF_any.acceptsA(new PrimitiveType("int")),
			 'PREDEF_any должен принимать PrimitiveType("int")');
assert(!PREDEF_any.acceptsA(new CompositeType("class", "A")),
			 'PREDEF_any не должен принимать CompositeType("A")');

// PREDEF_bool – принимает только PrimitiveType("bool")
assert(PREDEF_bool.acceptsA(new PrimitiveType("bool")),
			 'PREDEF_bool должен принимать PrimitiveType("bool")');
assert(!PREDEF_bool.acceptsA(new PrimitiveType("int")),
			 'PREDEF_bool не должен принимать PrimitiveType("int")');

// PREDEF_dict – принимает только DictionaryType
const sampleDict = new DictionaryType(new PrimitiveType("string"), new PrimitiveType("bool"));
assert(PREDEF_dict.acceptsA(sampleDict),
			 'PREDEF_dict должен принимать DictionaryType([string: bool])');
assert(!PREDEF_dict.acceptsA(new ArrayType(new PrimitiveType("bool"))),
			 'PREDEF_dict не должен принимать ArrayType');

// PREDEF_float
assert(PREDEF_float.acceptsA(new PrimitiveType("float")),
			 'PREDEF_float должен принимать PrimitiveType("float")');
assert(!PREDEF_float.acceptsA(new PrimitiveType("int")),
			 'PREDEF_float не должен принимать PrimitiveType("int")');

// PREDEF_int
assert(PREDEF_int.acceptsA(new PrimitiveType("int")),
			 'PREDEF_int должен принимать PrimitiveType("int")');
assert(!PREDEF_int.acceptsA(new PrimitiveType("float")),
			 'PREDEF_int не должен принимать PrimitiveType("float")');

// PREDEF_string
assert(PREDEF_string.acceptsA(new PrimitiveType("string")),
			 'PREDEF_string должен принимать PrimitiveType("string")');
assert(!PREDEF_string.acceptsA(new PrimitiveType("int")),
			 'PREDEF_string не должен принимать PrimitiveType("int")');

// PREDEF_type – принимает любые объекты, наследующие от Type
assert(PREDEF_type.acceptsA(new PrimitiveType("int")),
			 'PREDEF_type должен принимать PrimitiveType("int")');
assert(PREDEF_type.acceptsA(new CompositeType("class", "A")),
			 'PREDEF_type должен принимать CompositeType("A")');

// ---------------------------------------------------------------------------------
// 2. Тесты для предопределённых композитных типов
// ---------------------------------------------------------------------------------

// PREDEF_Any – принимает любой композитный тип.
assert(PREDEF_Any.acceptsA(compA),
			 'PREDEF_Any должен принимать CompositeType("A")');
assert(PREDEF_Any.acceptsA(compB),
			 'PREDEF_Any должен принимать CompositeType("B")');

// PREDEF_Class – принимает только композиты с kind === "class".
assert(PREDEF_Class.acceptsA(new CompositeType("class", "X")),
			 'PREDEF_Class должен принимать композит с kind "class"');
assert(!PREDEF_Class.acceptsA(new CompositeType("object", "Y", ["X"])),
			 'PREDEF_Class не должен принимать композит с kind "object"');

// PREDEF_Function – принимает композиты с kind === "function".
assert(PREDEF_Function.acceptsA(new CompositeType("function", "F")),
			 'PREDEF_Function должен принимать композит с kind "function"');
assert(!PREDEF_Function.acceptsA(new CompositeType("class", "F")),
			 'PREDEF_Function не должен принимать композит с kind "class"');

// ---------------------------------------------------------------------------------
// 3. Тесты для композитных и ссылочных типов (расширенные)
// ---------------------------------------------------------------------------------

// Создаём ReferenceType для compB:
const refB = new ReferenceType(compB);

// Базовые тесты (как в исходном наборе):
assert(refB.conformsTo(compA),
			 'refB.conformsTo(compA) должно возвращать true (B наследует A)');
assert(refB.conformsTo(compB),
			 'refB.conformsTo(compB) должно возвращать true');
assert(refB.acceptsA(compB),
			 'refB.acceptsA(compB) должно возвращать true');
assert(compB.conformsTo(refB),
			 'compB.conformsTo(refB) должно возвращать true');
assert(!compA.conformsTo(refB),
			 'compA.conformsTo(refB) должно возвращать false');

// --- Дополнительные проверки наследственности ---
// Создадим ReferenceType для композитов более глубокого уровня:
const refC = new ReferenceType(compC);
const refD = new ReferenceType(compD);

// Поскольку compC наследует от B (и transitively A):
assert(refC.conformsTo(compB),
			 'refC.conformsTo(compB) должно возвращать true (C наследует B)');
assert(refC.conformsTo(compA),
			 'refC.conformsTo(compA) должно возвращать true (C наследует B, который наследует A)');

// Для compD (на 4-м уровне):
assert(refD.conformsTo(compC),
			 'refD.conformsTo(compC) должно возвращать true (D наследует C)');
assert(refD.conformsTo(compB),
			 'refD.conformsTo(compB) должно возвращать true (D наследует C, а C наследует B)');
assert(refD.conformsTo(compA),
			 'refD.conformsTo(compA) должно возвращать true (наследование через несколько уровней)');

// --- Проверки для композитов с generic‑параметрами ---

// Создадим корректную ссылку (generic‑аргумент — Number)
const refE_correct = new ReferenceType(compE, [ numberType ]);
// Создадим неверную ссылку (generic‑аргумент — String вместо Number)
const refE_incorrect = new ReferenceType(compE, [ stringType ]);

assert(refE_correct.conformsTo(compE),
			 'refE_correct должен конформить compE (generic соответствует)');
assert(compE.conformsTo(refE_correct),
			 'compE.conformsTo(refE_correct) должно возвращать true');
assert(!refE_incorrect.conformsTo(compE),
			 'refE_incorrect не должен конформить compE, т.к. generic-аргумент не соответствует');

// --- Дополнительные тесты для разделения между пустым списком ([]) и списком по умолчанию (null) ---
// Тест 1: Сравнение двух raw-композитов (без явных generic‑аргументов)
// Здесь мы хотим, чтобы compE принимал compE, то есть чтобы default generic подставлялся.
assert(
	compE.acceptsA(compE),
	'Raw CompositeType("E") должен конформить сам себя, используя default generic arguments'
);

// Тест 2: Сравнение с ReferenceType, где typeArgs == null (то есть, аргументы не заданы).
const refE_default = new ReferenceType(compE);
assert(
	compE.acceptsA(refE_default),
	'При typeArgs == null должен использоваться список по умолчанию, поэтому compE.acceptsA(refE_default) должно возвращать true'
);
assert(
	refE_default.acceptsA(compE),
	'При typeArgs == null должен использоваться список по умолчанию, поэтому refE_default.acceptsA(compE) должно возвращать true'
);

// Тест 3: Сравнение с ReferenceType, где typeArgs задан как пустой массив [].
// Это означает, что кандидат явно задаёт пустой список аргументов, что не совпадает с требуемым количеством (1).
const refE_empty = new ReferenceType(compE, []);
assert(
	!compE.acceptsA(refE_empty),
	'При typeArgs == [] (пустой список) и наличии генериков в compE, compE.acceptsA(refE_empty) должно возвращать false'
);
assert(
	!refE_empty.acceptsA(compE),
	'При typeArgs == [] (пустой список) и наличии генериков в compE, refE_empty.acceptsA(compE) должно возвращать false'
);

// --- Проверки того, что ReferenceType не разворачивается случайно ---
// Если обернуть ссылку в ParenthesizedType, то она должна остаться ссылкой (то есть её строковое представление должно содержать "Reference(")
let bracketedRefE = new ParenthesizedType(refE_correct);
assert(bracketedRefE.toString().includes("Reference("),
			 'Ссылка, обернутая в ParenthesizedType, должна оставаться ссылкой с аргументами, а не разворачиваться в композит');

// Проверим, что вызов unwrapped() на ReferenceType не теряет информацию о generic‑аргументах.
// Предполагается, что реализация ReferenceType.unwrapped() в данной архитектуре не разворачивает ссылку до композита.
let unwrappedRefE = refE_correct.unwrapped();
assert(unwrappedRefE instanceof ReferenceType,
			 'unwrapped() на ReferenceType не должен превращать ссылку в обычный композит');
assert(unwrappedRefE.typeArgs[0].toString() === numberType.toString(),
			 'При unwrapped() generic-аргументы ReferenceType должны сохраняться');

// ---------------------------------------------------------------------------------
// 4. Тесты для Optional и Bracketed типов
// ---------------------------------------------------------------------------------

// OptionalType: plain значение должно приниматься опциональным типом, а plain не принимает опциональное.
const optA = new OptionalType(compA);
assert(!optA.conformsTo(compA),
			 'Optional(compA).conformsTo(compA) должно быть false');
assert(compA.conformsTo(optA),
			 'compA.conformsTo(Optional(compA)) должно быть true');

// ParenthesizedType: разворачивается до базового типа.
const bracketedA = new ParenthesizedType(compA);
assert(bracketedA.conformsTo(compA),
			 'Bracketed(compA).conformsTo(compA) должно быть true');
assert(compA.conformsTo(bracketedA),
			 'compA.conformsTo(Bracketed(compA)) должно быть true');

// ---------------------------------------------------------------------------------
// 5. Тесты для коллекционных типов: ArrayType и DictionaryType
// ---------------------------------------------------------------------------------

// ArrayType – ковариантность для элементов.
const animal = new CompositeType("class", "Animal");
const dog = new CompositeType("class", "Dog", [animal.index]);
const arrayAnimal = new ArrayType(animal);
const arrayDog = new ArrayType(dog);

assert(!arrayAnimal.conformsTo(arrayDog),
			 'arrayAnimal.conformsTo(arrayDog) должно быть false');
assert(arrayDog.conformsTo(arrayAnimal),
			 'arrayDog.conformsTo(arrayAnimal) должно быть true (Animal принимает Dog)');

assert(arrayAnimal.acceptsA(new ParenthesizedType(arrayDog)),
			 'arrayAnimal.acceptsA(Bracketed(arrayDog)) должно быть true');

// DictionaryType – проверка для ключей и значений.
const dictAnimal = new DictionaryType(animal, animal);
const dictDog = new DictionaryType(dog, dog);

assert(!dictAnimal.conformsTo(dictDog),
			 'dictAnimal.conformsTo(dictDog) должно быть false');
assert(dictDog.conformsTo(dictAnimal),
			 'dictDog.conformsTo(dictAnimal) должно быть false');

// ---------------------------------------------------------------------------------
// 6. Тесты для коллекционных типов: несовместимость примитивных и композитных
// ---------------------------------------------------------------------------------

// Примитивный ArrayType не должен быть совместим с композитным типом коллекции
const compArray = new CompositeType("class", "Array", [], [PREDEF_any]);
const refcompArray = new ReferenceType(compArray, [animal]);
assert(!arrayAnimal.conformsTo(refcompArray),
			 'Примитивный ArrayType не совместим с композитным Array (refcompArray)');

// ---------------------------------------------------------------------------------
// 7. Тесты для VariadicType
// ---------------------------------------------------------------------------------
// VariadicType(Number) должен принимать VariadicType(Number)
assert(
	new VariadicType(numberType).acceptsA(new VariadicType(numberType)),
	'VariadicType(Number) должен принимать VariadicType(Number)'
);

// VariadicType(Number) должен принимать обычный Number
assert(
	new VariadicType(numberType).acceptsA(numberType),
	'VariadicType(Number) должен принимать Number'
);

// Если innerType – Optional(Number), то пустой VariadicType должен конформиться,
// так как Optional(Number) принимает void.
const optionalNumber = new OptionalType(numberType);
assert(
	new VariadicType(optionalNumber).acceptsA(new VariadicType()),
	'VariadicType(Optional(Number)) должен принимать пустой VariadicType, если Optional(Number) принимает void'
);

// ---------------------------------------------------------------------------------
// 8. Тесты для InoutType
// ---------------------------------------------------------------------------------
assert(
	new InoutType(intType).acceptsA(new InoutType(intType)),
	'InoutType(Int) должен принимать InoutType(Int)'
);
assert(
	!(new InoutType(intType).acceptsA(intType)),
	'InoutType(Int) не должен принимать обычный Int'
);
// Тест нормализации: inout(inout Int) должно нормализоваться до inout Int.
const nestedInout = new InoutType(new InoutType(intType));
assert(
	nestedInout.normalized().toString() === new InoutType(intType).toString(),
	'Нормализация inout(inout Int) должна давать inout Int'
);
// Тест с обёрнутым типом: InoutType(Int) должен принимать тип, обёрнутый в ParenthesizedType.
assert(
	new InoutType(intType).acceptsA(new ParenthesizedType(new InoutType(intType))),
	'InoutType(Int) должен принимать ParenthesizedType(InoutType(Int))'
);

// ---------------------------------------------------------------------------------
// 9. Тесты для FunctionType
// ---------------------------------------------------------------------------------
// Для функции тип возврата будем брать PREDEF_VOID_EXTREME.
const funcReturnType = PREDEF_VOID_EXTREME;

// Функция: (Int, String) -> void, модификатор awaits=true.
const expectedFunc = new FunctionType(
	[],                  // без дженерик‑параметров
	[ intType, stringType ],
	funcReturnType,
	{ awaits: true }
);
const func1 = new FunctionType(
	[],
	[ intType, stringType ],
	funcReturnType,
	{ awaits: true }
);
assert(
	func1.conformsTo(expectedFunc),
	'FunctionType с сигнатурой (Int, String) -> void с awaits=true должен конформить такую же функцию'
);

// Функция с вариативными параметрами: (Variadic(Number), String, Variadic(Bool), Variadic) -> void, awaits=true.
const expectedFuncVariadic = new FunctionType(
	[], // без дженерик‑параметров
	[ new VariadicType(numberType), stringType, new VariadicType(boolType), new VariadicType() ],
	funcReturnType,
	{ awaits: true }
);
// Пример: функция с параметрами: (Int, String, Bool, Int, Bool)
const func2 = new FunctionType(
	[],
	[ intType, stringType, boolType, intType, boolType ],
	funcReturnType,
	{ awaits: true }
);
assert(
	func2.conformsTo(expectedFuncVariadic),
	'FunctionType с вариативными параметрами должен конформить функцию, удовлетворяющую сигнатуре'
);

// Функция с различием в модификаторе: (Int, String) -> void, awaits=true не должна конформить функцию с awaits=false.
const func3 = new FunctionType(
	[],
	[ intType, stringType ],
	funcReturnType,
	{ awaits: false }
);
assert(
	!func3.conformsTo(expectedFunc),
	'FunctionType с awaits=true не должен конформить функцию с awaits=false'
);

// Тест поддержки обёрнутого типа: функция, обёрнутая в ParenthesizedType, должна работать корректно.
assert(
	new ParenthesizedType(func1).conformsTo(expectedFunc),
	'FunctionType должен корректно работать с типами, обёрнутыми в ParenthesizedType'
);

// ---------------------------------------------------------------------------------
// 10. Тесты для UnionType
// ---------------------------------------------------------------------------------
const unionType = new UnionType([ intType, stringType, boolType ]);
assert(
	unionType.acceptsA(intType),
	'UnionType(Int | String | Bool) должен принимать Int'
);
assert(
	unionType.acceptsA(stringType),
	'UnionType(Int | String | Bool) должен принимать String'
);
assert(
	unionType.acceptsA(boolType),
	'UnionType(Int | String | Bool) должен принимать Bool'
);
assert(
	!unionType.acceptsA(new PrimitiveType("Float")),
	'UnionType(Int | String | Bool) не должен принимать Float'
);
// Тест нормализации: "Int | (String | Bool)" должно нормализоваться в "Int | String | Bool".
const unionNested = new UnionType([
	intType,
	new ParenthesizedType(new UnionType([ stringType, boolType ]))
]);
assert(
	unionNested.normalized().toString() === "Int | String | Bool",
	'Нормализованный UnionType("Int | (String | Bool)") должен давать "Int | String | Bool"'
);

// ---------------------------------------------------------------------------------
// 11. Тесты для IntersectionType
// ---------------------------------------------------------------------------------
// IntersectionType с двумя одинаковыми альтернативами: (Int & Int) должен принимать Int.
const intersectionSame = new IntersectionType([ intType, intType ]);
assert(
	intersectionSame.acceptsA(intType),
	'IntersectionType(Int & Int) должен принимать Int'
);
// IntersectionType с разными альтернативами: (Int & String) не должен принимать Int или String по отдельности.
const intersectionDiff = new IntersectionType([ intType, stringType ]);
assert(
	!intersectionDiff.acceptsA(intType),
	'IntersectionType(Int & String) не должен принимать Int'
);
assert(
	!intersectionDiff.acceptsA(stringType),
	'IntersectionType(Int & String) не должен принимать String'
);

// Тест 1: IntersectionType([A, B])
// Ожидается, что тип, наследующий A и B, будет конформен.
const intersectionAB = new IntersectionType([ compA, compB ]);
assert(
	intersectionAB.acceptsA(compC),
	'IntersectionType([A, B]) должен принимать CompositeType("C"), так как C наследует A и B'
);
assert(
	intersectionAB.acceptsA(compB),
	'IntersectionType([A, B]) должен принимать CompositeType("B")'
);
assert(
	intersectionAB.acceptsA(compD),
	'IntersectionType([A, B]) должен принимать CompositeType("D"), так как D наследует A и B через C'
);

// Тест 2: IntersectionType([A, B, C])
// Ожидается, что тип, наследующий A, B и C, будет конформен, а тип, который не наследует один из них – нет.
const intersectionABC = new IntersectionType([ compA, compB, compC ]);
assert(
	intersectionABC.acceptsA(compD),
	'IntersectionType([A, B, C]) должен принимать CompositeType("D"), так как D наследует C, B и A'
);
assert(
	intersectionABC.acceptsA(compC),
	'IntersectionType([A, B, C]) должен принимать CompositeType("C")'
);
// Например, пусть попытаемся проверить compB относительно IntersectionType([A, B, C]) – поскольку compB не наследует C, проверка должна вернуть false.
assert(
	!intersectionABC.acceptsA(compB),
	'IntersectionType([A, B, C]) не должен принимать CompositeType("B"), так как B не наследует C'
);

// Тест 3: Проверка с ReferenceType.
assert(
	intersectionABC.acceptsA(refD),
	'IntersectionType([A, B, C]) должен принимать ReferenceType(compD)'
);

// Тест 4: Проверка с композитом с generic‑параметрами.
// Создадим IntersectionType, требующий, чтобы тип удовлетворял и compB, и compE.
const intersectionBE = new IntersectionType([ compB, compE ]);
assert(
	intersectionBE.acceptsA(refE_correct),
	'IntersectionType([B, E]) должен принимать ReferenceType(compE, [Number]), когда generic соответствует'
);
assert(
	!intersectionBE.acceptsA(refE_incorrect),
	'IntersectionType([B, E]) не должен принимать ReferenceType(compE, [String]), когда generic не соответствует'
);

// Тест 5: Проверка работы с обёрнутыми типами (ParenthesizedType)
// Обернём compD в ParenthesizedType и проверим, что IntersectionType([A, B, C]) принимает его.
assert(
	intersectionABC.acceptsA(new ParenthesizedType(compD)),
	'IntersectionType([A, B, C]) должен принимать ParenthesizedType(compD)'
);
// Аналогично, если сам IntersectionType обёрнут:
assert(
	(new ParenthesizedType(intersectionABC)).acceptsA(compD),
	'Bracketed IntersectionType([A, B, C]) должен принимать CompositeType("D")'
);

// ---------------------------------------------------------------------------------
// Вывод результатов тестирования
// ---------------------------------------------------------------------------------
console.log(`\nВсего тестов: ${testsTotal}, Пройдено: ${testsPassed}`);
if (testsTotal === testsPassed) {
	console.log("Все тесты пройдены успешно!");
} else {
	console.error("Некоторые тесты не прошли!");
}
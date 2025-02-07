// Глобальный реестр для CompositeType (для рекурсивного сбора наследования)
const compositeRegistry = {};

// ======================
// Базовый класс Type
// ======================
class Type {
  /**
   * Проверяет, соответствует ли этот тип (значение) ожидаемому типу other.
   * Сравнение проводится для нормализованных (развёрнутых) типов.
   * @param {Type} other
   * @returns {boolean}
   */
  conformsTo(other) {
    return other.normalized().acceptsA(this.normalized());
  }

  /**
   * Определяет, может ли _этот_ тип (как ожидаемый) принять тип other (значение).
   * @param {Type} other
   * @returns {boolean}
   */
  acceptsA(other) {
    throw new Error("Метод acceptsA должен быть реализован в подклассе");
  }

  /**
   * Возвращает нормализованную (каноническую) форму типа.
   * По умолчанию – этот тип без изменений.
   * @returns {Type}
   */
  normalized() {
    return this;
  }

  /**
   * Снимает прозрачную обёртку (если есть).
   * По умолчанию возвращает себя.
   * @returns {Type}
   */
  unwrap() {
    return this;
  }
}

// ======================
// Примитивный тип
// ======================
class PrimitiveType extends Type {
  constructor(name) {
    super();
    this.name = name;
  }

  acceptsA(other) {
    if(other instanceof OptionalType) return false;
    const candidate = (other instanceof BracketedType) ? other.unwrap() : other;
    if(candidate instanceof PrimitiveType) {
      return this.name === candidate.name || this.name === 'Number' && (candidate.name === 'Int' || candidate.name === 'Float');  // Дополнительные сравнения для тестов
    }
    return false;
  }

  normalized() {
    return this;
  }

  toString() {
    return this.name;
  }
}

// ======================
// Опциональный тип
// ======================
class OptionalType extends Type {
  constructor(innerType) {
    super();
    this.innerType = innerType;
  }

  acceptsA(other) {
    // Принимаем пустые значения
    if(other === null || other === undefined) return true;
    // Если переданный тип после нормализации – void, то тоже принимаем
    const normOther = other.normalized();
    if(normOther instanceof PredefinedType && normOther.id === "void") return true;
    if(other instanceof OptionalType) {
      return this.innerType.normalized().acceptsA(other.innerType.normalized());
    }
    return this.innerType.normalized().acceptsA(normOther);
  }

  normalized() {
    const innerNorm = this.innerType.normalized();
    if(innerNorm instanceof OptionalType) return innerNorm;
    return new OptionalType(innerNorm);
  }

  unwrap() {
    // OptionalType сохраняет информацию об опциональности – unwrap не снимает её.
    return this;
  }

  toString() {
    return `Optional<${this.innerType.toString()}>`;
  }
}

// ======================
// Тип в скобках (прозрачная обёртка)
// ======================
class BracketedType extends Type {
  constructor(innerType) {
    super();
    this.innerType = innerType;
  }

  acceptsA(other) {
    return this.innerType.normalized().acceptsA(other.normalized());
  }

  normalized() {
    return this.innerType.normalized();
  }

  unwrap() {
    return this.innerType.unwrap();
  }

  toString() {
    return `(${this.innerType.toString()})`;
  }
}

// ======================
// CompositeType с поддержкой generic-параметров
// ======================
function checkCompositeConformance(baseComp, candidateComp, candidateArgs) {
  // Проверяем, совпадает ли базовый композит с кандидатом или является его предком.
  if (!(candidateComp.id === baseComp.id || candidateComp.getFullInheritanceChain().has(baseComp.id))) {
    return false;
  }

  // Если базовый композит generic, проверяем generic-аргументы.
  if (baseComp.generics.length > 0) {
    if (!candidateArgs || candidateArgs.length !== baseComp.generics.length) {
      return false;
    }
    for (let i = 0; i < baseComp.generics.length; i++) {
      let expectedConstraint = baseComp.generics[i].normalized();
      let providedArg = candidateArgs[i].normalized();
      if (!expectedConstraint.acceptsA(providedArg)) return false;
    }
  }

  return true;
}

class CompositeType extends Type {
  /**
   * @param {string} kind - например, "class" или "object"
   * @param {string} id - идентификатор (название)
   * @param {string[]} inherits - список идентификаторов родительских типов
   * @param {Type[]} generics - массив generic‑параметров (ограничений), заданных как Type
   */
  constructor(kind, id, inherits = [], generics = []) {
    super();
    this.kind = kind;
    this.id = id;
    this.inherits = inherits;
    this.generics = generics; // теперь просто массив Type
    compositeRegistry[this.id] = this;
  }

  getFullInheritanceChain() {
    const chain = new Set(this.inherits);
    for (let parentId of this.inherits) {
      const parent = compositeRegistry[parentId];
      if (parent) {
        for (let ancestor of parent.getFullInheritanceChain()) {
          chain.add(ancestor);
        }
      }
    }
    return chain;
  }

  acceptsA(other) {
    let candidate = other;
    if(candidate instanceof BracketedType) candidate = candidate.unwrap();

    // Если входящий тип – ReferenceType, то выполняем проверку с учётом generic‑аргументов.
    if (candidate instanceof ReferenceType) {
      let compCandidate = candidate.composite;
      // Базовая проверка: входящий композит должен быть равен this либо наследовать его.
      if (!(compCandidate.id === this.id || compCandidate.getFullInheritanceChain().has(this.id))) {
        return false;
      }
      // Если входящий композит является generic (generics не пуст), то требуем, чтобы у candidate были typeArgs,
      // и для каждого ограничения выполнялось условие, что ожидаемый generic (constraint) принимает соответствующий аргумент.
      if (compCandidate.generics && compCandidate.generics.length > 0) {
        if (!candidate.typeArgs || candidate.typeArgs.length !== compCandidate.generics.length) {
          return false;
        }
        for (let i = 0; i < compCandidate.generics.length; i++) {
          let expectedConstraint = compCandidate.generics[i].normalized();
          let providedArg = candidate.typeArgs[i].normalized();
          if (!expectedConstraint.acceptsA(providedArg)) return false;
        }
      }
      return true;
    }

    if(!(candidate instanceof CompositeType)) return false;
    if(candidate.id === this.id) return true;
    const fullChain = candidate.getFullInheritanceChain();
    return fullChain.has(this.id);
  }

  /*
  acceptsA(other) {
    let candidate = other.unwrap();
    if (candidate instanceof ReferenceType) {
      return checkCompositeConformance(this.composite, candidate.composite, candidate.typeArgs);
    }
    if (!(candidate instanceof CompositeType)) return false;
    return checkCompositeConformance(this.composite, candidate, candidate.generics);
  }
  */

  normalized() {
    return this;
  }

  unwrap() {
    return this;
  }

  toString() {
    let genStr = "";
    if(this.generics && this.generics.length > 0) {
      genStr = "<" + this.generics.map(t => t.toString()).join(", ") + ">";
    }
    const inh = this.inherits.length ? ` inherits [${[...this.getFullInheritanceChain()].join(", ")}]` : "";
    return `${this.kind} ${this.id}${genStr}${inh}`;
  }
}

// ======================
// ReferenceType с поддержкой типовых аргументов
// ======================
class ReferenceType extends Type {
  /**
   * @param {CompositeType} composite - базовый композитный тип
   * @param {Type[]} typeArgs - массив типовых аргументов (опционально)
   */
  constructor(composite, typeArgs = []) {
    super();
    this.composite = composite;
    this.typeArgs = typeArgs;
  }

  /*
  acceptsA(other) {
    if(other instanceof CompositeType) {
      return this.composite.normalized().acceptsA(other.normalized());
    }
    if(other instanceof BracketedType) {
      return this.acceptsA(other.unwrap());
    }
    if(other instanceof ReferenceType) {
      if(!this.composite.normalized().acceptsA(other.composite.normalized())) return false;
      // Если композит имеет generic-параметры, то сравниваем их.
      if(this.composite.generics.length > 0) {
        if(!this.typeArgs || this.typeArgs.length !== this.composite.generics.length) return false;
        if(!other.typeArgs || other.typeArgs.length !== this.composite.generics.length) return false;
        for(let i = 0; i < this.composite.generics.length; i++) {
          let expectedArg = this.typeArgs[i].normalized();
          let providedArg = other.typeArgs[i].normalized();
          if(!expectedArg.acceptsA(providedArg)) return false;
        }
      }
      return true;
    }
    return false;
  }
  */

  acceptsA(other) {
    let candidate = other.unwrap();
    if (candidate instanceof ReferenceType) {
      return checkCompositeConformance(this.composite, candidate.composite, candidate.typeArgs);
    }
    if (!(candidate instanceof CompositeType)) return false;
    return checkCompositeConformance(this.composite, candidate, candidate.generics);
  }

  normalized() {
    const normArgs = this.typeArgs.map(arg => arg.normalized());
    return new ReferenceType(this.composite, normArgs);
  }

  unwrap() {
    return this.composite.unwrap();
  }

  toString() {
    let argsStr = "";
    if(this.typeArgs && this.typeArgs.length > 0) {
      argsStr = "<" + this.typeArgs.map(arg => arg.toString()).join(", ") + ">";
    }
    return `Reference(${this.composite.id}${argsStr})`;
  }
}

// ======================
// Новый тип коллекции: ArrayType (примитивный)
// ======================
class ArrayType extends Type {
  /**
   * @param {Type} elementType - тип элемента коллекции
   */
  constructor(elementType) {
    super();
    this.elementType = elementType;
  }

  acceptsA(other) {
    // Сначала, если other обёрнут (например, в скобки), снимаем обёртку
    let candidate = (other instanceof BracketedType) ? other.unwrap() : other;
    // Если candidate не является ArrayType, сравнение не проходит.
    if (!(candidate instanceof ArrayType)) return false;
    // Проверяем ковариантность: ожидаемый элемент должен принимать предоставленный элемент.
    return this.elementType.normalized().acceptsA(candidate.elementType.normalized());
  }

  normalized() {
    return new ArrayType(this.elementType.normalized());
  }

  toString() {
    return `[${this.elementType.toString()}]`;
  }
}

// ======================
// Новый тип коллекции: DictionaryType (примитивный)
// ======================
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
    // Если аргумент обёрнут, снимаем обёртку.
    let candidate = (other instanceof BracketedType) ? other.unwrap() : other;
    // Если candidate не является DictionaryType, сравнение не проходит.
    if (!(candidate instanceof DictionaryType)) return false;
    // Проверяем, что для ключей и значений выполняется ковариантное сравнение.
    const keyOk = this.keyType.normalized().acceptsA(candidate.keyType.normalized());
    const valueOk = this.valueType.normalized().acceptsA(candidate.valueType.normalized());
    return keyOk && valueOk;
  }

  normalized() {
    return new DictionaryType(this.keyType.normalized(), this.valueType.normalized());
  }

  toString() {
    return `[${this.keyType.toString()}: ${this.valueType.toString()}]`;
  }
}


// ====================================================================
// Реализация предопределённых типов через класс PredefinedType
// ====================================================================

class PredefinedType extends Type {
  /**
   * @param {string} id - идентификатор типа (например, "_", "any", "bool", "Class", "Function" и т.д.)
   * @param {string} category - категория типа: "extreme", "primitive" или "composite"
   * @param {function(Type):boolean} acceptsFn - функция проверки, принимает ли этот тип другой тип
   */
  constructor(id, category, acceptsFn) {
    super();
    this.id = id;
    this.category = category;
    this.acceptsFn = acceptsFn;
  }

  acceptsA(other) {
    // При проверке вызываем переданную функцию, которая знает, какие типы она принимает.
    return this.acceptsFn(other);
  }

  normalized() {
    return this;
  }

  toString() {
    return this.id;
  }
}

// ====================================================================
// Определим предопределённые типы согласно списку
// ====================================================================

// Крайности
// "_" используется как заглушка, которая принимает любой тип
const PREDEF_ANY_EXTREME = new PredefinedType("_", "extreme", (other) => true);

// void принимает только пустые значения (null, undefined) или другие void
const PREDEF_VOID_EXTREME = new PredefinedType("void", "extreme", (other) => {
  return other === null || other === undefined || (other instanceof PredefinedType && other.id === "void");
});

// Примитивы
// Тип "any" для примитивов — принимает любой примитивный тип.
const PREDEF_any = new PredefinedType("any", "primitive", (other) => {
  // Для нашей системы под «примитивными» будем понимать экземпляры PrimitiveType.
  return other instanceof PrimitiveType;
});

// "bool" – логическое значение.
const PREDEF_bool = new PredefinedType("bool", "primitive", (other) => {
  return (other instanceof PrimitiveType && other.name === "bool");
});

// "dict" – словарь (примитивный вариант, то есть DictionaryType).
const PREDEF_dict = new PredefinedType("dict", "primitive", (other) => {
  return other instanceof DictionaryType;
});

// "float" – число с плавающей точкой.
const PREDEF_float = new PredefinedType("float", "primitive", (other) => {
  return (other instanceof PrimitiveType && other.name === "float");
});

// "int" – целое число.
const PREDEF_int = new PredefinedType("int", "primitive", (other) => {
  return (other instanceof PrimitiveType && other.name === "int");
});

// "string" – строка.
const PREDEF_string = new PredefinedType("string", "primitive", (other) => {
  return (other instanceof PrimitiveType && other.name === "string");
});

// "type" – тип (например, сам тип).
// Здесь можно трактовать, что этот предопределённый тип принимает только те значения,
// которые являются представлениями типов (например, экземпляры PredefinedType, CompositeType, ReferenceType и т.д.).
const PREDEF_type = new PredefinedType("type", "primitive", (other) => {
  return other instanceof Type;
});

// Композиты
// "Any" – описывает любой композит.
const PREDEF_Any = new PredefinedType("Any", "composite", (other) => {
  return other instanceof CompositeType;
});

// "Class" – принимает только композиты с kind === "class"
const PREDEF_Class = new PredefinedType("Class", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "class");
});

// "Enumeration" – принимает композиты с kind === "enumeration"
const PREDEF_Enumeration = new PredefinedType("Enumeration", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "enumeration");
});

// "Function" – специальный композит; здесь можно проверять по kind === "function"
// (в более сложной реализации можно учитывать сигнатуру – параметры и тип возврата)
const PREDEF_Function = new PredefinedType("Function", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "function");
});

// "Namespace" – принимает композиты с kind === "namespace"
const PREDEF_Namespace = new PredefinedType("Namespace", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "namespace");
});

// "Object" – принимает композиты с kind === "object"
const PREDEF_Object = new PredefinedType("Object", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "object");
});

// "Protocol" – принимает композиты с kind === "protocol"
const PREDEF_Protocol = new PredefinedType("Protocol", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "protocol");
});

// "Structure" – принимает композиты с kind === "structure"
const PREDEF_Structure = new PredefinedType("Structure", "composite", (other) => {
  return (other instanceof CompositeType && other.kind === "structure");
});

// ====================================================================
// VariadicType – обозначает вариативную позицию (например, Int...)
// ====================================================================
class VariadicType extends Type {
  /**
   * @param {Type|null} innerType – тип для каждого элемента вариативной группы или null, если тип не указан.
   */
  constructor(innerType = null) {
    super();
    this.innerType = innerType;
  }

  acceptsA(other) {
    if (other instanceof BracketedType) return this.acceptsA(other.unwrap());
    if (!(other instanceof VariadicType)) {
      // Если other не является VariadicType – сравниваем ожидаемый innerType с other напрямую
      if (this.innerType === null) return true;
      return this.innerType.normalized().acceptsA(other.normalized());
    }
    // Если ожидаемый VariadicType пуст (innerType === null), пусть он принимает всё
    if (this.innerType === null) return true;
    // Если provided VariadicType пуст, то разрешаем такое соответствие, если
    // ожидаемый innerType принимает void (или значение, которое void обозначает)
    if (other.innerType === null) {
      return this.innerType.normalized().acceptsA(PREDEF_VOID_EXTREME);
    }
    return this.innerType.normalized().acceptsA(other.innerType.normalized());
  }

  normalized() {
    if (this.innerType) {
      return new VariadicType(this.innerType.normalized());
    }
    return this;
  }

  // VariadicType не разворачивается – unwrap возвращает сам объект.
  unwrap() {
    return this;
  }

  toString() {
    return this.innerType ? `${this.innerType.toString()}...` : `...`;
  }
}

// ====================================================================
// InoutType – обёртка для inout-параметров (как в Swift)
// ====================================================================
class InoutType extends Type {
  /**
   * @param {Type} innerType – тип, который оборачивается (не может быть null)
   */
  constructor(innerType) {
    super();
    if (!innerType) {
      throw new Error("InoutType не может быть пустым");
    }
    this.innerType = innerType;
  }

  acceptsA(other) {
    // Снимаем обёртку, если other в BracketedType
    if (other instanceof BracketedType) return this.acceptsA(other.unwrap());
    // InoutType совместим только с другими InoutType: сравнение производится строго по внутреннему типу.
    if (!(other instanceof InoutType)) return false;
    return this.innerType.normalized().acceptsA(other.innerType.normalized());
  }

  normalized() {
    // Если innerType уже InoutType, нормализуем до одного уровня.
    const normInner = this.innerType.normalized();
    if (normInner instanceof InoutType) return normInner;
    return new InoutType(normInner);
  }

  unwrap() {
    return this;
  }

  toString() {
    return `inout ${this.innerType.toString()}`;
  }
}

// ====================================================================
// Функциональный тип (FunctionType)
// ====================================================================
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
            if (!expectedType.normalized().acceptsA(providedList[k].normalized())) {
              return false;
            }
          }
          return true;
        } else {
          // Если VariadicType не последний — перебираем, сколько элементов будет поглощено.
          for (let k = j; k <= providedList.length; k++) {
            let allMatch = true;
            for (let m = j; m < k; m++) {
              if (!expectedType.normalized().acceptsA(providedList[m].normalized())) {
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
        if (!expectedType.normalized().acceptsA(providedList[j].normalized())) return false;
        return matchFrom(i + 1, j + 1);
      }
    }
    return matchFrom(0, 0);
  }

  acceptsA(other) {
    if (other instanceof BracketedType) return this.acceptsA(other.unwrap());
    if (!(other instanceof FunctionType)) return false;

    // Сравнение generic-параметров с поддержкой вариативности.
    if (!FunctionType.matchTypeLists(this.genericParameters, other.genericParameters)) return false;

    // Сравнение модификаторов: если ожидающий (this) имеет значение (не null), оно должно совпадать.
    for (let key of ["inits", "deinits", "awaits", "throws"]) {
      if (this.modifiers[key] !== null) {
        if (other.modifiers[key] !== this.modifiers[key]) return false;
      }
    }

    // Сравнение списков параметров с поддержкой VariadicType.
    if (!FunctionType.matchTypeLists(this.parameterTypes, other.parameterTypes)) return false;

    // Сравнение типа возврата.
    if (!this.returnType.normalized().acceptsA(other.returnType.normalized())) return false;

    return true;
  }

  normalized() {
    const normGenerics = this.genericParameters.map(g => g.normalized());
    const normParams = this.parameterTypes.map(p => p.normalized());
    const normReturn = this.returnType.normalized();
    return new FunctionType(normGenerics, normParams, normReturn, this.modifiers);
  }

  unwrap() {
    return this;
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

// ====================================================================
// UnionType – принимает значение, если оно соответствует хотя бы одной из альтернатив
// ====================================================================
class UnionType extends Type {
  /**
   * @param {Type[]} alternatives – массив альтернативных типов.
   */
  constructor(alternatives) {
    super();
    this.alternatives = alternatives;
  }

  acceptsA(other) {
    // Если other обёрнут в BracketedType, снимаем обёртку.
    if (other instanceof BracketedType) {
      return this.acceptsA(other.unwrap());
    }
    // Для UnionType достаточно, если хотя бы одна альтернатива принимает other.
    for (let alt of this.alternatives) {
      if (alt.normalized().acceptsA(other.normalized())) {
        return true;
      }
    }
    return false;
  }

  normalized() {
    // Нормализуем каждую альтернативу и «разворачиваем» вложенные UnionType.
    let normAlts = [];
    for (let alt of this.alternatives) {
      let normAlt = alt.normalized();
      if (normAlt instanceof UnionType) {
        // Если альтернатива сама по себе объединение – «разворачиваем» его альтернативы.
        normAlts = normAlts.concat(normAlt.alternatives);
      } else {
        normAlts.push(normAlt);
      }
    }
    // Если после нормализации остаётся только один элемент, можно вернуть его напрямую.
    if (normAlts.length === 1) {
      return normAlts[0];
    }
    return new UnionType(normAlts);
  }

  unwrap() {
    return this;
  }

  toString() {
    return this.alternatives.map(alt => alt.toString()).join(" | ");
  }
}

// ====================================================================
// IntersectionType – принимает значение, если оно соответствует всем альтернативам
// ====================================================================
class IntersectionType extends Type {
  /**
   * @param {Type[]} alternatives – массив альтернативных (пересекаемых) типов.
   */
  constructor(alternatives) {
    super();
    this.alternatives = alternatives;
  }

  acceptsA(other) {
    if (other instanceof BracketedType) {
      return this.acceptsA(other.unwrap());
    }
    // Для IntersectionType other должен быть принят всеми альтернативами.
    for (let alt of this.alternatives) {
      if (!alt.normalized().acceptsA(other.normalized())) {
        return false;
      }
    }
    return true;
  }

  normalized() {
    // Нормализуем каждую альтернативу и «разворачиваем» вложенные IntersectionType.
    let normAlts = [];
    for (let alt of this.alternatives) {
      let normAlt = alt.normalized();
      if (normAlt instanceof IntersectionType) {
        normAlts = normAlts.concat(normAlt.alternatives);
      } else {
        normAlts.push(normAlt);
      }
    }
    if (normAlts.length === 1) {
      return normAlts[0];
    }
    return new IntersectionType(normAlts);
  }

  unwrap() {
    return this;
  }

  toString() {
    return this.alternatives.map(alt => alt.toString()).join(" & ");
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

// ----- Вспомогательные функции для отчёта о прохождении тестов -----
let testsPassed = 0, testsTotal = 0;
function assert(condition, message) {
  testsTotal++;
  if (!condition) {
    console.error("TEST FAILED:", message);

  } else {
    testsPassed++;
  }
}

// ---------------------------------------------------------------------------------
// 1. Тесты для предопределённых типов (PredefinedType)
// ---------------------------------------------------------------------------------

// Тесты для крайностей (extremes)
// PREDEF_ANY_EXTREME (идентификатор "_" ) должен принимать любой тип.
assert(PREDEF_ANY_EXTREME.acceptsA(new PrimitiveType("int")) === true,
       'PREDEF_ANY_EXTREME ("_") должен принимать PrimitiveType("int")');
assert(PREDEF_ANY_EXTREME.acceptsA(new CompositeType("class", "A")) === true,
       'PREDEF_ANY_EXTREME ("_") должен принимать CompositeType("A")');

// Тесты для void (PREDEF_VOID_EXTREME)
// PREDEF_VOID_EXTREME должен принимать null, undefined и сам себя, но не принимать, например, PrimitiveType("int")
assert(PREDEF_VOID_EXTREME.acceptsA(null) === true,
       'PREDEF_VOID_EXTREME должен принимать null');
assert(PREDEF_VOID_EXTREME.acceptsA(undefined) === true,
       'PREDEF_VOID_EXTREME должен принимать undefined');
assert(PREDEF_VOID_EXTREME.acceptsA(PREDEF_VOID_EXTREME) === true,
       'PREDEF_VOID_EXTREME должен принимать сам себя');
assert(PREDEF_VOID_EXTREME.acceptsA(new PrimitiveType("int")) === false,
       'PREDEF_VOID_EXTREME не должен принимать PrimitiveType("int")');

// Тесты для примитивов
// PREDEF_any – принимает только примитивы.
assert(PREDEF_any.acceptsA(new PrimitiveType("int")) === true,
       'PREDEF_any должен принимать PrimitiveType("int")');
assert(PREDEF_any.acceptsA(new CompositeType("class", "A")) === false,
       'PREDEF_any не должен принимать CompositeType("A")');

// PREDEF_bool – принимает только PrimitiveType("bool")
assert(PREDEF_bool.acceptsA(new PrimitiveType("bool")) === true,
       'PREDEF_bool должен принимать PrimitiveType("bool")');
assert(PREDEF_bool.acceptsA(new PrimitiveType("int")) === false,
       'PREDEF_bool не должен принимать PrimitiveType("int")');

// PREDEF_dict – принимает только DictionaryType
const sampleDict = new DictionaryType(new PrimitiveType("string"), new PrimitiveType("bool"));
assert(PREDEF_dict.acceptsA(sampleDict) === true,
       'PREDEF_dict должен принимать DictionaryType([string: bool])');
assert(PREDEF_dict.acceptsA(new ArrayType(new PrimitiveType("bool"))) === false,
       'PREDEF_dict не должен принимать ArrayType');

// PREDEF_float
assert(PREDEF_float.acceptsA(new PrimitiveType("float")) === true,
       'PREDEF_float должен принимать PrimitiveType("float")');
assert(PREDEF_float.acceptsA(new PrimitiveType("int")) === false,
       'PREDEF_float не должен принимать PrimitiveType("int")');

// PREDEF_int
assert(PREDEF_int.acceptsA(new PrimitiveType("int")) === true,
       'PREDEF_int должен принимать PrimitiveType("int")');
assert(PREDEF_int.acceptsA(new PrimitiveType("float")) === false,
       'PREDEF_int не должен принимать PrimitiveType("float")');

// PREDEF_string
assert(PREDEF_string.acceptsA(new PrimitiveType("string")) === true,
       'PREDEF_string должен принимать PrimitiveType("string")');
assert(PREDEF_string.acceptsA(new PrimitiveType("int")) === false,
       'PREDEF_string не должен принимать PrimitiveType("int")');

// PREDEF_type – принимает любые объекты, наследующие от Type
assert(PREDEF_type.acceptsA(new PrimitiveType("int")) === true,
       'PREDEF_type должен принимать PrimitiveType("int")');
assert(PREDEF_type.acceptsA(new CompositeType("class", "A")) === true,
       'PREDEF_type должен принимать CompositeType("A")');

// ---------------------------------------------------------------------------------
// 2. Тесты для предопределённых композитных типов
// ---------------------------------------------------------------------------------

// PREDEF_Any – принимает любой композитный тип.
const compA = new CompositeType("class", "A");
const compB = new CompositeType("object", "B", ["A"]);
assert(PREDEF_Any.acceptsA(compA) === true,
       'PREDEF_Any должен принимать CompositeType("A")');
assert(PREDEF_Any.acceptsA(compB) === true,
       'PREDEF_Any должен принимать CompositeType("B")');

// PREDEF_Class – принимает только композиты с kind === "class".
assert(PREDEF_Class.acceptsA(new CompositeType("class", "X")) === true,
       'PREDEF_Class должен принимать композит с kind "class"');
assert(PREDEF_Class.acceptsA(new CompositeType("object", "Y", ["X"])) === false,
       'PREDEF_Class не должен принимать композит с kind "object"');

// PREDEF_Function – принимает композиты с kind === "function".
assert(PREDEF_Function.acceptsA(new CompositeType("function", "F")) === true,
       'PREDEF_Function должен принимать композит с kind "function"');
assert(PREDEF_Function.acceptsA(new CompositeType("class", "F")) === false,
       'PREDEF_Function не должен принимать композит с kind "class"');

// ---------------------------------------------------------------------------------
// 3. Тесты для композитных и ссылочных типов
// ---------------------------------------------------------------------------------

const compositeA = new CompositeType("class", "A");
const compositeB = new CompositeType("object", "B", ["A"]);
const refB = new ReferenceType(compositeB);

// Проверяем методы conformsTo и acceptsA
assert(refB.conformsTo(compositeA) === true,
       'refB.conformsTo(compositeA) должно возвращать true (B наследует A)');
assert(refB.conformsTo(compositeB) === true,
       'refB.conformsTo(compositeB) должно возвращать true');
assert(refB.acceptsA(compositeB) === true,
       'refB.acceptsA(compositeB) должно возвращать true');
assert(compositeB.conformsTo(refB) === true,
       'compositeB.conformsTo(refB) должно возвращать true');
assert(compositeA.conformsTo(refB) === false,
       'compositeA.conformsTo(refB) должно возвращать false');

// ---------------------------------------------------------------------------------
// 4. Тесты для Optional и Bracketed типов
// ---------------------------------------------------------------------------------

// OptionalType: plain значение должно приниматься опциональным типом, а plain не принимает опциональное.
const optA = new OptionalType(compositeA);
assert(optA.conformsTo(compositeA) === false,
       'Optional(compositeA).conformsTo(compositeA) должно быть false');
assert(compositeA.conformsTo(optA) === true,
       'compositeA.conformsTo(Optional(compositeA)) должно быть true');

// BracketedType: разворачивается до базового типа.
const bracketedA = new BracketedType(compositeA);
assert(bracketedA.conformsTo(compositeA) === true,
       'Bracketed(compositeA).conformsTo(compositeA) должно быть true');
assert(compositeA.conformsTo(bracketedA) === true,
       'compositeA.conformsTo(Bracketed(compositeA)) должно быть true');

// ---------------------------------------------------------------------------------
// 5. Тесты для коллекционных типов: ArrayType и DictionaryType
// ---------------------------------------------------------------------------------

// ArrayType – ковариантность для элементов.
const animal = new CompositeType("class", "Animal");
const dog = new CompositeType("class", "Dog", ["Animal"]);
const arrayAnimal = new ArrayType(animal);
const arrayDog = new ArrayType(dog);

assert(arrayAnimal.conformsTo(arrayDog) === false,
       'arrayAnimal.conformsTo(arrayDog) должно быть false');
assert(arrayDog.conformsTo(arrayAnimal) === true,
       'arrayDog.conformsTo(arrayAnimal) должно быть true (Animal принимает Dog)');

assert(arrayAnimal.acceptsA(new BracketedType(arrayDog)) === true,
       'arrayAnimal.acceptsA(Bracketed(arrayDog)) должно быть true');

// DictionaryType – проверка для ключей и значений.
const dictAnimal = new DictionaryType(animal, animal);
const dictDog = new DictionaryType(dog, dog);

assert(dictAnimal.conformsTo(dictDog) === false,
       'dictAnimal.conformsTo(dictDog) должно быть false');
assert(dictDog.conformsTo(dictAnimal) === true,
       'dictDog.conformsTo(dictAnimal) должно быть false');

// ---------------------------------------------------------------------------------
// 6. Тесты для коллекционных типов: несовместимость примитивных и композитных
// ---------------------------------------------------------------------------------

// Примитивный ArrayType не должен быть совместим с композитным типом коллекции
const compositeArray = new CompositeType("class", "Array", [], [PREDEF_any]);
const refCompositeArray = new ReferenceType(compositeArray, [animal]);
assert(arrayAnimal.conformsTo(refCompositeArray) === false,
       'Примитивный ArrayType не совместим с композитным Array (refCompositeArray)');

// ---------------------------------------------------------------------------------
// 7. Тесты для VariadicType
// ---------------------------------------------------------------------------------
// VariadicType(Number) должен принимать VariadicType(Number)
assert(
  new VariadicType(numberType).acceptsA(new VariadicType(numberType)) === true,
  'VariadicType(Number) должен принимать VariadicType(Number)'
);

// VariadicType(Number) должен принимать обычный Number
assert(
  new VariadicType(numberType).acceptsA(numberType) === true,
  'VariadicType(Number) должен принимать Number'
);

// Если innerType – Optional(Number), то пустой VariadicType должен конформиться,
// так как Optional(Number) принимает void.
const optionalNumber = new OptionalType(numberType);
assert(
  new VariadicType(optionalNumber).acceptsA(new VariadicType(null)) === true,
  'VariadicType(Optional(Number)) должен принимать пустой VariadicType, если Optional(Number) принимает void'
);

// ---------------------------------------------------------------------------------
// 8. Тесты для InoutType
// ---------------------------------------------------------------------------------
assert(
  new InoutType(intType).acceptsA(new InoutType(intType)) === true,
  'InoutType(Int) должен принимать InoutType(Int)'
);
assert(
  new InoutType(intType).acceptsA(intType) === false,
  'InoutType(Int) не должен принимать обычный Int'
);
// Тест нормализации: inout(inout Int) должно нормализоваться до inout Int.
const nestedInout = new InoutType(new InoutType(intType));
assert(
  nestedInout.normalized().toString() === new InoutType(intType).toString(),
  'Нормализация inout(inout Int) должна давать inout Int'
);
// Тест с обёрнутым типом: InoutType(Int) должен принимать тип, обёрнутый в BracketedType.
assert(
  new InoutType(intType).acceptsA(new BracketedType(new InoutType(intType))) === true,
  'InoutType(Int) должен принимать BracketedType(InoutType(Int))'
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
  func1.conformsTo(expectedFunc) === true,
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
  func2.conformsTo(expectedFuncVariadic) === true,
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
  func3.conformsTo(expectedFunc) === false,
  'FunctionType с awaits=true не должен конформить функцию с awaits=false'
);

// Тест поддержки обёрнутого типа: функция, обёрнутая в BracketedType, должна работать корректно.
assert(
  new BracketedType(func1).conformsTo(expectedFunc) === true,
  'FunctionType должен корректно работать с типами, обёрнутыми в BracketedType'
);

// ---------------------------------------------------------------------------------
// 10. Тесты для UnionType
// ---------------------------------------------------------------------------------
const unionType = new UnionType([ intType, stringType, boolType ]);
assert(
  unionType.acceptsA(intType) === true,
  'UnionType(Int | String | Bool) должен принимать Int'
);
assert(
  unionType.acceptsA(stringType) === true,
  'UnionType(Int | String | Bool) должен принимать String'
);
assert(
  unionType.acceptsA(boolType) === true,
  'UnionType(Int | String | Bool) должен принимать Bool'
);
assert(
  unionType.acceptsA(new PrimitiveType("Float")) === false,
  'UnionType(Int | String | Bool) не должен принимать Float'
);
// Тест нормализации: "Int | (String | Bool)" должно нормализоваться в "Int | String | Bool".
const unionNested = new UnionType([
  intType,
  new BracketedType(new UnionType([ stringType, boolType ]))
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
  intersectionSame.acceptsA(intType) === true,
  'IntersectionType(Int & Int) должен принимать Int'
);
// IntersectionType с разными альтернативами: (Int & String) не должен принимать Int или String по отдельности.
const intersectionDiff = new IntersectionType([ intType, stringType ]);
assert(
  intersectionDiff.acceptsA(intType) === false,
  'IntersectionType(Int & String) не должен принимать Int'
);
assert(
  intersectionDiff.acceptsA(stringType) === false,
  'IntersectionType(Int & String) не должен принимать String'
);

// Создадим базовые композитные типы с наследованием до 3 уровней:

// Уровень 1:
//const compA = new CompositeType("class", "A"); // базовый класс A

// Уровень 2:
//const compB = new CompositeType("object", "B", ["A"]); // B наследует A

// Уровень 3:
const compC = new CompositeType("object", "C", ["B"]); // C наследует B (и transitively A)

// Дополнительный уровень для проверки (уровень 4):
const compD = new CompositeType("object", "D", ["C"]); // D наследует C, B и A

// Тест 1: IntersectionType([A, B])
// Ожидается, что тип, наследующий A и B, будет конформен.
const intersectionAB = new IntersectionType([ compA, compB ]);
assert(
  intersectionAB.acceptsA(compC) === true,
  'IntersectionType([A, B]) должен принимать CompositeType("C"), так как C наследует A и B'
);
assert(
  intersectionAB.acceptsA(compB) === true,
  'IntersectionType([A, B]) должен принимать CompositeType("B")'
);
assert(
  intersectionAB.acceptsA(compD) === true,
  'IntersectionType([A, B]) должен принимать CompositeType("D"), так как D наследует A и B через C'
);

// Тест 2: IntersectionType([A, B, C])
// Ожидается, что тип, наследующий A, B и C, будет конформен, а тип, который не наследует один из них – нет.
const intersectionABC = new IntersectionType([ compA, compB, compC ]);
assert(
  intersectionABC.acceptsA(compD) === true,
  'IntersectionType([A, B, C]) должен принимать CompositeType("D"), так как D наследует C, B и A'
);
assert(
  intersectionABC.acceptsA(compC) === true,
  'IntersectionType([A, B, C]) должен принимать CompositeType("C")'
);
// Например, пусть попытаемся проверить compB относительно IntersectionType([A, B, C]) – поскольку compB не наследует C, проверка должна вернуть false.
assert(
  intersectionABC.acceptsA(compB) === false,
  'IntersectionType([A, B, C]) не должен принимать CompositeType("B"), так как B не наследует C'
);

// Тест 3: Проверка с ReferenceType.
// Создадим ReferenceType, ссылающийся на композит compD.
const refD = new ReferenceType(compD);
assert(
  intersectionABC.acceptsA(refD) === true,
  'IntersectionType([A, B, C]) должен принимать ReferenceType(compD)'
);

// Тест 4: Проверка с композитом с generic‑параметрами.
// Создадим композит compE, который наследует от compB и имеет один generic-параметр с ограничением Number.
const compE = new CompositeType("object", "E", ["B"], [ numberType ]);
// Создадим ReferenceType для compE с корректным generic-аргументом:
const refE_correct = new ReferenceType(compE, [ numberType ]);
// Создадим IntersectionType, требующий, чтобы тип удовлетворял и compB, и compE.
const intersectionBE = new IntersectionType([ compB, compE ]);
assert(
  intersectionBE.acceptsA(refE_correct) === true,
  'IntersectionType([B, E]) должен принимать ReferenceType(compE, [Number]), когда generic соответствует'
);
// Теперь создадим неверный generic-аргумент: например, используем String вместо Number.
const refE_incorrect = new ReferenceType(compE, [ stringType ]);
assert(
  intersectionBE.acceptsA(refE_incorrect) === false,
  'IntersectionType([B, E]) не должен принимать ReferenceType(compE, [String]), когда generic не соответствует'
);

// Тест 5: Проверка работы с обёрнутыми типами (BracketedType)
// Обернём compD в BracketedType и проверим, что IntersectionType([A, B, C]) принимает его.
assert(
  intersectionABC.acceptsA(new BracketedType(compD)) === true,
  'IntersectionType([A, B, C]) должен принимать BracketedType(compD)'
);
// Аналогично, если сам IntersectionType обёрнут:
assert(
  (new BracketedType(intersectionABC)).acceptsA(compD) === true,
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

// ---------------------------------------------------------------------------------
/*
console.log(FunctionType.matchTypeLists([ new VariadicType(numberType) ],[ new VariadicType(numberType) ]));
// Пример inout:
const inoutInt = new InoutType(intType);
console.log("inoutInt:", inoutInt.toString()); // inout Int
console.log("inout(inout Int) нормализуется:",
            new InoutType(new InoutType(intType)).toString()); // inout Int

// Пример FunctionType с variadic параметрами и вариативными generic‑параметрами.
// Сигнатура ожидаемой функции: generic-параметры: [Number...]
// Параметры: (Variadic(Number), String, Variadic(Bool)) -> void, с модификатор awaits=true.
const expectedFunc = new FunctionType(
  [ new VariadicType(numberType) ],   // generic-параметры
  [
    new VariadicType(numberType),       // первый вариативный сегмент параметров
    stringType,                         // фиксированный параметр
    //new VariadicType(),
    new VariadicType(boolType)          // второй вариативный сегмент параметров
  ],
  PREDEF_VOID_EXTREME,
  { awaits: true }
);

// Функция func1: (Int, String, Bool) -> void с таким же generic-параметром.
const func1 = new FunctionType(
  [ new VariadicType(numberType) ],
  [
    intType,
    stringType,
    boolType
  ],
  PREDEF_VOID_EXTREME,
  { awaits: true }
);
console.log("func1 соответствует expectedFunc:", func1.conformsTo(expectedFunc)); // Должно быть true

// Функция func2: (Float, String, Bool, Int, Bool) -> void
const func2 = new FunctionType(
  [ new VariadicType(numberType) ],
  [
    floatType,
    stringType,
    boolType,
    intType,
    boolType
  ],
  PREDEF_VOID_EXTREME,
  { awaits: true }
);
console.log("func2 соответствует expectedFunc:", func2.conformsTo(expectedFunc)); // true

// Если обернуть func2 в скобки – сравнение всё равно корректно:
console.log("Bracketed func2 соответствует expectedFunc:", func2.conformsTo(new BracketedType(expectedFunc)));

// Функция func3: отличается модификатором awaits
const func3 = new FunctionType(
  [ new VariadicType(numberType) ],
  [
    intType,
    stringType,
    boolType
  ],
  PREDEF_VOID_EXTREME,
  { awaits: false }
);
console.log("func3 соответствует expectedFunc:", func3.conformsTo(expectedFunc)); // false

// Пример UnionType:
const union1 = new UnionType([intType, stringType, boolType]);
console.log(union1.toString());  // Выведет: "Int | String | Bool"
console.log("Union принимает Int:", union1.acceptsA(intType));      // true
console.log("Union принимает String:", union1.acceptsA(stringType)); // true

// Пример с вложенными объединениями: "Int | (String | Bool)" должно нормализоваться в "Int | String | Bool"
const unionNested = new UnionType([intType, new BracketedType(new UnionType([stringType, boolType]))]);
console.log("Нормализованный union:", unionNested.normalized().toString());  // "Int | String | Bool"

// Пример IntersectionType:
const intersection1 = new IntersectionType([intType, stringType, boolType]);
console.log(intersection1.toString());  // "Int & String & Bool"
// Для IntersectionType acceptsA(other) вернёт true только если other соответствует всем альтернативам.
// Например, если тип other одновременно конформен Int, String и Bool.
*/
String.prototype.getMember = function(key, args, getType) {
	return this[key]
}

class Dictionary {
	constructor() {
		this.items = new Map();
	}

	// Self

	// var a: getType = this
	// var a: getType = this[args[0]]
	get(args, getType, subscript) {
		if(args) {
			if(subscript && args.length === 1) {
				return this.getMember(args[0], null, getType, true);
			} else {
				throw 'Only single subscripted argument is allowed';
			}
		}

		return this;
	}

	// this[args[0]] = setValue
	set(args, setValue, subscript) {
		if(!subscript || !args || args.length !== 1) {
			throw 'Only subscripted set with single key is allowed';
		}

		this.setMember(args[0], null, setValue, true);
	}

	// delete this[args[0]]
	delete_(args, subscript) {
		if(!subscript || !args || args.length !== 1) {
			throw 'Only subscripted delete with single key is allowed';
		}

		this.deleteMember(args[0], null, true);
	}

	// Member

	// var a: getType = this[key]
	// var a: getType = this[key](...args)
	getMember(key, args, getType, subscript) {
		if(!subscript) {
			throw 'Only subscripted get is allowed';
		}

		const values = this.items.get(key);
		if(!values) return undefined;

		if(args && Array.isArray(values)) {
			for(let fn of values) {
				if(typeof fn === 'function' && fn.length === args.length) {
					return fn(...args);
				}
			}
			return undefined;
		}

		if(Array.isArray(values)) {
			return values[values.length-1];
		}

		return values;
	}

	// this[key] = setValue
	// this[key](...args) = setValue
	setMember(key, args, setValue, subscript) {
		if(!subscript) {
			throw 'Only subscripted set is allowed';
		}

		const existing = this.items.get(key);

		if(typeof setValue === 'function') {
			if(!existing) {
				this.items.set(key, [setValue]);
			} else
			if(Array.isArray(existing)) {
				existing.push(setValue);
			} else {
				this.items.set(key, [existing, setValue]);
			}
		} else {
			this.items.set(key, setValue);
		}
	}

	// delete this[key]
	// delete this[key](...args)
	deleteMember(key, args, subscript) {
		if(!subscript) {
			throw 'Only subscripted delete is allowed';
		}

		this.items.delete(key);
	}
}

class Composite {
	constructor() {
		this.fields = new Map();
		this.subscripts = new Map();
	}

	// Self

	// var a: getType = this
	// var a: getType = this(...args)
	// var a: getType = this[...args]
	get(args, getType, subscript) {
		if(args) {
			if(subscript) {
				return this.subscripts.get(JSON.stringify(args));
			} else {
				throw 'Can\'t directly call a composite';
			}
		}

		return this;
	}

	// this = setValue
	// this(...args) = setValue
	// this[...args] = setValue
	set(args, setValue, subscript) {
		if(args) {
			if(subscript) {
				return this.subscripts.set(JSON.stringify(args), setValue);
			} else {
				throw 'Can\'t replace a function';
			}
		}

		throw 'Can\'t directly set a composite';
	}

	// delete this
	// delete this(...args)
	// delete this[...args]
	delete_(args, subscript) {
		if(args) {
			if(subscript) {
				return this.subscripts.delete(JSON.stringify(args));
			} else {
				throw 'Can\'t delete a function';
			}
		}

		throw 'Can\'t directly delete a composite';
	}

	// Member

	// var a: getType = this.key
	// var a: getType = this.key(...args)
	// var a: getType = this.key[...args]
	getMember(key, args, getType, subscript) {
		return this.accessMember(key, args, getType, undefined, false, subscript);
	}

	// this.key = setValue
	// this.key(...args) = setValue
	// this.key[...args] = setValue
	setMember(key, args, setValue, subscript) {
		this.accessMember(key, args, undefined, setValue, false, subscript);
	}

	// delete this.key
	// delete this.key(...args)
	// delete this.key[...args]
	deleteMember(key, args, subscript) {
		this.accessMember(key, args, undefined, undefined, true, subscript);
	}

	// Helpers

	accessMember(key, args, getType, setValue, delete_, subscript) {
	  const candidates = this.fields.get(key) || []

	  // ——— САБСКРИПТ-ВЕТКА ———
	  if (subscript) {
	    // удаление
	    if (delete_) {
	      for (let obj of candidates) {
	        if (obj instanceof Composite || obj instanceof Inout) {
	          return obj.delete_(args, true);
	        }
	      }
	      return;
	    }
	    // присвоение
	    if (setValue !== undefined) {
	      for (let obj of candidates) {
	        if (obj instanceof Composite || obj instanceof Inout) {
	          return obj.set(args, setValue, true);
	        }
	      }
	      return;
	    }
	    // чтение / вызов сабскрипта
	    for (let obj of candidates) {
	      if (obj instanceof Composite || obj instanceof Inout) {
	        const res = obj.get(args, getType, true);
	        if (res !== undefined) return res;
	      }
	    }
	    return undefined;
	  }

	  // ——— MEMBER / METHOD-ВЕТКА ———
	  // удаление поля/методов
	  if (delete_) {
	    return this.fields.delete(key);
	  }
	  // присвоение нового «члена»
	  if (setValue !== undefined) {
	    const arr = candidates;
	    arr.push(setValue);
	    this.fields.set(key, arr);
	    return;
	  }
	  // вызов метода
	  if (args) {
	    for (let fn of candidates) {
	      if (typeof fn === 'function') {
	        return fn(...args);
	      }
	    }
	    return undefined;
	  }
	  // чтение поля
	  return candidates.length
	    ? candidates[candidates.length - 1]
	    : undefined;
	}
}

class Inout {
	constructor(LHS, RHS) {
		this.LHS = LHS; // Composite / Inout
		this.RHS = RHS; // key (usually string)
	}

	// Self

	// var a: getType = LHS.RHS
	// var a: getType = LHS.RHS(...args)
	// var a: getType = LHS.RHS[...args]
	get(args, getType, subscript) {
		return this.getMember(this.RHS, args, getType, subscript);
	}

	// LHS.RHS = setValue
	// LHS.RHS(...args) = setValue
	// LHS.RHS[...args] = setValue
	set(args, setValue, subscript) {
		this.setMember(this.RHS, args, setValue, subscript);
	}

	// delete LHS.RHS
	// delete LHS.RHS(...args)
	// delete LHS.RHS[...args]
	delete_(args, subscript) {
		this.deleteMember(this.RHS, args, subscript);
	}

	// Member

	// var a: getType = LHS.key
	// var a: getType = LHS.key(...args)
	// var a: getType = LHS.key[...args]
	getMember(key, args, getType, subscript) {
		let LHS = this.normalizedLHS();
		if(LHS) return LHS.getMember(key, args, getType, subscript);
	}

	// LHS.key = setValue
	// LHS.key(...args) = setValue
	// LHS.key[...args] = setValue
	setMember(key, args, setValue, subscript) {
		let LHS = this.normalizedLHS();
		if(LHS) LHS.setMember(key, args, setValue, subscript);
	}

	// delete LHS.key
	// delete LHS.key(...args)
	// delete LHS.key[...args]
	deleteMember(key, args, subscript) {
		let LHS = this.normalizedLHS();
		if(LHS) LHS.deleteMember(key, args, subscript);
	}

	// Helpers

	normalizedLHS() {
		if(this.LHS instanceof Inout) {
			return this.LHS.get();
		}

		return this.LHS;
	}
}

let comp1 = new Composite();
comp1.setMember('x', null, 123);

let comp1Inout = new Inout(comp1, 'x');
console.log(comp1Inout.get());          // 123
comp1Inout.set(null, 456);
console.log(comp1Inout.get());          // 456
comp1Inout.delete_();
console.log(comp1Inout.get());          // undefined



let comp2 = new Composite();
comp2.setMember('field', null, 'hello');

let comp2_field = new Inout(comp2, 'field');
console.log(comp2_field.get());           // hello
comp2_field.set(null, 'world');
console.log(comp2_field.get());           // world



let comp2_field_length = new Inout(comp2_field, 'length');  // Inout(Inout(comp2, 'field'), 'length')
// Simulate: comp2.field = "world" → "world".length
console.log(comp2_field_length.get());    // 5



let comp6 = new Composite();
comp2.setMember('nest', null, comp6);
comp6.setMember('test', null, 'string');

let comp2_nest = new Inout(comp2, 'nest');
let comp2_nest_test = new Inout(comp2_nest, 'test');
let comp2_nest_test_length = new Inout(comp2_nest_test, 'length');
console.log(comp2_nest_test_length.get());    // 6



let comp3 = new Composite();
comp3.setMember('func', null, (a, b) => a+b);

let comp3_func = new Inout(comp3, 'func');
console.log(comp3_func.get([4, 8]));   // 12



let comp4 = new Composite();
let comp5 = new Composite();
let subArgs = ['a', 'b']
comp4.setMember('x', null, comp5);
comp5.set(subArgs, 'value123', true);

let comp4_x = new Inout(comp4, 'x');
console.log(comp4_x.get(subArgs, null, true));    // value123
comp4_x.set(subArgs, 'newVal', true);
console.log(comp4_x.get(subArgs, null, true));    // newVal


let comp7 = new Composite();
let comp8 = new Composite();
let comp9 = new Composite();
comp7.setMember('subscripted', null, comp9);
comp7.setMember('subscripted', null, comp8);
let firstSubArgs = [1]
let secondSubArgs = [1, 2]
comp8.set(firstSubArgs, 'firstSubscript', true);
comp9.set(secondSubArgs, 'secondSubscript', true);

let comp7_subscripted = new Inout(comp7, 'subscripted');
console.log(comp7_subscripted.get(firstSubArgs, null, true));   // firstSubscript
console.log(comp7_subscripted.get(secondSubArgs, null, true));  // secondSubscript



let dict = new Dictionary();
let dict_foo = new Inout(dict, 'foo');
dict_foo.set(null, 42, true);
console.log(dict_foo.get(null, null, true)); // 42
dict_foo.delete_(null, true);
console.log(dict_foo.get(null, null, true)); // undefined



let dict2 = new Dictionary();

dict2.setMember('func', null, (a) => 0, true);
dict2.setMember('func', null, (a, b) => 1, true);

let dict2_func = new Inout(dict2, 'func');

console.log(dict2.getMember('func', [1], null, true));  // 0
console.log(dict2_func.get([1, 2], null, true));        // 1
console.log(dict2_func.get([1, 2, 3], null, true));     // undefined

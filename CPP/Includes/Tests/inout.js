String.prototype.getMember = function(key, args, getType) {
	return this[key]
}

class Dictionary {
	constructor() {
		this.items = new Map();
	}

	// Self

	// var a: getType = this
	get(args, getType) {
		if(args) {
			throw 'Can\'t directly call a dictionary';
		}

		return this;
	}

	set(setValue) {
		throw 'Can\'t directly set a dictionary';
	}

	delete_() {
		throw 'Can\'t directly delete a dictionary';
	}

	// Member

	// var a: getType = this[key[0]]
	// var a: getType = this[key[0]](...args)
	getMember(key, args, getType) {
		if(!Array.isArray(key) || key.length !== 1) {
			throw 'Only single-argument subscripted get is allowed';
		}

		const values = this.items.get(key[0]);
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

	// this[key[0]] = setValue
	setMember(key, setValue) {
		if(!Array.isArray(key) || key.length !== 1) {
			throw 'Only single-argument subscripted set is allowed';
		}

		const existing = this.items.get(key[0]);

		if(typeof setValue === 'function') {
			if(!existing) {
				this.items.set(key[0], [setValue]);
			} else
			if(Array.isArray(existing)) {
				existing.push(setValue);
			} else {
				this.items.set(key[0], [existing, setValue]);
			}
		} else {
			this.items.set(key[0], setValue);
		}
	}

	// delete this[key[0]]
	deleteMember(key) {
		if(!Array.isArray(key) || key.length !== 1) {
			throw 'Only single-argument subscripted delete is allowed';
		}

		this.items.delete(key[0]);
	}
}

class Composite {
	constructor() {
		this.fields = {}
	}

	// Self

	// var a: getType = this
	get(args, getType) {
		if(args) {
			throw 'Can\'t directly call a composite';
		}

		return this;
	}

	set(setValue) {
		throw 'Can\'t directly set a composite';
	}

	delete_() {
		throw 'Can\'t directly delete a composite';
	}

	// Member

	// var a: getType = this.key
	// var a: getType = this.key(...args)
	// var a: getType = this[...key]
	// var a: getType = this[...key](...args)
	getMember(key, args, getType) {
		return this.accessMember(key, args, getType, undefined, false);
	}

	// this.key = setValue
	// this[...key] = setValue
	setMember(key, setValue) {
		this.accessMember(key, undefined, undefined, setValue, false);
	}

	// delete this.key
	// delete this[...key]
	deleteMember(key) {
		this.accessMember(key, undefined, undefined, undefined, true);
	}

	// Helpers

	accessMember(key, args, getType, setValue, delete_) {
		if(Array.isArray(key)) {
			if(delete_) {
				delete this.fields[JSON.stringify(key)]
				return;
			}

			if(setValue !== undefined) {
				this.fields[JSON.stringify(key)] = setValue;
				return;
			}

			return this.fields[JSON.stringify(key)]
		}

		let candidates = this.fields[key] ?? []

		if(delete_) {
			delete this.fields[key]
			return;
		}

		if(setValue !== undefined) {
			const arr = candidates;
			arr.push(setValue);
			this.fields[key] = arr;
			return;
		}

		if(args) {
			for(let fn of candidates) {
				if(typeof fn === 'function') {
					return fn(...args);
				}
			}
			return;
		}

		for(let i = candidates.length-1; i >= 0; i--) {
			if(!getType || candidates[i] instanceof getType) {
				return candidates[i]
			}
		}
	}
}

class Inout {
	constructor(...path) {
		this.path = this.flatten(path);
	}

	flatten(path) {
		let result = []

		for(let part of path) {
			if(part instanceof Inout) {
				result.push(...part.path);
			} else {
				result.push(part);
			}
		}

		return result;
	}

	// Self

	// var a: getType = path
	// var a: getType = path(...args)
	get(args, getType) {
		let target = this.evaluatedPath(this.path.slice(0, -1));

		if(target) {
			return target.getMember(this.path.at(-1), args, getType);
		}
	}

	// path = setValue
	set(setValue) {
		let target = this.evaluatedPath(this.path.slice(0, -1));

		if(target) {
			return target.setMember(this.path.at(-1), setValue);
		}
	}

	// delete path
	delete_() {
		let target = this.evaluatedPath(this.path.slice(0, -1));

		if(target) {
			return target.deleteMember(this.path.at(-1));
		}
	}

	// Member

	// var a: getType = path.key
	// var a: getType = path.key(...args)
	// var a: getType = path[...key]
	// var a: getType = path[...key](...args)
	getMember(key, args, getType) {
		let target = this.evaluatedPath(this.path);

		if(target) {
			return target.getMember(key, args, getType);
		}
	}

	// path.key = setValue
	// path[...key] = setValue
	setMember(key, setValue) {
		let target = this.evaluatedPath(this.path);

		if(target) {
			target.setMember(key, setValue);
		}
	}

	// delete path.key
	// delete path[...key]
	deleteMember(key) {
		let target = this.evaluatedPath(this.path);

		if(target) {
			target.deleteMember(key);
		}
	}

	// Helpers

	evaluatedPath(path) {
		let target = path[0]

		for(let i = 1; i < path.length; i++) {
			if(!target) {
				break;
			}

			target = target.getMember(path[i]);
		}

		return target;
	}
}

let comp1 = new Composite();
comp1.setMember('x', 123);

let comp1Inout = new Inout(comp1, 'x');
console.log(comp1Inout.get());          // 123
comp1Inout.set(456);
console.log(comp1Inout.get());          // 456
comp1Inout.delete_();
console.log(comp1Inout.get());          // undefined



let comp2 = new Composite();
comp2.setMember('field', 'hello');

let comp2_field = new Inout(comp2, 'field');
console.log(comp2_field.get());           // hello
comp2_field.set('world');
console.log(comp2_field.get());           // world



let comp2_field_length = new Inout(comp2_field, 'length');  // Inout(Inout(comp2, 'field'), 'length')
// comp2.field = "world" -> "world".length
console.log(comp2_field_length.get());    // 5



let comp6 = new Composite();
comp2.setMember('nest', comp6);
comp6.setMember('test', 'string');

let comp2_nest_test_length = new Inout(comp2, 'nest', 'test', 'length');
console.log(comp2_nest_test_length.get());    // 6



let comp3 = new Composite();
comp3.setMember('func', (a, b) => a+b);

let comp3_func = new Inout(comp3, 'func');
console.log(comp3_func.get([4, 8]));   // 12



let comp4 = new Composite();
let comp5 = new Composite();
comp4.setMember('x', comp5);
comp5.setMember(['a', 'b'], 'value123');
let comp4_x = new Inout(comp4, 'x');
console.log(comp4_x.getMember(['a', 'b']));    // value123
comp4_x.setMember(['a', 'b'], 'newVal');
console.log(comp4_x.getMember(['a', 'b']));    // newVal


let comp7 = new Composite();
let dict8 = new Dictionary();
let comp9 = new Composite();
comp7.setMember('subscripted', comp9);
comp7.setMember('subscripted', dict8);
dict8.setMember([1], 'firstSubscript');
comp9.setMember([1, 2], 'secondSubscript');

let comp7_subscripted = new Inout(comp7, 'subscripted');
let comp7_subscriptedArgs1 = new Inout(comp7, 'subscripted', [1]);
let comp7_subscriptedArgs2 = new Inout(comp7, 'subscripted', [1, 2]);
console.log(comp7_subscripted.getMember([1]));   // firstSubscript
//console.log(comp7_subscripted.getMember([1, 2]));  // secondSubscript  <--  без приведения к getType перед subscript-обращением - не работает
console.log(comp7_subscripted.get(undefined, Composite).getMember([1, 2]));  // secondSubscript
console.log(comp7_subscriptedArgs1.get());   // firstSubscript
//console.log(comp7_subscriptedArgs2.get());   // secondSubscript  <--  без приведения к getType перед subscript-обращением (невозможно в цельной цепочке) - не работает



let dict = new Dictionary();
let dict_foo = new Inout(dict, ['foo']);
dict_foo.set(42);
console.log(dict_foo.get()); // 42
dict_foo.delete_();
console.log(dict_foo.get()); // undefined



let dict2 = new Dictionary();

dict2.setMember(['func'], (a) => 0, true);
dict2.setMember(['func'], (a, b) => 1, true);

let dict2_func = new Inout(dict2, ['func']);

console.log(dict2.getMember(['func'], [1]));  // 0
console.log(dict2_func.get([1, 2]));          // 1
console.log(dict2_func.get([1, 2, 3]));       // undefined
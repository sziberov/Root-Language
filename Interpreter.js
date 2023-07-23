class Interpreter {
	static tokens;
	static tree;
	static composites;
	static scopes;
	static controlTransfer;
	static preferences;
	static reports;

	static rules = {
		argument: (node, scope) => {
			return {
				label: node.label?.value,
				value: this.executeStatement(node.value, scope)
			}
		},
		arrayLiteral: (node, scope) => {
			let result,
				values = []

			for(let value of node.values) {
				value = this.executeStatement(value, scope);

				if(value != null) {
					values.push(value);
				}
			}

			let composite = this.getValueComposite(this.findMemberOverload(scope, 'Array')?.value);

			if(composite != null) {
				// TODO: Instantinate Array()
			} else {
				result = this.createValue('dictionary', values);
			}

			return result;
		},
		arrayType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'array');
		},
		booleanLiteral: (node) => {
			// TODO: Instantinate Boolean()

			return this.createValue('boolean', node.value === 'true');
		},
		callExpression: (node, scope) => {
			let gargs = [],  // (Generic) arguments
				args = []

			for(let garg of node.genericArguments) {
				let type = [],
					typePart = this.helpers.createTypePart(type, undefined, garg, scope);

				gargs.push(type);
			}
			for(let arg of node.arguments) {
				arg = this.executeStatement(arg, scope);

				if(arg != null) {
					args.push(arg);
				}
			}

			let callee = node.callee,
				calleeMOSP = this.helpers.getMemberOverloadSearchParameters(callee, scope);

			if(calleeMOSP == null) {
				callee = this.executeStatement(callee, scope);
			}

			if((calleeMOSP?.composite == null || calleeMOSP?.identifier == null) && callee == null) {
				this.report(1, node, 'Cannot call anything but a valid (in particular, chain) expression.');

				return;
			}

			let calleeFCP = this.helpers.getFunctionCallParameters(node, scope, calleeMOSP, callee, args, gargs);

			if(calleeFCP.function == null) {
				this.report(2, node, (!calleeFCP.initializer ? 'Function' : 'Initializer')+' with specified signature wasn\'t found.');

				return;
			}

			return this.helpers.callFunction(node, scope, calleeFCP.function, args, gargs, calleeFCP.FSC, calleeFCP.initializer);
		},
		chainExpression: (node, scope) => {
			let composite = this.getValueComposite(this.executeStatement(node.composite, scope));

			if(composite == null) {
				this.report(2, node, 'Composite wasn\'t found.');

				return;
			}

			let identifier = node.member;

			if(identifier.type === 'stringLiteral') {
				identifier = this.rules.stringLiteral(node.member, scope);
			} else {
				identifier = identifier.value;
			}

			return this.helpers.findMemberOverload(scope, composite, identifier, undefined, true)?.value;
		},
		classDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		classExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		collectionType: (node, scope, type, typePart, title) => {
			let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				composite = this.getValueComposite(this.findMemberOverload(scope, capitalizedTitle)?.value);

			if(composite != null) {
				typePart.reference = this.getOwnID(composite);
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

			if(title === 'dictionary') {
				this.helpers.createTypePart(type, typePart, node.key, scope);
			}

			this.helpers.createTypePart(type, typePart, node.value, scope);
		},
		combiningType: (node, scope, type, typePart, title) => {
			if(node.subtypes.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [title]: true });

			for(let subtype of node.subtypes) {
				this.helpers.createTypePart(type, typePart, subtype, scope);
			}
		},
		compositeDeclaration: (node, scope, anonymous) => {
			let modifiers = node.modifiers,
				identifier = node.identifier?.value;

			if(!anonymous && identifier == null) {
				return;
			}

			let title = node.type.replace('Declaration', '')
								 .replace('Expression', ''),
				capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				genericParameters = node.genericParameters,
				inheritedTypes = node.inheritedTypes,
				composite = this['create'+capitalizedTitle](identifier, scope);

			if(genericParameters?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { genericParameters: true });

				for(let genericParameter of genericParameters) {
					this.rules.genericParameter(genericParameter, scope, composite.type, typePart);
				}
			}
			if(inheritedTypes?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { inheritedTypes: true });

				for(let inheritedType of inheritedTypes) {
					this.helpers.createTypePart(composite.type, typePart, inheritedType, scope, false);

					// TODO: Protocol conformance checking (if not conforms, remove from type and report)
				}
			}
			for(let typePart of composite.type) {
				this.retainComposite(composite, this.getComposite(typePart.reference));
			}

			if(title === 'class') {
				let superComposite = this.getComposite(this.getTypeInheritedID(composite.type));

				if(superComposite != null) {
					this.setSuperID(composite, superComposite);
				}
			}

			let type = [this.createTypePart(undefined, undefined, { predefined: capitalizedTitle, self: true })],
				value = this.createValue('reference', this.getOwnID(composite)),
				observers = [],
				statements = node.body?.statements ?? [],
				objectStatements = []

			if(['class', 'structure'].includes(title)) {
				this.helpers.separateStatements(statements, objectStatements);
				this.setStatements(composite, objectStatements);
			}

			if(!anonymous) {
				this.setMemberOverload(scope, identifier, modifiers, type, value, observers);
			}

			this.addScope(composite);
			this.executeStatements(statements, composite);
			this.removeScope(false);

			if(anonymous) {
				return value;
			}
		},
		defaultType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable']);
		},
		deinitializerDeclaration: (node, scope) => {
			let identifier = 'deinit',
				function_ = this.createFunction(identifier, node.body?.statements, scope),
				signature = this.rules.functionSignature({
					type: 'functionSignature',
					genericParameters: [],
					parameters: [],
					awaits: -1,  // 1?
					throws: -1,  // 1?
					returnType: undefined
				}, scope),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_)),
				observers = []

			function_.type = signature;

			this.setMemberOverload(scope, identifier, [], type, value, observers, () => {});
		},
		dictionaryLiteral: (node, scope) => {
			let result = this.createValue('dictionary', new Map());

			for(let entry of node.entries) {
				entry = this.rules.entry(entry, scope);

				if(entry != null) {
					result.primitiveValue.set(entry.key, entry.value);
				}
			}

			let calleeMOSP = { composite: scope, identifier: 'Dictionary', internal: false },
				args = [{ label: undefined, value: result }],
				gargs = [],
				calleeFCP = this.helpers.getFunctionCallParameters(node, scope, calleeMOSP, undefined, args, gargs);

			if(calleeFCP.function != null) {
				result = this.helpers.callFunction(node, scope, calleeFCP.function, args, gargs, calleeFCP.FSC, calleeFCP.initializer);
			}

			return result;
		},
		dictionaryType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'dictionary');
		},
		entry: (node, scope) => {
			return {
				key: this.executeStatement(node.key, scope),
				value: this.executeStatement(node.value, scope)
			}
		},
		enumerationDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		enumerationExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		expressionsSequence: (node, scope) => {
			if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '=') {
				let lhs = node.values[0],
					lhsMOSP = this.helpers.getMemberOverloadSearchParameters(lhs, scope);

				if(lhsMOSP != null && lhsMOSP.composite != null && lhsMOSP.identifier != null) {
					lhs = this.findMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, undefined, lhsMOSP.internal);
				} else {
					lhs = undefined;
				}

				// TODO: Create member with default type if not exists

				if(lhs == null) {
					this.report(1, node, 'Cannot assign to anything but a valid identifier or chain expression.');

					return;
				}

				let rhs = this.executeStatement(node.values[2], scope);

				this.setMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, lhs.modifiers, lhs.type, rhs, lhs.observers, undefined, lhsMOSP.internal);

				return rhs;
			}
			if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '==') {
				let lhs = this.executeStatement(node.values[0], scope),
					rhs = this.executeStatement(node.values[2], scope),
					value = lhs?.primitiveType === rhs?.primitiveType && lhs?.primitiveValue === rhs?.primitiveValue;

				if(!value) {
				//	debugger;
				}

				return this.createValue('boolean', value);
			}
		},
		floatLiteral: (node) => {
			// TODO: Instantinate Float()

			return this.createValue('float', node.value*1);
		},
		functionDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier?.value;

			if(identifier == null) {
				return;
			}

			let function_,
				statements = Array.from(node.body?.statements ?? []),
				objectStatements = [],
				signature,
				functionScope,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value,
				observers = [],
				object = this.compositeIsObject(scope),
				staticDeclaration = modifiers.includes('static') || !object && !this.compositeIsInstantiable(scope);

			this.helpers.separateStatements(statements, objectStatements);

			if(staticDeclaration) {
				if(!object) {  // Static in non-object
					signature = node.signature;
					functionScope = scope;
				} else {  // Static in object
					return;
				}
			} else {
				if(object) {  // Non-static in object
					statements = []
					signature = node.signature;
					functionScope = this.findMemberOverload(scope, identifier, (v) => this.findValueFunction(v.value, []), true)?.matchingValue ?? scope;
				} else {  // Non-static in non-object
					if(statements.length === 0) {
						return;
					}

					objectStatements = []
					functionScope = scope;
				}
			}

			function_ = this.createFunction(identifier, objectStatements, functionScope);
			function_.type = this.rules.functionSignature(signature, scope);
			value = this.createValue('reference', this.getOwnID(function_));

			this.setMemberOverload(scope, identifier, modifiers, type, value, observers, () => {});
			this.executeStatements(statements, function_);
		},
		functionExpression: (node, scope) => {
			let signature = this.rules.functionSignature(node.signature, scope),
				function_ = this.createFunction(undefined, node.body?.statements, scope);

			function_.type = signature;

			return this.createValue('reference', this.getOwnID(function_));
		},
		functionSignature: (node, scope) => {
			let type = [],
				typePart = this.createTypePart(type, undefined, { predefined: 'Function' });

			typePart.awaits = node?.awaits ?? -1;
			typePart.throws = node?.throws ?? -1;

			for(let v of ['genericParameters', 'parameters']) {
				if(node?.[v].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v]: true });

					for(let node_ of node[v]) {
						this.executeStatement(node_, scope, type, typePart_);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node?.returnType, scope);

			return type;
		},
		functionType: (node, scope, type, typePart) => {
			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

			typePart.awaits = node.awaits ?? 0;
			typePart.throws = node.throws ?? 0;

			for(let v of ['genericParameter', 'parameter']) {
				if(node[v+'Types'].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v+'s']: true });

					for(let node_ of node[v+'Types']) {
						this.helpers.createTypePart(type, typePart_, node_, scope);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node.returnType, scope);
		},
		genericParameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);
			typePart.identifier = node.identifier.value;
		},
		identifier: (node, scope) => {
			return this.helpers.findMemberOverload(scope, scope, node.value)?.value;
		},
		ifStatement: (node, scope) => {
			if(node.condition == null) {
				return;
			}

			let namespace = this.createNamespace('Local<'+(scope.title ?? '#'+this.getOwnID(scope))+', If>', scope, null),
				condition;

			this.addScope(namespace);

			condition = this.executeStatement(node.condition, namespace);
			condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

			if(condition || node.else?.type !== 'ifStatement') {
				let branch = node[condition ? 'then' : 'else']

				if(branch?.type === 'functionBody') {
					this.executeStatements(branch.statements, namespace);
				} else {
					this.setControlTransfer(this.executeStatement(branch, namespace));
				}
			}

			this.removeScope();

			if(!condition && node.else?.type === 'ifStatement') {
				this.rules.ifStatement(node.else, scope);
			}

			return this.controlTransfer?.value;
		},
		initializerDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = 'init',
				function_ = this.createFunction(identifier, node.body?.statements, scope),
				signature = node.signature,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_)),
				observers = []

			signature ??= {
				type: 'functionSignature',
				genericParameters: [],
				parameters: [],
				awaits: -1,
				throws: -1,
				returnType: undefined
			}
			signature.returnType = {
				type: 'typeIdentifier',
				identifier: {
					type: 'identifier',
					value: 'Self'
				},
				genericArguments: []
			}

			if(node.nillable) {
				signature.returnType = {
					type: 'nillableType',
					value: signature.returnType
				}
			}

			signature = this.rules.functionSignature(signature, scope);
			function_.type = signature;

			if(function_.statements.at(-1)?.type !== 'returnStatement') {
				function_.statements.push({
					type: 'returnStatement',
					value: {
						type: 'identifier',
						value: 'self'
					}
				});
			}

			this.setMemberOverload(scope, identifier, modifiers, type, value, observers, () => {});
		},
		inoutExpression: (node, scope) => {
			let value = this.executeStatement(node.value, scope);

			if(value?.primitiveType === 'pointer') {
				return value;
			}
			if(value?.primitiveType !== 'reference') {
				this.report(2, node, 'Non-reference value ("'+(value?.primitiveType ?? 'nil')+'") can\'t be used as a pointer.');

				return;
			}

			return this.createValue('pointer', value.primitiveValue);
		},
		inoutType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['inout']);
		},
		integerLiteral: (node) => {
			// TODO: Instantinate Integer()

			return this.createValue('integer', node.value*1);
		},
		intersectionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'intersection');
		},
		module: () => {
			let namespace = this.getComposite(0) ?? this.createNamespace('Global');

			let print = this.createFunction('print', (arguments_) => {
					this.print(this.getValueString(arguments_[0].value));
				}, namespace),
				getComposite = this.createFunction('getComposite', (arguments_) => {
					return this.createValue('reference', arguments_[0].value.primitiveValue);
				}, namespace),
				getCallsString = this.createFunction('getCallsString', () => {
					return this.createValue('string', this.getCallsString());
				}, namespace);

			print.type = [
				{
					predefined: 'Function',
					awaits: -1,
					throws: -1
				},
				{
					parameters: true,
					super: 0
				},
				{
					super: 1,
					predefined: '_',
					nillable: true,
					identifier: 'value'
				},
				{
					super: 0,
					predefined: '_',
					nillable: true
				}
			]
			getComposite.type = [
				{
					predefined: 'Function',
					awaits: -1,
					throws: -1
				},
				{
					parameters: true,
					super: 0
				},
				{
					super: 1,
					predefined: 'integer',
					identifier: 'value'
				},
				{
					super: 0,
					predefined: 'Any',
					nillable: true
				}
			]
			getCallsString.type = [
				{
					predefined: 'Function',
					awaits: -1,
					throws: -1
				},
				{
					super: 0,
					predefined: 'string'
				}
			]

			this.setMemberOverload(namespace, 'print', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(print)), []);
			this.setMemberOverload(namespace, 'getComposite', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getComposite)), []);
			this.setMemberOverload(namespace, 'getCallsString', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getCallsString)), []);

			this.addScope(namespace);
			this.executeStatements(this.tree?.statements, namespace);
			this.removeScope();
			this.resetControlTransfer();
		},
		namespaceDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		namespaceExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		nillableExpression: (node, scope) => {
			let value;

			try {
				value = this.executeStatement(node, scope);
			} catch(error) {
				/*
				if(error !== 0) {
					throw error;
				}
				*/
			}

			return value;
		},
		nillableType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable'], 1);
		},
		nilLiteral: () => {},
		operatorDeclaration: (node, scope) => {
			let operator = node.operator?.value,
				precedence,
				associativity;

			if(operator == null) {
				return;
			}

			for(let entry of node.body?.statements ?? []) {
				if(entry.type === 'entry') {
					if(entry.key.type === 'identifier') {
						if(entry.key.value === 'precedence' && entry.value.type === 'integerLiteral') {
							precedence = entry.value.value;
						}
						if(entry.key.value === 'associativity' && entry.value.type === 'identifier' && ['left', 'right', 'none'].includes(entry.value.value)) {
							associativity = entry.value.value;
						}
					}
				}
			}

			if(precedence == null || associativity == null) {
				return;
			}

			this.setOperatorOverload(scope, node.operator.value, node.modifiers, precedence, associativity);
		},
		optionalType: (node, scope, type, typePart, titles, mainTitle) => {
			if(!titles.some(v => v in typePart)) {
				typePart[titles[mainTitle ?? 0]] = true;
			}

			this.executeStatement(node.value, scope, type, typePart);
		},
		parameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);

			if(node.label != null) {
				typePart.label = node.label.value;
			}

			typePart.identifier = node.identifier.value;

			if(node.value != null) {
				typePart.value = node.value;
			}
		},
		parenthesizedExpression: (node, scope) => {
			return this.executeStatement(node.value, scope);
		},
		parenthesizedType: (node, scope, type, typePart) => {
			this.executeStatement(node.value, scope, type, typePart);
		},
		postfixExpression: (node, scope) => {
			let value = this.executeStatement(node.value, scope);

			if(value == null) {
				return;
			}

			if(['float', 'integer'].includes(value.primitiveType)) {
				if(node.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue-1);
				}
				if(node.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue+1);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		predefinedType: (node, scope, type, typePart) => {
			typePart.predefined = node.value;
		},
		prefixExpression: (node, scope) => {
			let value = this.executeStatement(node.value, scope);

			if(value == null) {
				return;
			}

			if(node.operator?.value === '!' && value.primitiveType === 'boolean') {
				return this.createValue('boolean', !value.primitiveValue);
			}
			if(['float', 'integer'].includes(value.primitiveType)) {
				if(node.operator?.value === '-') {
					return this.createValue(value.primitiveType, -value.primitiveValue);
				}
				if(node.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
				if(node.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		protocolDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		protocolExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		protocolType: (node, scope, type, typePart) => {
			// TODO: This
		},
		returnStatement: (node, scope) => {
			let value = this.executeStatement(node.value, scope);

			this.setControlTransfer(value, 'return');

			return value;
		},
		stringLiteral: (node, scope, primitive) => {
			let string = '';

			for(let segment of node.segments) {
				if(segment.type === 'stringSegment') {
					string += segment.value;
				} else {
					string += this.getValueString(this.executeStatement(segment.value, scope));
				}
			}

			// TODO: Instantinate String()

			return this.createValue('string', string);
		},
		structureDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		structureExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		throwStatement: (node, scope) => {
			let value = this.executeStatement(node.value, scope);

			this.setControlTransfer(value, 'throw');

			return value;
		},
		typeExpression: (node, scope) => {
			let type = [],
				typePart = this.helpers.createTypePart(type, undefined, node.type_, scope);

			return this.createValue('type', type);
		},
		typeIdentifier: (node, scope, type, typePart) => {
			let composite = this.getValueComposite(this.rules.identifier(node.identifier, scope));

			if(composite == null || this.compositeIsObject(composite)) {
				typePart.predefined = 'Any';

				this.report(2, node, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = this.getOwnID(composite, true);
			}

			if(node.genericArguments.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

			for(let genericArgument of node.genericArguments) {
				this.helpers.createTypePart(type, typePart, genericArgument, scope);
			}
		},
		unionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'union');
		},
		variableDeclaration: (node, scope) => {
			let modifiers = node.modifiers;

			for(let declarator of node.declarators) {
				let identifier = declarator.identifier.value,
					type = [],
					typePart = this.helpers.createTypePart(type, undefined, declarator.type_, scope),
					value = this.executeStatement(declarator.value, scope),
					observers = []

				// Type-related checks

				this.setMemberOverload(scope, identifier, modifiers, type, value, observers);
			}
		},
		variadicGenericParameter: (node, scope) => {
			return {
				identifier: undefined,
				type: [this.createTypePart(undefined, undefined, { variadic: true })]
			}
		},
		variadicType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['variadic']);
		}
	}

	static helpers = {
		callFunction: (node, scope, function_, args, gargs, FSC, initializer) => {
		//	this.report(0, node, 'ca: '+function_.title+', '+this.getOwnID(function_));

			let FSO,  // Function self object
				SSC = this.getComposite(scope.IDs.Self),  // Scope self composite
				location = this.tokens[node.range.start]?.location;

			if(initializer) {
				if(gargs.length === 0) {  // Find initializer's object in scope's object chain
					let SSO = this.getComposite(scope.IDs.self);  // Scope self (super) object

					if(this.compositeIsObject(SSO)) {
						while(SSO != null) {
							if(FSC === this.getComposite(SSO.IDs.Self)) {
								if(SSO.life === 0) {
									FSO = SSO;
								}

								break;
							}

							SSO = this.getComposite(SSO.IDs.super);
						}
					}
				}

				if(FSO == null && this.compositeIsInstantiable(FSC)) {  // Or create new object chain
					let objects = []

					while(FSC != null) {
						let object = this.createObject(undefined, FSC, objects[0]);

						objects.unshift(object);

						if(objects.length === 1) {
							let gparams = this.getTypeGenericParameters(FSC.type);  // Generic parameters

							for(let i = 0; i < gparams.length; i++) {
								if(gargs.length-1 < i) {
									break;
								}

								this.setMemberOverload(object, gparams[i].identifier, [], [{ predefined: 'type' }], gargs[i], []);
							}
						}
						if(objects.length > 1) {
							this.setSuperID(objects[1], objects[0]);
						}

						FSC = this.getComposite(this.getTypeInheritedID(FSC.type));
					}

					FSO = objects.at(-1);
				}
			}

			let value = this.callFunction(function_, args, FSO, undefined, SSC, location);
		//	this.report(0, node, 'ke: '+JSON.stringify(value));

			return value;
		},
		createOrSetCollectionTypePart: (type, typePart, collectionFlag) => {
			let collectionFlags = {
				array: true,
				dictionary: true,
				genericArguments: true,
				genericParameters: true,
				intersection: true,
				parameters: true,
				predefined: 'Function',
				union: true
			}

			for(let flag in typePart) {
				if(
					flag in collectionFlags &&
					typePart[flag] === collectionFlags[flag]
				) {
					return this.createTypePart(type, typePart, collectionFlag);
				}
			}

			return Object.assign(typePart, collectionFlag);
		},
		createTypePart: (type, typePart, node, scope, fallback = true) => {
			typePart = this.createTypePart(type, typePart);

			this.executeStatement(node, scope, type, typePart);

			if(fallback) {
				for(let flag in typePart) {
					if(flag !== 'super') {
						return typePart;
					}
				}

				typePart.predefined = '_';
				typePart.nillable = true;
			}

			return typePart;
		},
		separateStatements: (statements, objectStatements) => {
			let staticTypes = ['deinitializerDeclaration', 'initializerDeclaration'],
				bothTypes = ['functionDeclaration']

			for(let i = 0; i < statements.length; i++) {
				let statement = statements[i]

				if(staticTypes.includes(statement.type)) {
					continue;
				}

				if(!statement.modifiers?.includes('static')) {
					objectStatements.push(statement);

					if(bothTypes.includes(statement.type)) {
						continue;
					}

					statements.splice(i, 1);
					i--;
				}
			}
		},
		findMemberOverload: (scope, composite, identifier, matching, internal) => {
			// TODO: Access-related checks

			return this.findMemberOverload(composite, identifier, matching, internal);
		},
		getFunctionCallParameters: (node, scope, calleeMOSP, callee, args, gargs) => {
			for(let i = 0; i < 2; i++) {
				let function_ = calleeMOSP == null ? this.findValueFunction(callee, args) : this.findMemberOverload(calleeMOSP.composite, calleeMOSP.identifier, (v) => this.findValueFunction(v.value, args), calleeMOSP.internal)?.matchingValue,
					FSC = this.getComposite(function_?.IDs.Self) ?? calleeMOSP.composite,  // Function self composite
					initializing = function_?.title === 'init' || calleeMOSP?.identifier === 'init',
					instantiable = this.compositeIsInstantiable(FSC),
					initializer = initializing && instantiable;

				if(function_ == null && i === 0 && !initializer) {  // Fallback to search of an initializer
					calleeMOSP = {
						composite: calleeMOSP == null ? this.getValueComposite(callee) : this.findMemberOverload(calleeMOSP.composite, calleeMOSP.identifier, (v) => this.getValueComposite(v.value), calleeMOSP.internal)?.matchingValue,
						identifier: 'init',
						internal: true
					}

					if(this.compositeIsObject(calleeMOSP.composite)) {
						calleeMOSP.composite = this.getComposite(calleeMOSP.composite.IDs.Self);
					}

					continue;
				}

				return {
					function: function_,
					FSC: FSC,
					initializer: initializer
				}
			}
		},
		getMemberOverloadSearchParameters: (node, scope) => {
			if(node.type === 'identifier') {
				return {
					composite: scope,
					identifier: node.value,
					internal: false
				}
			} else
			if(node.type === 'chainExpression') {
				return {
					composite: this.getValueComposite(this.executeStatement(node.composite, scope)),
					identifier: node.member.type === 'identifier' ? node.member.value : this.rules.stringLiteral(node.member, scope, true).primitiveValue,
					internal: true
				}
			}
		}
	}

	static getSave() {
		// https://isocpp.org/wiki/faq/serialization

		// Can be useful as the optimization step for imports (kind of precompliation)
		// Then source code changes should be supported using hashes or anything

		/*
		return {
			tokens: JSON.stringify(this.tokens),
			tree: JSON.stringify(this.tree),
			composites: JSON.stringify(this.composites),
			calls: JSON.stringify(this.scopes),
			preferences: JSON.stringify(this.preferences),
			reports: JSON.stringify(this.reports)
		}
		*/

		return JSON.stringify(this.composites, (k, v) => {
			if(v instanceof Map) {
				return {
					__TYPE__: 'Map',
					__VALUE__: Array.from(v.entries())
				};
			} else
			if(v == null) {
				return null;
			} else {
				return v;
			}
		});
	}

	static restoreSave(save) {}

	static getComposite(ID) {
		return this.composites[ID]
	}

	static createComposite(title, type, scope) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		let composite = {
			title: title,
			IDs: {
				own: this.composites.length,
				super: undefined,
				Super: undefined,
				self: undefined,
				Self: undefined,
				sub: undefined,
				Sub: undefined,
				scope: undefined,
				retainers: []
			},
			life: 1,  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
			type: type,
			statements: [],
			imports: {},
			operators: {},
			members: {},
			observers: []
		}

		this.composites.push(composite);

		if(scope != null) {
			this.setScopeID(composite, scope);
		}
	//	this.print('cr: '+composite.title+', '+this.getOwnID(composite));

		return composite;
	}

	static destroyComposite(composite) {
		if(composite.life === 2) {
			return;
		}

	//	this.print('ds: '+composite.title+', '+this.getOwnID(composite));

		composite.life = 2;

		if(this.compositeIsObject(composite)) {
			let function_ = this.findMemberOverload(composite, 'deinit', (v) => this.findValueFunction(v.value, []), true)?.matchingValue;

			if(function_ != null) {
				this.callFunction(function_, [], composite);
			}
		}

		let ID = this.getOwnID(composite);

		for(let composite_ of this.composites) {
			if(this.getRetainersIDs(composite_, true)?.includes(ID)) {
				this.releaseComposite(composite, composite_);
			}
		}

		delete this.composites[ID]

		if(this.getRetainersIDs(composite) > 0) {
		// TODO: Notify all retainers about destroying

			this.report(1, undefined, 'Composite #'+this.getOwnID(composite)+' was destroyed with a non-empty retainer list (either it was retained by on its own non-significantly-retained composite(s) or something destroyed it forcibly).');
		}

	//	this.print('de: '+JSON.stringify(composite));
	}

	static destroyReleasedComposite(composite) {
		if(composite != null && !this.compositeRetained(composite)) {
			this.destroyComposite(composite);
		}
	}

	static retainComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		let retainersIDs = this.getRetainersIDs(retainedComposite),
			retainingID = this.getOwnID(retainingComposite);

		if(!retainersIDs.includes(retainingID)) {
			retainersIDs.push(retainingID);
		}
	}

	static releaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		let retainersIDs = this.getRetainersIDs(retainedComposite),
			retainingID = this.getOwnID(retainingComposite);

		if(retainersIDs.includes(retainingID)) {
			retainersIDs.splice(retainersIDs.indexOf(retainingID));

			this.destroyReleasedComposite(retainedComposite);
		}
	}

	/*
	 * Automatically retains or releases composite.
	 *
	 * Use of this function is highly recommended when real retainment state is unknown
	 * at the moment, otherwise basic functions can be used in favour of performance.
	 */
	static retainOrReleaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		if(this.compositeRetains(retainingComposite, retainedComposite)) {
			this.retainComposite(retainingComposite, retainedComposite);
		} else {
			this.releaseComposite(retainingComposite, retainedComposite);
		}
	}

	/*
	 * Returns a real state of direct retainment.
	 *
	 * Composite considered really retained if it is used by an at least one of
	 * the retainer's IDs (excluding own and retainers list), type, import, member or observer.
	 */
	static compositeRetains(retainingComposite, retainedComposite) {
		if(retainingComposite?.life < 2) return (
			this.IDsRetain(retainingComposite, retainedComposite) ||
			this.typeRetains(retainingComposite.type, retainedComposite) ||
			this.importsRetain(retainingComposite, retainedComposite) ||
			this.membersRetain(retainingComposite, retainedComposite) ||
			this.observersRetain(retainingComposite, retainedComposite)
		);
	}

	/*
	 * Returns a formal state of direct or indirect retainment.
	 *
	 * Composite considered formally retained if it or at least one of
	 * its retainers list members (recursively) can be as recognized as the retainer.
	 *
	 * retainersIDs is used to exclude a retain cycles and redundant passes
	 * from the lookup and meant to be set internally only.
	 */
	static compositeRetainsDistant(retainingComposite, retainedComposite, retainersIDs = []) {
		/*
		if(Array.isArray(retainingComposite)) {
			for(let retainingComposite_ of retainingComposite) {
				if(this.compositeRetainsDistant(retainingComposite_, retainedComposite)) {
					return true;
				}
			}

			return false;
		}
		*/

		if(retainedComposite == null || retainingComposite == null) {
			return;
		}
		if(retainedComposite === retainingComposite) {
			return true;
		}

		for(let retainerID of this.getRetainersIDs(retainedComposite)) {
			if(retainersIDs.includes(retainerID)) {
				continue;
			}

			retainersIDs.push(retainerID);

			if(this.compositeRetainsDistant(retainingComposite, this.getComposite(retainerID), retainersIDs)) {
				return true;
			}
		}
	}

	/*
	 * Returns a significant state of general retainment.
	 *
	 * Composite considered significantly retained if it is formally retained by
	 * the global namespace, a current scope namespace or a control trasfer value.
	 */
	static compositeRetained(composite) {
		return (
			this.compositeRetainsDistant(this.getComposite(0), composite) ||
			this.compositeRetainsDistant(this.getScope()?.namespace, composite) ||
		//	this.compositeRetainsDistant(this.scopes.map(v => v.namespace), composite) ||  // May be useful when "with" syntax construct will be added
			this.compositeRetainsDistant(this.getValueComposite(this.controlTransfer?.value), composite)
		);
	}

	static createClass(title, scope) {
		let class_ = this.createComposite(title, [{ predefined: 'Class' }], scope);

		this.setSelfID(class_, class_);

		return class_;
	}

	static createEnumeration(title, scope) {
		let enumeration = this.createComposite(title, [{ predefined: 'Enumeration' }], scope);

		this.setSelfID(enumeration, enumeration);

		return enumeration;
	}

	static createFunction(title, statements, scope) {
		let function_ = this.createComposite(title, [{ predefined: 'Function' }], scope);

		this.setInheritedLevelIDs(function_, scope);
		this.setStatements(function_, statements);

		return function_;
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels is missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	static createNamespace(title, scope, levels) {
		let namespace = this.createComposite(title, [{ predefined: 'Namespace' }], scope);

		if(levels != null) {
			this.setInheritedLevelIDs(namespace, levels);
		} else
		if(levels !== null) {
			this.setSelfID(namespace, namespace);
		}

		return namespace;
	}

	static createObject(superObject, selfComposite, subObject) {
		let title = 'Object<'+(selfComposite.title ?? '#'+this.getOwnID(selfComposite))+'>',
			type = [{ predefined: 'Object', inheritedTypes: true }, { super: 0, reference: this.getOwnID(selfComposite) }],
			object = this.createComposite(title, type, selfComposite);

		object.life = 0;

		if(superObject != null) {
			this.setSuperID(object, superObject);
		}

		this.setSelfID(object, object);

		if(subObject != null) {
			this.setSubID(object, subObject);
		}

		return object;
	}

	static createProtocol(title, scope) {
		let protocol = this.createComposite(title, [{ predefined: 'Protocol' }], scope);

		this.setSelfID(protocol, protocol);

		return protocol;
	}

	static createStructure(title, scope) {
		let structure = this.createComposite(title, [{ predefined: 'Structure' }], scope);

		this.setSelfID(structure, structure);

		return structure;
	}

	static compositeIsFunction(composite) {
		return this.typeIsComposite(composite?.type, 'Function');
	}

	static compositeIsNamespace(composite) {
		return this.typeIsComposite(composite?.type, 'Namespace');
	}

	static compositeIsObject(composite) {
		return this.typeIsComposite(composite?.type, 'Object');
	}

	static compositeIsInstantiable(composite) {
		return (
			this.typeIsComposite(composite?.type, 'Class') ||
			this.typeIsComposite(composite?.type, 'Structure')
		);
	}

	static compositeIsCallable(composite) {
		return (
			this.compositeIsFunction(composite) ||
			this.compositeIsInstantiable(composite)
		);
	}

	static getScope(offset = 0) {
		return this.scopes[this.scopes.length-1+offset]
	}

	/*
	 * Should be used for a bodies before executing its contents.
	 *
	 * ARC utilize a current scope to be aware of retainments by the execution state
	 * along with a control transfer value.
	 *
	 * Additionally function and its call location can be specified for debugging purposes.
	 */
	static addScope(namespace, function_, location) {
		this.scopes.push({
			namespace: namespace,
			function: function_,
			location: location
		});
	}

	/*
	 * Removes from the stack and optionally automatically destroys its last scope.
	 */
	static removeScope(destroy = true) {
		let namespace = this.scopes.pop()?.namespace;

		if(destroy) {
			this.destroyReleasedComposite(namespace);
		}
	}

	static getCalls() {
		return this.scopes.filter(v => v.function != null);
	}

	static getCallsString() {
		let result = '';

		for(let i = this.scopes.length-1, j = 0; i >= 0 && j < 8; i--) {
			let scope = this.scopes[i],
				function_ = scope.function,
				location = scope.location;

			if(function_ == null) {
				continue;
			}

			if(j > 0) {
				result += '\n';
			}

			result += j+': ';

			let composite = this.getComposite(function_.IDs.Self);

			if(function_.title != null && composite != null) {
				result += (composite.title ?? '#'+this.getOwnID(composite))+'.';
			}

			result += function_.title ?? '#'+this.getOwnID(function_);

			if(location != null) {
				result += ':'+(location.line+1)+':'+(location.column+1);
			}

			j++;
		}

		return result;
	}

	/*
	 * Should be used for a return values before releasing its scopes.
	 *
	 * ARC utilize a control trasfer value to be aware of retainments by the execution state
	 * along with a current scope.
	 */
	static setControlTransfer(value, type) {
		this.controlTransfer = {
			value: value,
			type: type
		}
	}

	static resetControlTransfer() {
		this.controlTransfer = undefined;
	}

	static executeStatement(node, scope, ...arguments_) {
		return this.rules[node?.type]?.(node, scope, ...arguments_);
	}

	/*
	 * Last statement in a body will be treated like a returning one even if it's not an explicit return.
	 * Manual control transfer and additional control transfer types is supported but should be also implemented by rules.
	 */
	static executeStatements(nodes, scope, additionalCTT = []) {
		let globalScope = this.getScope(-1) == null,
			CTT = [  // Control transfer types
			//	'break',
			//	'continue',
			//	'fallthrough',
				'return',
				'throw',
				...additionalCTT
			]

		for(let node of nodes ?? []) {
			let start = this.composites.length,
				value = this.executeStatement(node, scope),
				end = this.composites.length,
				CTed = this.controlTransfer != null,  // Control transferred
				expectedlyCTed = CTed && CTT.includes(this.controlTransfer.type),
				valueCTed = CTed && this.controlTransfer.value === value,
				ICTing = node === nodes.at(-1),  // Implicitly control-transferring
				valueCTing = !expectedlyCTed && !valueCTed && ICTing;

			if(valueCTing) {
				this.setControlTransfer(!globalScope ? value : undefined);
			}

			for(start; start < end; start++) {
				this.destroyReleasedComposite(this.getComposite(start));
			}

			if(expectedlyCTed) {
				break;
			}
		}
	}

	/*
	 * Levels such as super, self or sub, can be inherited from another composite.
	 * If that composite is an unitialized object, it will be initialized with statements stored by its Self.
	 *
	 * Scope, that statements will be executed within, can be set explicitly or created automatically.
	 * Existing scope will prevail over levels, so it will not inherit them nor an object will be tried to initialize.
	 */
	static callFunction(function_, arguments_ = [], levels, scope, caller, location) {
		if(this.getCalls().length === this.preferences.callStackSize) {
			this.report(2, undefined, 'Maximum call stack size exceeded.\n'+this.getCallsString());

			return;
		}

		if(typeof function_.statements === 'function') {
			return function_.statements(arguments_);
		}

		let namespaceTitle = 'Call<'+(function_.title ?? '#'+this.getOwnID(function_))+'>',
			namespace = scope ?? this.createNamespace(namespaceTitle, function_, levels ?? function_),
			parameters = this.getTypeFunctionParameters(function_.type);

		if(caller != null) {
			let type = [{ predefined: 'Any', nillable: true }],
				value = this.createValue('reference', this.getOwnID(caller));

			this.setMemberOverload(namespace, 'caller', [], type, value, []);
		}

		for(let i = 0; i < arguments_.length; i++) {
			let argument = arguments_[i],
				parameterType = this.getSubtype(parameters, parameters[i]),
				identifier = parameterType[0]?.identifier ?? '$'+i;

			this.setMemberOverload(namespace, identifier, [], parameterType, argument.value, []);
		}

		let initializing = scope == null && this.compositeIsObject(levels) && levels.life === 0;

		this.addScope(namespace, function_, location);

		if(initializing) {
			let selfComposite = this.getComposite(levels.IDs.Self);

			this.addScope(levels);
			this.executeStatements(selfComposite?.statements, levels);
			this.removeScope(false);
		}

		this.executeStatements(function_.statements, namespace);

		if(initializing) {
			levels.life = 1;
		}

		this.removeScope();

		let returnValue = this.controlTransfer?.value;

		// TODO: Return value type checking

		this.resetControlTransfer();

		return returnValue;
	}

	static setCompositeType(composite, type) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		composite.type = type;
	}

	static setID(composite, key, value) {
		if(['own', 'retainers'].includes(key)) {
			return;
		}

		let OV, NV;  // Old/new value

		OV = composite.IDs[key]
		NV = composite.IDs[key] = value;

		if(key.toLowerCase() !== 'self') {  // ID chains should not be cyclic
			let composites = [],
				composite_ = composite;

			while(composite_ != null) {
				if(composites.includes(composite_)) {
					composite.IDs[key] = OV;

					return;
				}

				composites.push(composite_);

				composite_ = this.getComposite(composite_.IDs[key]);
			}
		}

		if(OV !== NV) {
			this.retainOrReleaseComposite(composite, this.getComposite(OV));
			this.retainComposite(composite, this.getComposite(NV));
		}
	}

	static getOwnID(composite, nillable) {
		return !nillable ? composite.IDs.own : composite?.IDs.own;
	}

	static getSelfCompositeID(composite) {
		return !this.compositeIsObject(composite) ? this.getOwnID(composite) : this.getTypeInheritedID(composite.type);
	}

	static getScopeID(composite) {
		return composite.IDs.scope;
	}

	static getRetainersIDs(composite, nillable) {
		return !nillable ? composite.IDs.retainers : composite?.IDs.retainers;
	}

	static setSuperID(composite, superComposite) {
		this.setID(composite, 'super', this.getOwnID(superComposite));
		this.setID(composite, 'Super', this.getSelfCompositeID(superComposite));
	}

	static setSelfID(composite, selfComposite) {
		this.setID(composite, 'self', this.getOwnID(selfComposite));
		this.setID(composite, 'Self', this.getSelfCompositeID(selfComposite));
	}

	static setSubID(object, subObject) {
		if(!this.compositeIsObject(object)) {
			return;
		}

		if(this.compositeIsObject(subObject)) {
			this.setID(object, 'sub', this.getOwnID(subObject));
			this.setID(object, 'Sub', this.getTypeInheritedID(subObject.type));
		}
	}

	static setInheritedLevelIDs(inheritingComposite, inheritedComposite) {
		if(inheritedComposite === inheritingComposite) {
			return;
		}

		let excluded = ['own', 'scope', 'retainers']

		for(let key in inheritingComposite.IDs) {
			if(!excluded.includes(key)) {
				this.setID(inheritingComposite, key, inheritedComposite?.IDs[key]);
			}
		}
	}

	static setScopeID(composite, scopeComposite) {
		this.setID(composite, 'scope', this.getOwnID(scopeComposite, true));
	}

	static IDsRetain(retainingComposite, retainedComposite) {
		let excluded = ['own', 'retainers']

		for(let key in retainingComposite.IDs) {
			if(!excluded.includes(key) && retainingComposite.IDs[key] === this.getOwnID(retainedComposite)) {
				return true;
			}
		}
	}

	static createTypePart(type, superPart, flags = {}) {
		let part = { ...flags }

		if(type != null) {
			if(superPart != null) {
				part.super = type.indexOf(superPart);
			}

			type.push(part);
		}

		return part;
	}

	static getTypeInheritedID(type) {
		let index = type.findIndex(v => v.inheritedTypes);

		if(index === -1) {
			return;
		}

		return type.find(v => v.super === index)?.reference;
	}

	static getTypeGenericParameters(type) {
		return this.getSubtype(type, type.find(v => v.genericParameters)).slice(1);
	}

	static getTypeFunctionParameters(type) {
		return this.getSubtype(type, type.find(v => v.parameters)).slice(1);
	}

	static getSubtype(type, part, offset) {
		offset ??= type.indexOf(part);

		if(offset < 0) {
			return []
		}
		if(offset === 0) {
			return structuredClone(type);
		}

		let type_ = [],
			part_ = structuredClone(part);

		part_.super -= offset;

		if(part_.super < 0) {
			delete part_.super;
		}

		type_.push(part_);

		for(let part_ of type) {
			if(part_.super === type.indexOf(part)) {
				type_.push(...this.getSubtype(type, part_, offset));
			}
		}

		return type_;
	}

	static typeIsComposite(type, wantedValue, any) {
		if(type == null) {
			return;
		}

		let currentValue = type[0]?.predefined;

		if(wantedValue != null && currentValue !== wantedValue) {
			return;
		}

		let possibleValues = [
			'Class',
			'Enumeration',
			'Function',
			'Namespace',
			'Object',
			'Protocol',
			'Structure'
		]

		if(any) {
			possibleValues.push('Any');
		}

		if(!possibleValues.includes(currentValue)) {
			return;
		}

		return true;
	}

	static typeAccepts(acceptingType, acceptedType) {
		// TODO: Type equality check

		return true;
	}

	static typeRetains(retainingType, retainedComposite) {
		return retainingType.some(v => v.reference === this.getOwnID(retainedComposite));
	}

	static createValue(primitiveType, primitiveValue) {
		return {
			primitiveType: primitiveType,	// 'boolean', 'dictionary', 'float', 'integer', 'pointer', 'reference', 'string', 'type'
			primitiveValue: primitiveValue	// boolean, integer, map (object), string, type
		}
	}

	static getValueType(value) {
		if(value == null) {
			return []
		}

		let type = [],
			part = this.createTypePart(type);

		if(!['pointer', 'reference'].includes(value.primitiveType)) {
			part.predefined = value.primitiveType;
		} else {
			if(value.primitiveType === 'pointer') {
				part.inout = true;
			}

			part.reference = value.primitiveValue;

			let composite = this.getValueComposite(value);

			while(composite != null) {
				if(!this.compositeIsObject(composite)) {
					if(this.getOwnID(composite) === value.primitiveValue) {
						part.self = true;
					} else {
						part.reference = this.getOwnID(composite);
					}

					composite = undefined;
				} else {
					composite = this.getComposite(this.getScopeID(composite));
				}
			}
		}

		return type;
	}

	static getValueComposite(value) {
		if(!['pointer', 'reference'].includes(value?.primitiveType)) {
			return;
		}

		return this.getComposite(value.primitiveValue);
	}

	static getValueString(value) {
		if(['boolean', 'float', 'integer', 'string'].includes(value?.primitiveType)) {
			return value.primitiveValue+'';
		}

		let composite = this.getValueComposite(value);

		if(composite != null) {
			return JSON.stringify(composite);
		}

		return value != null ? JSON.stringify(value) : 'nil';
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(!this.compositeIsFunction(function_)) {
			return;
		}

		let parameters = this.getTypeFunctionParameters(function_.type);

		if(arguments_.length !== parameters.filter(v => v.super === 0).length) {
			// TODO: Variadic parameters support

			return;
		}

		for(let i = 0; i < (arguments_ ?? []).length; i++) {
			let argument = arguments_[i],
				parameter = parameters[i]

			if(parameter.label != null && argument.label !== parameter.label) {
				return;
			}
			if(!this.typeAccepts(parameter, this.getValueType(argument.value))) {
				return;
			}
		}

		return function_;
	}

	static findValueFunction(value, arguments_) {
		let function_;

		if(value?.primitiveType === 'dictionary') {
			for(let key in value.primitiveValue) {
				if((function_ = this./*find*/getValueFunction(value.primitiveValue[key], arguments_)) != null) {
					break;
				}
			}
		} else {
			function_ = this.getValueFunction(value, arguments_);
		}

		return function_;
	}

	static retainOrReleaseValueComposites(retainingComposite, retainingValue) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				this.retainOrReleaseValueComposites(retainingComposite, retainingValue.primitiveValue[key]);
			}
		} else {
			this.retainOrReleaseComposite(retainingComposite, this.getValueComposite(retainingValue));
		}
	}

	static valueRetains(retainingValue, retainedComposite) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				let retainingValue_ = retainingValue.primitiveValue[key]

				if(this.valueRetains(retainingValue_, retainedComposite)) {
					return true;
				}
			}
		} else {
			return this.getValueComposite(retainingValue) === retainedComposite;
		}
	}

	static setStatements(callableComposite, statements) {
		if(!this.compositeIsCallable(callableComposite)) {
			return;
		}

		callableComposite.statements = statements ?? []
	}

	static getImport(composite, identifier) {
		return composite.imports[identifier]
	}

	static findImport(composite, identifier) {
		while(composite != null) {
			let import_ = this.getImport(composite, identifier);

			if(import_ != null) {
				return import_;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	static setImport(namespace, identifier, value) {
		if(!this.compositeIsNamespace(namespace)) {
			return;
		}

		namespace.imports[identifier] = value;
	}

	static deleteImport(composite, identifier) {
		let value = this.getComposite(this.getImport(composite, identifier));

		delete composite.imports[identifier]

		this.retainOrReleaseComposite(composite, value);
	}

	static importsRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.imports) {
			if(retainingComposite.imports[identifier] === this.getOwnID(retainedComposite)) {
				return true;
			}
		}
	}

	static getOperator(composite, identifier) {
		return composite.operators[identifier]
	}

	static addOperator(namespace, identifier) {
		if(!this.compositeIsNamespace(namespace)) {
			return;
		}

		return this.getOperator(namespace, identifier) ?? (namespace.operators[identifier] = []);
	}

	static removeOperator(composite, identifier) {
		delete composite.operators[identifier]
	}

	static getOperatorOverload(composite, identifier, matching) {
		let operator = this.getOperator(composite, identifier);

		if(operator != null) {
			for(let overload of operator) {
				if(matching == null || matching(overload)) {
					return overload;
				}
			}
		}
	}

	static findOperatorOverload(composite, identifier, matching) {
		while(composite != null) {
			let overload = this.getOperatorOverload(composite, identifier, matching);

			if(overload != null) {
				return overload;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	static setOperatorOverload(namespace, identifier, modifiers, associativity, precedence) {
		let type = modifiers.find(v => ['prefix', 'postfix', 'infix'].includes(v));

		if(type == null) {
			return;
		}

		let operator = this.addOperator(namespace, identifier);

		if(operator == null) {
			return;
		}

		let overload = this.getOperatorOverload(namespace, identifier, (v) => v.modifiers.includes(type)) ?? (operator[operator.push({})-1]);

		overload.modifiers = modifiers;
		overload.associativity = associativity;
		overload.precedence = precedence;
	}

	static getMember(composite, identifier) {
		return composite.members[identifier]
	}

	static addMember(composite, identifier) {
		return this.getMember(composite, identifier) ?? (composite.members[identifier] = []);
	}

	static removeMember(composite, identifier) {
		let member = this.getMember(composite, identifier);

		if(member == null) {
			return;
		}

		delete composite.members[identifier]

		for(let overload of member) {
			this.retainOrReleaseValueComposites(composite, overload.value);
		}
	}

	static getMemberOverload(composite, identifier, matching) {
		let member = this.getMember(composite, identifier);

		if(member == null) {  // Try to substitute pseudovariables
			let ID = {
			//	global: () => undefined,			  Global-object is no thing
				Global: () => 0,				   // Global-type
				super: () => composite.IDs.super,  // Super-object or a type
				Super: () => composite.IDs.Super,  // Super-type
				self: () => composite.IDs.self,	   // Self-object or a type
				Self: () => composite.IDs.Self,	   // Self-type
				sub: () => composite.IDs.sub,	   // Sub-object
				Sub: () => composite.IDs.Sub	   // Sub-type
			//	metaSelf: () => undefined,			  Self-object or a type (descriptor)
			//	arguments: () => undefined			  Function arguments array (needed?)
			}[identifier]?.();

			if(ID == null) {
				return;
			}

			member = [
				{
					modifiers: [],
					type: [{ predefined: 'Any' }],
					value: this.createValue('reference', ID),
					observers: []
				}
			]
		}

		if(member != null) {
			for(let overload of member) {
				if(matching == null) {
					return overload;
				}

				let matchingValue = matching(overload);

				if(matchingValue != null) {
					return this.getMemberOverloadProxy(overload, { matchingValue: matchingValue });
				}
			}
		}
	}

	static getMemberOverloadProxy(overload, additions) {
		if(overload.additions != null) {
			for(let key in additions) {
				overload.additions[key] = additions[key]
			}

			return overload;
		}

		return new Proxy(overload, {
			get(target, key, receiver) {
				if(key === 'additions') {
					return additions;
				}
				if(key in additions) {
					return additions[key]
				}

				return overload[key]
			},
			set(target, key, value) {
				if(key === 'additions') {
					return true;
				}
				if(key in additions) {
					additions[key] = value;

					return true;
				}

				overload[key] = value;

				return true;
			}
		});
	}

	/*
	 * Looking for member overload in Scope chain (scope).
	 *
	 * If search is internal, only first composite in chain will be checked.
	 */
	static findMemberOverload(composite, identifier, matching, internal) {
		while(composite != null) {
			let overload =
				this.findMemberOverloadInComposite(composite, identifier, matching) ??
				this.findMemberOverloadInObjectChain(composite, identifier, matching) ??
				this.findMemberOverloadInInheritanceChain(composite, identifier, matching);

			if(overload != null) {
				return overload;
			}

			if(internal) {
				break;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	/*
	 * Looking for member overload in a function or namespace.
	 *
	 * This types are slightly different from other. They can inherit or miss levels and eventually aren't
	 * inheritable nor instantiable themself. But they are still used as members storage
	 * and can participate in plain scope chains.
	 */
	static findMemberOverloadInComposite(composite, identifier, matching) {
		if(
			!this.compositeIsFunction(composite) &&
			!this.compositeIsNamespace(composite)
		) {
			return;
		}

		let overload = this.getMemberOverload(composite, identifier, matching);

		if(overload != null) {
			return this.getMemberOverloadProxy(overload, { owner: composite });
		}

		// TODO: Imports lookup
	}

	/*
	 * Looking for member overload in Object chain (super, self, sub).
	 *
	 * When a "virtual" overload is found, search oncely displaces to a lowest sub-object.
	 */
	static findMemberOverloadInObjectChain(object, identifier, matching) {
		if(!this.compositeIsObject(object)) {
			let object_ = this.getComposite(object.IDs.self);

			if(object_ === object || !this.compositeIsObject(object_)) {
				return;
			}

			object = object_;
		}

		let virtual;

		while(object != null) {  // Higher
			let overload = this.getMemberOverload(object, identifier, matching);

			if(overload != null) {
				if(!virtual && overload.modifiers.includes('virtual')) {
					virtual = true;

					let object_;

					while((object_ = this.getComposite(object.IDs.sub)) != null) {  // Lowest
						object = object_;
					}

					continue;
				}

				return this.getMemberOverloadProxy(overload, { owner: object });
			}

			object = this.getComposite(object.IDs.super);
		}
	}

	/*
	 * Looking for member overload in Inheritance chain (Super, Self).
	 */
	static findMemberOverloadInInheritanceChain(composite, identifier, matching) {
		if(this.compositeIsObject(composite)) {
			composite = this.getComposite(composite.IDs.Self);
		}

		while(composite != null) {
			let overload = this.getMemberOverload(composite, identifier, matching);

			if(overload != null) {
				return this.getMemberOverloadProxy(overload, { owner: composite });
			}

			composite = this.getComposite(composite.IDs.Super);
		}
	}

	/*
	 * Not specifying "internal" means use of direct declaration without search.
	 */
	static setMemberOverload(composite, identifier, modifiers, type, value, observers, matching, internal) {
		let overload = internal == null ? this.getMemberOverload(composite, identifier, matching) : this.findMemberOverload(composite, identifier, matching, internal);

		if(overload == null) {
			/*
			if(internal == null) {
				return;
			}
			*/

			let member = this.addMember(composite, identifier);

			overload = member[member.push({})-1]
		} else
		if(internal != null) {
			composite = overload.owner;
		}

		let OT = overload.type ?? [],  // Old/new type
			NT = type ?? [],
			OV = overload.value,  // Old/new value
			NV = value;

		overload.modifiers = modifiers;
		overload.type = type;
		overload.value = value;
		overload.observers = observers;

		if(OT !== NT) {
			let IDs = new Set([...OT, ...NT].map(v => v.reference));

			for(let ID of IDs) {
				this.retainOrReleaseComposite(composite, this.getComposite(ID));
			}
		}
		if(OV !== NV) {
			this.retainOrReleaseValueComposites(composite, OV);
			this.retainOrReleaseValueComposites(composite, NV);
		}
	}

	static membersRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.members) {
			for(let overload of retainingComposite.members[identifier] ?? []) {
				if(
					this.typeRetains(overload.type, retainedComposite) ||
					this.valueRetains(overload.value, retainedComposite) ||
					overload.observers.some(v => v.value === this.getOwnID(retainedComposite))
				) {
					return true;
				}
			}
		}
	}

	static getObserver() {}

	static findObserver() {}

	static setObserver(composite) {
		if(!['Class', 'Function', 'Namespace', 'Structure'].includes(composite.type[0]?.predefined)) {
			return;
		}
	}

	static deleteObserver(composite, observer) {
		composite.observers = composite.observers.filter(v => v !== observer);

		this.retainOrReleaseComposite(composite, this.getComposite(observer.value));
	}

	static observersRetain(retainingComposite, retainedComposite) {
		return retainingComposite.observers.some(v => v.value === this.getOwnID(retainedComposite));
	}

	static report(level, node, string) {
		let position = node?.range?.start ?? 0,
			location = this.tokens[position]?.location ?? {
				line: 0,
				column: 0
			}

		this.reports.push({
			level: level,
			position: position,
			location: location,
			string: (node != null ? node.type+' -> ' : '')+string
		});
	}

	static print(string) {
		this.reports.push({
			level: 0,
			position: undefined,
			location: undefined,
			string: string
		});
	}

	static reset(composites, preferences) {
		this.tokens = []
		this.tree = undefined;
		this.composites = composites ?? []
		this.scopes = []
		this.controlTransfer = undefined;
		this.preferences = {
			callStackSize: 128,
			allowedReportLevel: 2,
			metaprogrammingLevel: 3,
			arbitaryPrecisionArithmetics: true,
			...(preferences ?? {})
		}
		this.reports = []
	}

	static interpret(lexerResult, parserResult, composites = [], preferences = {}) {
		this.reset(composites, preferences);

		this.tokens = lexerResult.tokens;
		this.tree = structuredClone(parserResult.tree);

		this.rules.module();

		let result = {
			composites: this.composites,
			reports: this.reports
		}

		this.reset();

		return result;
	}
}
let memory = [
	{
		address: 0,
		inheritance: ['Namespace'],
		imports: [
			/*
			{
				identifier: 'Foundation',
				value: 0
			}
			*/
		],
		operators: [
			{
				modifiers: ['postfix'],
				identifier: '++',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['postfix'],
				identifier: '--',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['prefix'],
				identifier: '++',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['prefix'],
				identifier: '--',
				associativity: null,
				precedence: null
			},
			{
				modifiers: [/* 'private', */ 'infix'],
				identifier: '+',
				associativity: 'left',
				precedence: 64
			}
		],
		members: [
			{
				modifiers: ['public', 'static'],
				identifier: 'TestClass',
				typed: true,
				inheritance: [1],
				nillable: true,
				value: '1',
				observers: []
			},
			{
				modifiers: ['public', 'static', 'final'],
				identifier: 'testPi',
				typed: true,
				inheritance: ['float'],
				nillable: true,
				value: '3.14',
				observers: []
			},
			{
				modifiers: ['public', 'static'],
				identifier: 'testClassInstance',
				typed: true,
				inheritance: [2],
				nillable: true,
				value: '3',
				observers: []
			},
			{
				modifiers: ['public'],
				identifier: 'computedInstanceProperty',
				typed: false,
				inheritance: [],
				nillable: true,
				value: null,
				observers: [
					{
						identifier: 'willGet',  // 'get', 'didGet', 'willSet', 'set', 'didSet', 'willDelete', 'delete', 'didDelete'
						value: null  // Function address
					}
				]
			}
		],
		observers: {
			chain: [
				{
					identifier: 'willGet',  // ...
					value: null  // Function address
				}
			],
			subscript: []
		},
		scope: null,
		retains: 1
	},
	{
		address: 1,
		inheritance: ['Class'],
		generics: [],
		members: [
			{
				modifiers: ['public', 'static', 'final'],
				identifier: 'init',
				typed: true,
				inheritance: [
					/*
					{
						type: 'Function',
						generics: [],  // [null] - Unspecified list, [] - Empty list
						parameters: [
							{
								typed: false,
								inheritance: [],
								nillable: true,
								variadic: true
							},
							{
								typed: false,
								inheritance: [],
								nillable: true,
								variadic: true
							}
						],
						awaits: 0,  // -1 - Unspecified, 0 - False, 1 - True
						throws: 0,
						return: {
							typed: true,
							inheritance: ['bool'],
							nillable: false
						}
					}
					*/
					'Function'
				],
				nillable: false,
				value: '2',
				observers: []
			},
			{
				modifiers: ['public'],
				identifier: 'testVariable',
				typed: true,
				inheritance: ['int'],
				nillable: true,
				value: null,
				observers: []
			}
		],
		observers: {
			chain: [],
			subscript: []
		},
		scope: 0,
		retains: 1
	},
	{
		address: 2,
		inheritance: ['Function'],
		generics: [
			/*
			{
				identifier: 'A',
				typed: false,
				inheritance: [],
				nillable: true
			},
			*/
		],
		parameters: [
			{
				label: null,
				identifier: 'firstArgument',
				type: {
					inout: false,
					fixed: true,
					inheritance: ['float'],
					nillable: false,
					variadic: false
				}
			},
			{
				label: null,
				identifier: 'secondArgument',
				type: {
					inout: false,
					fixed: true,
					inheritance: ['bool'],
					nillable: true,
					variadic: false
				}
			}
		],
		awaits: false,
		throws: false,
		returnType: {
			fixed: true,
			inheritance: ['bool'],
			nillable: true
		},
		body: '\ttestVariable = 2 print(\'Test: \'+firstArgument)\n\n\treturn secondArgument',  // Body node from Parser's result
		scope: 1,
		retains: 1
	},
	{
		address: 3,
		inheritance: ['Object', 1],
		members: [
			{
				identifier: 'testVariable',
				value: '2'
			}
		],
		scopeAddress: 0,
		retainCount: 1
	}
]
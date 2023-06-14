// Rules for a type parts:
// - One TP can have only one collection flag.
// - TPs that have collection flags should also have children.
// - Non-first collection flags are attached as a separate child TPs.
// - Masks that are not mutually exclusive can be combined.
// - Masks can not be ambigously combined (e.g. variadic array type vs array of variadic type).
// - TPs should be minimalistic and can not be redundant.

//    inout A! | _? & Global<B>...
// 0: inout    |               ...
// 1:       A!
// 2:               &
// 3:            _?
// 4:                 Global< >
// 5:                        B

let memberType0 = [
	{
		super: undefined,
		flags: ['inout', 'union', 'variadic'],
		value: undefined
	},
	{
		super: 0,
		flags: ['reference', 'default'],
		value: 'A'  // Composite's address
	},
	{
		super: 0,
		flags: ['intersection'],
		value: undefined
	},
	{
		super: 2,
		flags: ['predefined', 'nillable'],
		value: '_'
	},
	{
		super: 2,
		flags: ['reference', 'genericArguments'],
		value: 'Global'
	},
	{
		super: 4,
		flags: ['reference'],
		value: 'B'
	}
]

//    (A | B) & C
// 0:         &
// 1: (  |  )
// 2:  A
// 3:      B
// 4:           C

let memberType1 = [
	{
		super: undefined,
		flags: ['intersection'],
		value: undefined
	},
	{
		super: 0,
		flags: ['union'],
		value: undefined
	},
	{
		super: 1,
		flags: ['reference'],
		value: 'A'
	},
	{
		super: 1,
		flags: ['reference'],
		value: 'B'
	},
	{
		super: 0,
		flags: ['reference'],
		value: 'C'
	}
]

//    () / Function
// 0: () / Function

let memberType2 = [
	{
		super: undefined,
		flags: ['predefined'],
		value: 'Function'
	}
]

//    <...>(...) awaits? throws? -> _?
// 0: <   >      awaits? throws?
// 1:  ...
// 2:      (   )
// 3:       ...
// 4:                            -> _?

let memberType3 = [
	{
		super: undefined,
		flags: ['function', 'genericParameters', 'awaits?', 'throws?'],
		value: undefined
	},
	{
		super: 0,
		flags: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		flags: ['parameters'],
		value: undefined
	},
	{
		super: 2,
		flags: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		flags: ['return', 'predefined', 'nillable'],
		value: '_'
	}
]

//    ([]..., ...) awaits throws -> _
// 0: (          ) awaits throws
// 1:  []...
// 2:       , ...
// 3:                            -> _

let memberType3 = [
	{
		super: undefined,
		flags: ['function', 'parameters', 'awaits', 'throws'],
		value: undefined
	},
	{
		super: 0,
		flags: ['array', 'variadic'],
		value: undefined
	},
	{
		super: 0,
		flags: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		flags: ['return', 'predefined'],
		value: '_'
	}
]

//    (() -> _)?
// 0: (()     )?
// 1:     -> _

let memberType4 = [
	{
		super: undefined,
		flags: ['function', 'nillable'],
		value: undefined
	},
	{
		super: 0,
		flags: ['return', 'predefined'],
		value: '_'
	}
]

//    <T>()
// 0: < >()
// 1:  T

let memberType5 = [
	{
		super: undefined,
		flags: ['function', 'genericParameters'],
		value: undefined
	},
	{
		super: 0,
		flags: ['predefined', 'nillable'],
		value: '_'
	}
]

//    [] / [_?] / Array<_?>
// 0: [] / [  ] / Array<  >
// 1:    /  _?  /       _?

let memberType6 = [  // Fallback
	{
		super: undefined,
		flags: ['array'],
		value: undefined
	},
	{
		super: 0,
		flags: ['predefined', 'nillable'],
		value: '_'
	}
]

let memberType7 = [  // Preferable and default
	{
		super: undefined,
		flags: ['reference', 'genericArguments'],
		value: 'Array'
	},
	{
		super: 0,
		flags: ['predefined', 'nillable'],
		value: '_'
	}
]

//    class A: B<_?>
// 0: class A:
// 1:          B<  >
// 2:            _?

let compositeType0 = [
	{
		super: undefined,
		flags: ['predefined', 'inheritance'],
		value: 'Class'
	},
	{
		super: 0,
		flags: ['reference', 'genericArguments'],
		value: 'B'
	},
	{
		super: 1,
		flags: ['predefined', 'nillable'],
		value: '_'
	}
]
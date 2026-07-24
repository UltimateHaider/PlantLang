const { LLVMEmitter } = require('../src/codegen/llvm/llvm_emitter');
const { CodeWordsChecker } = require('../src/security/codewords_governance');

let passed = 0, failed = 0;

function assert(cond, label) {
  if (cond) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
}

function assertEqual(a, b, label) {
  if (a === b) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  else { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ' (' + JSON.stringify(a) + ' !== ' + JSON.stringify(b) + ')'); }
}

function assertNotThrows(fn, label) {
  try { fn(); passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
  catch (e) { failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label + ': ' + e.message); }
}

function assertThrows(fn, label) {
  try { fn(); failed++; console.log('  \u001b[31m\u2717\u001b[0m ' + label); }
  catch (e) { passed++; console.log('  \u001b[32m\u2713\u001b[0m ' + label); }
}

function main() {

// ═══════════════════════════════════════════════════════════════
//  1. PlantMap — Construction / Insertion / Lookup
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 1. PlantMap: Create / Set / Get ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: {
        type: 'MapLiteral',
        entries: [
          { type: 'KeyValuePair', key: { type: 'Literal', literalType: 'STRING', value: 'a' }, value: { type: 'Literal', literalType: 'NUMBER', value: 1 } },
          { type: 'KeyValuePair', key: { type: 'Literal', literalType: 'STRING', value: 'b' }, value: { type: 'Literal', literalType: 'NUMBER', value: 2 } },
        ]
      }},
    ]
  };
  assertNotThrows(() => { const ir = emitter.generate(program); assert(ir.includes('@plant_map_create'), 'IR calls plant_map_create'); }, 'Map literal generates IR without error');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [] } },
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'k' }, valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 42 }, mapIdent: 'm' },
    ]
  };
  assertNotThrows(() => { const ir = emitter.generate(program); assert(ir.includes('@plant_map_set'), 'IR calls plant_map_set'); }, 'Create empty map + LINK generates IR');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [] } },
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'x' }, valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 10 }, mapIdent: 'm' },
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'y' }, valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 20 }, mapIdent: 'm' },
    ]
  };
  const ir = emitter.generate(program);
  const setCount = (ir.match(/call.*@plant_map_set/g) || []).length;
  assertEqual(setCount, 2, 'two LINK statements produce two plant_map_set calls');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [] } },
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'key1' }, valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 100 }, mapIdent: 'm' },
      { type: 'ShowStatement', expr: { type: 'MapLiteral', entries: [] } },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'Map operations in pipeline do not crash');
})();

// ═══════════════════════════════════════════════════════════════
//  2. FOR...IN — Iteration over Arrays and Maps
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 2. FOR...IN: Array and Map Iteration ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'arr', varType: 'LIST', valueExpr: { type: 'ArrayLiteral', elements: [
        { type: 'Literal', literalType: 'NUMBER', value: 10 },
        { type: 'Literal', literalType: 'NUMBER', value: 20 },
        { type: 'Literal', literalType: 'NUMBER', value: 30 },
      ]}},
      {
        type: 'ForInStatement',
        iterVar: 'x',
        sourceExpr: { type: 'Identifier', name: 'arr' },
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Identifier', name: 'x' } },
        ]
      },
    ]
  };
  assertNotThrows(() => { const ir = emitter.generate(program); assert(ir.includes('forin.loop'), 'IR contains forin loop label'); }, 'FOR...IN over array generates IR');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'ForInStatement',
        iterVar: 'k',
        sourceExpr: { type: 'MapLiteral', entries: [
          { key: { type: 'Literal', literalType: 'STRING', value: 'a' }, value: { type: 'Literal', literalType: 'NUMBER', value: 1 } },
        ]},
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Identifier', name: 'k' } },
        ]
      },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'FOR...IN over map literal generates IR');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'arr', varType: 'LIST', valueExpr: { type: 'ArrayLiteral', elements: [] } },
      {
        type: 'ForInStatement',
        iterVar: 'x',
        sourceExpr: { type: 'Identifier', name: 'arr' },
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Identifier', name: 'x' } },
        ]
      },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'FOR...IN over empty array generates IR');
})();

// ═══════════════════════════════════════════════════════════════
//  3. WEATHER — Error-handling block translation
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 3. WEATHER / SHELTER / CALM ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'WeatherStatement',
        conditionExpr: null,
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Literal', literalType: 'STRING', value: 'try body' } },
        ],
        shelterClauses: [
          { type: 'ShelterStatement', stormType: 'ZERO_STORM', errVar: null, bodyStatements: [
            { type: 'ShowStatement', expr: { type: 'Literal', literalType: 'STRING', value: 'caught' } },
          ]},
        ],
        calmClause: { type: 'CalmStatement', bodyStatements: [] },
      },
    ]
  };
  const ir = emitter.generate(program);
  assert(ir.includes('weather.body'), 'IR contains weather.body label');
  assert(ir.includes('weather.shelter'), 'IR contains weather.shelter label');
  assert(ir.includes('weather.calm'), 'IR contains weather.calm label');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'WeatherStatement',
        conditionExpr: { type: 'Literal', literalType: 'STRING', value: 'storm' },
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Literal', literalType: 'STRING', value: 'in storm' } },
        ],
        shelterClauses: [],
        calmClause: null,
      },
    ]
  };
  const ir = emitter.generate(program);
  assert(ir.includes('@plant_env_set_weather'), 'WEATHER with condition calls plant_env_set_weather');
})();

// ═══════════════════════════════════════════════════════════════
//  4. SPECIES Declaration
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 4. SPECIES Declaration ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'SpeciesDeclaration',
        name: 'Greeter',
        parentName: null,
        fields: [{ name: 'msg', varType: 'TX', defaultExpr: null }],
        actions: [],
      },
    ]
  };
  const ir = emitter.generate(program);
  assert(ir.includes('@plant_entity_set_species'), 'SPECIES declaration calls plant_entity_set_species');
  assert(ir.includes('Greeter'), 'IR contains species name constant');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'SpeciesDeclaration',
        name: 'Dog',
        parentName: 'Animal',
        fields: [{ name: 'breed', varType: 'TX', defaultExpr: null }],
        actions: [],
      },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'SPECIES with parent generates IR');
})();

// ═══════════════════════════════════════════════════════════════
//  5. CodeWords Safety Checks — New Node Types Not Flagged
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 5. CodeWords: New Node Types Not Flagged ---\u001b[0m');

(function() {
  const checker = new CodeWordsChecker([]);
  const program = {
    type: 'Program',
    statements: [
      { type: 'LinkStatement', line: 1, column: 1, keyExpr: 'a', valueExpr: '1', mapIdent: 'm' },
      { type: 'ForInStatement', line: 2, column: 1, iterVar: 'x', sourceExpr: 'arr', bodyStatements: [] },
      { type: 'WeatherStatement', line: 3, column: 1, conditionExpr: null, bodyStatements: [], shelterClauses: [], calmClause: null },
      { type: 'SpeciesDeclaration', line: 4, column: 1, name: 'Foo', fields: [], actions: [] },
    ]
  };
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'LINK/FOR...IN/WEATHER/SPECIES produce zero violations without directives');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_NETWORK']);
  const program = {
    type: 'Program',
    statements: [
      { type: 'LinkStatement', keyExpr: 'a', valueExpr: '1', mapIdent: 'm' },
      { type: 'HarvestStatement', line: 2, column: 1, urlExpr: 'http://test' },
    ]
  };
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'HARVEST + LINK with #ALLOW_NETWORK passes');
})();

(function() {
  const checker = new CodeWordsChecker([]);
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: null },
      { type: 'LinkStatement', keyExpr: 'a', valueExpr: '1', mapIdent: 'm' },
    ]
  };
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'LINK without any network directive passes (non-network op)');
})();

(function() {
  const checker = new CodeWordsChecker(['#ALLOW_HARVEST']);
  const program = {
    type: 'Program',
    statements: [
      { type: 'ForInStatement', iterVar: 'k', sourceExpr: { type: 'Identifier', name: 'm' }, bodyStatements: [] },
      { type: 'HarvestStatement', line: 2, column: 1, urlExpr: 'http://test' },
    ]
  };
  const violations = checker.checkAST(program, 'test.plant');
  assertEqual(violations.length, 0, 'FOR...IN + HARVEST with #ALLOW_HARVEST passes');
})();

// ═══════════════════════════════════════════════════════════════
//  6. Full Pipeline Integration — Parse → Generate → No Errors
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 6. Pipeline Integration ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [
        { key: { type: 'Literal', literalType: 'STRING', value: 'name' }, value: { type: 'Literal', literalType: 'STRING', value: 'plant' } },
      ]}},
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'version' }, valueExpr: { type: 'Literal', literalType: 'NUMBER', value: 42 }, mapIdent: 'm' },
      {
        type: 'ForInStatement',
        iterVar: 'k',
        sourceExpr: { type: 'Identifier', name: 'm' },
        bodyStatements: [
          { type: 'ShowStatement', expr: { type: 'Identifier', name: 'k' } },
        ]
      },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'End-to-end MAP + LINK + FOR...IN pipeline generates IR');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'WeatherStatement',
        conditionExpr: null,
        bodyStatements: [
          { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [] } },
          { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'inside' }, valueExpr: { type: 'Literal', literalType: 'STRING', value: 'weather' }, mapIdent: 'm' },
        ],
        shelterClauses: [
          { type: 'ShelterStatement', stormType: 'ANY_STORM', errVar: null, bodyStatements: [] },
        ],
        calmClause: { type: 'CalmStatement', bodyStatements: [] },
      },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'WEATHER block with nested MAP/LINK generates IR');
})();

(function() {
  const emitter = new LLVMEmitter();
  const program = {
    type: 'Program',
    statements: [
      {
        type: 'SpeciesDeclaration',
        name: 'Container',
        fields: [{ name: 'data', varType: 'MAP', defaultExpr: null }],
        actions: [],
      },
      { type: 'CreateStatement', identifier: 'm', varType: 'MAP', valueExpr: { type: 'MapLiteral', entries: [] } },
      { type: 'LinkStatement', keyExpr: { type: 'Literal', literalType: 'STRING', value: 'spec' }, valueExpr: { type: 'Literal', literalType: 'STRING', value: 'test' }, mapIdent: 'm' },
    ]
  };
  assertNotThrows(() => emitter.generate(program), 'SPECIES + MAP + LINK pipeline generates IR');
})();

// ═══════════════════════════════════════════════════════════════
//  7. IR Correctness — Verify Declarations Present
// ═══════════════════════════════════════════════════════════════
console.log('\u001b[1m--- 7. IR Declaration Correctness ---\u001b[0m');

(function() {
  const emitter = new LLVMEmitter();
  const ir = emitter.generate({
    type: 'Program',
    statements: []
  });
  assert(ir.includes('declare i8* @plant_map_create'), 'IR declares plant_map_create');
  assert(ir.includes('declare void @plant_map_set'), 'IR declares plant_map_set');
  assert(ir.includes('declare i8* @plant_map_get'), 'IR declares plant_map_get');
  assert(ir.includes('declare void @plant_sys_action'), 'IR declares plant_sys_action');
  assert(ir.includes('declare void @plant_env_set_weather'), 'IR declares plant_env_set_weather');
  assert(ir.includes('declare i8* @plant_env_get_weather'), 'IR declares plant_env_get_weather');
  assert(ir.includes('declare void @plant_entity_set_species'), 'IR declares plant_entity_set_species');
})();

// ═══════════════════════════════════════════════════════════════
//  Summary
// ═══════════════════════════════════════════════════════════════
console.log('');
console.log(`\u001b[1mResults: ${passed} passed, ${failed} failed\u001b[0m`);
if (failed > 0) process.exit(1);
}

main();

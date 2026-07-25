'use strict';

class ExhaustivenessChecker {
  constructor(interpreter) {
    this._interpreter = interpreter;
    this._errors = [];
  }

  check(programNode) {
    this._errors = [];
    this._walk(programNode);
    return this._errors;
  }

  getErrors() {
    return this._errors;
  }

  _walk(node) {
    if (!node || typeof node !== 'object') return;
    if (node.type === 'MatchStatement' || node.type === 'MatchExpr') {
      this._checkMatch(node);
    }
    for (const key of Object.keys(node)) {
      if (key === 'parent') continue;
      const val = node[key];
      if (Array.isArray(val)) {
        for (const item of val) this._walk(item);
      } else if (val && typeof val === 'object' && val.type) {
        this._walk(val);
      }
    }
  }

  _checkMatch(node) {
    const subjectRef = node.subjectExpr;
    let choiceType = null;
    if (subjectRef && subjectRef.type === 'Identifier') {
      const name = subjectRef.name;
      if (this._interpreter.choices.has(name)) {
        choiceType = name;
      }
    }

    if (!choiceType) return;

    const choiceDef = this._interpreter.choices.get(choiceType);
    if (!choiceDef) return;

    const handledVariants = new Set();
    let hasWildcard = false;

    for (const clause of node.clauses) {
      if (clause.variantName === '_' || clause.variantName === 'ANY' || clause.variantName === 'ELSE') {
        hasWildcard = true;
      } else {
        handledVariants.add(clause.variantName);
      }
    }

    if (hasWildcard) return;

    for (const variant of choiceDef) {
      if (!handledVariants.has(variant.name)) {
        this._errors.push(
          `CompileError: Non-exhaustive MATCH statement. Missing case: ${variant.name}`
        );
      }
    }
  }
}

module.exports = { ExhaustivenessChecker };

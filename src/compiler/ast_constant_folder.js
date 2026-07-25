'use strict';

const { LiteralNode, ConstDeclarationNode } = require('../../core/ast');

class ASTConstantFolder {
  constructor() {
    this._consts = new Map();
    this._modified = false;
  }

  fold(programNode) {
    if (!programNode || !programNode.statements) return programNode;
    this._modified = false;
    this._collectConsts(programNode);
    this._foldNode(programNode);
    return programNode;
  }

  _collectConsts(node) {
    if (!node || typeof node !== 'object') return;
    if (node.type === 'ConstDeclaration') {
      const val = node.valueExpr;
      if (val && typeof val === 'object' && val.type === 'Literal') {
        this._consts.set(node.identifier, val.value);
      }
    }
    for (const key of Object.keys(node)) {
      if (key === 'parent') continue;
      const val = node[key];
      if (Array.isArray(val)) {
        for (const item of val) this._collectConsts(item);
      } else if (val && typeof val === 'object' && val.type) {
        this._collectConsts(val);
      }
    }
  }

  _foldNode(node) {
    if (!node || typeof node !== 'object') return;
    for (const key of Object.keys(node)) {
      if (key === 'parent') continue;
      const val = node[key];
      if (Array.isArray(val)) {
        for (let i = 0; i < val.length; i++) {
          if (val[i] && typeof val[i] === 'object' && val[i].type) {
            const folded = this._foldExpr(val[i]);
            if (folded !== val[i]) {
              val[i] = folded;
              this._modified = true;
            }
            this._foldNode(val[i]);
          }
        }
      } else if (val && typeof val === 'object' && val.type) {
        const folded = this._foldExpr(val);
        if (folded !== val) {
          node[key] = folded;
          this._modified = true;
        }
        this._foldNode(val);
      }
    }
  }

  _foldExpr(expr) {
    if (!expr || typeof expr !== 'object' || expr.type === 'Literal') return expr;

    if (expr.type === 'Identifier') {
      if (this._consts.has(expr.name)) {
        const val = this._consts.get(expr.name);
        return new LiteralNode(val, typeof val === 'string' ? 'STRING' : typeof val === 'boolean' ? 'FACT' : 'NUMBER', { line: expr.line, column: expr.column, depth: expr.depth });
      }
      return expr;
    }

    if (expr.type === 'BinaryOp' || expr.type === 'BinaryExpression') {
      const left = this._foldExpr(expr.left || expr.lhs);
      const right = this._foldExpr(expr.right || expr.rhs);
      if (left.type !== 'Literal' || right.type !== 'Literal') return expr;
      const op = (expr.operator || expr.op).toUpperCase();
      const lv = left.value;
      const rv = right.value;
      let result;
      switch (op) {
        case '+': result = typeof lv === 'number' && typeof rv === 'number' ? lv + rv : String(lv) + String(rv); break;
        case '-': result = lv - rv; break;
        case '*': result = lv * rv; break;
        case '/': result = lv / rv; break;
        case '%': result = lv % rv; break;
        case '**': result = Math.pow(lv, rv); break;
        case 'IS': result = lv === rv; break;
        case 'IS_NOT': result = lv !== rv; break;
        case 'GREATER_THAN': case 'GT': result = lv > rv; break;
        case 'LESS_THAN': case 'LT': result = lv < rv; break;
        case 'GTE': result = lv >= rv; break;
        case 'LTE': result = lv <= rv; break;
        case 'AND': result = lv && rv; break;
        case 'OR': result = lv || rv; break;
        default: return expr;
      }
      return new LiteralNode(result, typeof result === 'boolean' ? 'FACT' : typeof result === 'string' ? 'STRING' : 'NUMBER', { line: expr.line, column: expr.column, depth: expr.depth });
    }

    if (expr.type === 'UnaryOp' || expr.type === 'UnaryExpression') {
      const operand = this._foldExpr(expr.operand);
      if (operand.type !== 'Literal') return expr;
      const op = (expr.operator || expr.op).toUpperCase();
      let result;
      switch (op) {
        case 'NOT': result = !operand.value; break;
        case '-': result = -operand.value; break;
        default: return expr;
      }
      return new LiteralNode(result, typeof result === 'boolean' ? 'FACT' : 'NUMBER', { line: expr.line, column: expr.column, depth: expr.depth });
    }

    return expr;
  }

  wasModified() {
    return this._modified;
  }
}

module.exports = { ASTConstantFolder };

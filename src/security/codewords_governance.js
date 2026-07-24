'use strict';

const ALLOWED_DIRECTIVES = {
  '#ALLOW_NETWORK': { type: 'network', description: 'Permit HARVEST and LISTEN BRANCH' },
  '#ALLOW_HARVEST': { type: 'harvest', description: 'Permit HARVEST outbound requests' },
  '#ALLOW_LISTEN':  { type: 'listen', description: 'Permit LISTEN BRANCH socket server' },
};

const DEFAULT_DIRECTIVES = new Set();
const NETWORK_NODES = new Set(['HarvestStatement', 'ListenBranchStatement']);

class SecurityViolationError extends Error {
  constructor(nodeType, requiredDirective, details) {
    const msg = `[CodeWords] ${nodeType} requires "${requiredDirective}" directive. ${details || ''}`;
    super(msg);
    this.name = 'SecurityViolationError';
    this.nodeType = nodeType;
    this.requiredDirective = requiredDirective;
  }
}

class CodeWordsChecker {
  constructor(directives = []) {
    this._directives = new Set(directives.map(d => d.trim()));
    this._violations = [];
  }

  static parseDirectives(source) {
    const dirs = [];
    for (const line of source.split('\n')) {
      const trimmed = line.trim();
      if (trimmed.startsWith('#ALLOW_')) {
        dirs.push(trimmed);
      }
    }
    return dirs;
  }

  hasDirective(name) {
    if (this._directives.has(name)) return true;
    if (name === '#ALLOW_HARVEST' || name === '#ALLOW_LISTEN') {
      return this._directives.has('#ALLOW_NETWORK');
    }
    return false;
  }

  checkNode(node, sourcePath) {
    if (!node || !node.type) return true;
    if (!NETWORK_NODES.has(node.type)) return true;
    const needs = this._requiredDirective(node);
    if (!needs) return true;
    if (this.hasDirective(needs)) return true;
    const violation = new SecurityViolationError(
      node.type, needs,
      `at ${sourcePath || 'unknown'}:${node.line || '?'}:${node.column || '?'}`
    );
    this._violations.push(violation);
    return false;
  }

  checkAST(programNode, sourcePath) {
    this._violations = [];
    this._walk(programNode, sourcePath);
    return this._violations;
  }

  _walk(node, sourcePath) {
    if (!node || typeof node !== 'object') return;
    this.checkNode(node, sourcePath);
    for (const key of Object.keys(node)) {
      if (key === 'parent') continue;
      const val = node[key];
      if (Array.isArray(val)) {
        for (const item of val) this._walk(item, sourcePath);
      } else if (val && typeof val === 'object' && val.type) {
        this._walk(val, sourcePath);
      }
    }
  }

  _requiredDirective(node) {
    if (node.type === 'HarvestStatement') return '#ALLOW_HARVEST';
    if (node.type === 'ListenBranchStatement') return '#ALLOW_LISTEN';
    return null;
  }

  getViolations() {
    return this._violations;
  }

  getDirectives() {
    return Array.from(this._directives);
  }

  static getValidDirectives() {
    return Object.keys(ALLOWED_DIRECTIVES);
  }
}

module.exports = { CodeWordsChecker, SecurityViolationError, ALLOWED_DIRECTIVES };

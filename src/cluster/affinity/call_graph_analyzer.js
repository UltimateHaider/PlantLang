const DEFAULT_MAX_DEPTH = 3;

class CallGraphAnalyzer {
  constructor(options = {}) {
    this._maxDepth = options.maxDepth || parseInt(process.env.CALL_GRAPH_MAX_DEPTH || String(DEFAULT_MAX_DEPTH), 10);
    this._graph = new Map();
    this._edgeWeights = new Map();
    this._affinityGroups = [];
    this._analyzed = false;
  }

  configure(key, value) {
    if (key === 'CALL_GRAPH_MAX_DEPTH') {
      const v = parseInt(value, 10);
      if (v >= 1 && v <= 10) this._maxDepth = v;
    }
  }

  addFunction(name, calls = []) {
    if (!this._graph.has(name)) {
      this._graph.set(name, []);
    }
    for (const callee of calls) {
      const edgeKey = name + '->' + callee;
      this._edgeWeights.set(edgeKey, (this._edgeWeights.get(edgeKey) || 0) + 1);
      const deps = this._graph.get(name);
      if (!deps.includes(callee)) deps.push(callee);
      if (!this._graph.has(callee)) {
        this._graph.set(callee, []);
      }
    }
  }

  setEdgeWeight(caller, callee, weight) {
    const edgeKey = caller + '->' + callee;
    this._edgeWeights.set(edgeKey, weight);
    const deps = this._graph.get(caller);
    if (deps && !deps.includes(callee)) deps.push(callee);
    if (!this._graph.has(callee)) this._graph.set(callee, []);
  }

  getGraph() {
    return this._graph;
  }

  getNodeCount() {
    return this._graph.size;
  }

  getEdgeCount() {
    let count = 0;
    for (const deps of this._graph.values()) count += deps.length;
    return count;
  }

  getEdgeWeight(caller, callee) {
    return this._edgeWeights.get(caller + '->' + callee) || 0;
  }

  getDepth(name, visited = new Set(), depth = 0) {
    if (depth > this._maxDepth) return this._maxDepth;
    if (visited.has(name)) return depth;
    visited.add(name);
    const deps = this._graph.get(name);
    if (!deps || deps.length === 0) return depth;
    let maxDepth = depth;
    for (const callee of deps) {
      if (!visited.has(callee)) {
        const d = this.getDepth(callee, new Set(visited), depth + 1);
        if (d > maxDepth) maxDepth = d;
      }
    }
    return maxDepth;
  }

  static buildFromAST(astFunctions, options = {}) {
    const analyzer = new CallGraphAnalyzer(options);
    const visited = new Set();
    function walk(funcName) {
      if (visited.has(funcName)) return;
      visited.add(funcName);
      const func = astFunctions.find(f => f.name === funcName);
      if (!func) return;
      const calls = [];
      for (const call of (func.calls || [])) {
        calls.push(call);
        walk(call);
      }
      analyzer.addFunction(funcName, calls);
    }
    for (const func of astFunctions) walk(func.name);
    return analyzer;
  }

  computeAffinityGroups() {
    this._affinityGroups = [];
    const assigned = new Set();

    const adjacency = {};
    for (const [caller, callees] of this._graph) {
      if (!adjacency[caller]) adjacency[caller] = {};
      for (const callee of callees) {
        const w = this.getEdgeWeight(caller, callee);
        adjacency[caller][callee] = w;
        if (!adjacency[callee]) adjacency[callee] = {};
        adjacency[callee][caller] = (adjacency[callee][caller] || 0) + w;
      }
    }

    const nodes = Array.from(this._graph.keys());

    for (const startNode of nodes) {
      if (assigned.has(startNode)) continue;
      const group = this._louvainCommunityDetect(startNode, adjacency, assigned);
      if (group.length > 0) {
        this._affinityGroups.push(group);
        for (const n of group) assigned.add(n);
      }
    }

    for (const node of nodes) {
      if (!assigned.has(node)) {
        this._affinityGroups.push([node]);
        assigned.add(node);
      }
    }

    this._analyzed = true;
    return this._affinityGroups;
  }

  _louvainCommunityDetect(startNode, adjacency, assigned) {
    const group = [];
    const queue = [startNode];
    const visited = new Set();
    const internalWeights = new Map();

    while (queue.length > 0) {
      const current = queue.shift();
      if (visited.has(current) || assigned.has(current)) continue;
      visited.add(current);

      const neighbors = adjacency[current] || {};
      let internalWeight = 0;
      for (const [neighbor, w] of Object.entries(neighbors)) {
        if (visited.has(neighbor) || group.includes(neighbor)) {
          internalWeight += w;
        }
      }
      internalWeights.set(current, internalWeight);

      let totalWeight = 0;
      for (const w of Object.values(neighbors)) totalWeight += w;

      const depth = this.getDepth(current);
      const modularityGain = totalWeight > 0 ? (internalWeight / totalWeight) - (depth / this._maxDepth) * 0.1 : -1;

      if (modularityGain >= 0.1 || group.length === 0) {
        group.push(current);
        for (const neighbor of Object.keys(neighbors)) {
          if (!visited.has(neighbor) && !assigned.has(neighbor) && !queue.includes(neighbor)) {
            const neighborDepth = this.getDepth(neighbor);
            if (neighborDepth <= this._maxDepth) queue.push(neighbor);
          }
        }
      }
    }
    return group;
  }

  getAffinityGroups() {
    if (!this._analyzed) this.computeAffinityGroups();
    return this._affinityGroups;
  }

  getGroupForFunction(funcName) {
    if (!this._analyzed) this.computeAffinityGroups();
    for (const group of this._affinityGroups) {
      if (group.includes(funcName)) return group;
    }
    return null;
  }

  assignToNode(funcName, nodeId, placement) {
    if (!placement) placement = new Map();
    const group = this.getGroupForFunction(funcName);
    if (group) {
      for (const fn of group) {
        if (!placement.has(fn)) placement.set(fn, nodeId);
      }
    } else {
      placement.set(funcName, nodeId);
    }
    return placement;
  }

  computePlacement(nodeIds) {
    if (!this._analyzed) this.computeAffinityGroups();
    const placement = new Map();
    let nodeIndex = 0;
    for (const group of this._affinityGroups) {
      const targetNode = nodeIds[nodeIndex % nodeIds.length];
      for (const fn of group) {
        placement.set(fn, targetNode);
      }
      nodeIndex++;
    }
    return placement;
  }

  getMaxDepth() { return this._maxDepth; }

  getStats() {
    return {
      nodeCount: this._graph.size,
      edgeCount: this.getEdgeCount(),
      maxDepth: this._maxDepth,
      groupsCount: this._affinityGroups.length,
      analyzed: this._analyzed,
    };
  }
}

module.exports = { CallGraphAnalyzer, DEFAULT_MAX_DEPTH };

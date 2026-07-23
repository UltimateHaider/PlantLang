'use strict';

// ═══════════════════════════════════════════════════════════════
//  src/memory/allocator.js
//  v0.38.0 — Memory Region Inheritance for Nested Structs
//  FAST: ArenaAllocator — O(1) bump allocation, cascading scope reset
//  PERSISTENT: ARCHeap — cascading reference counting
// ═══════════════════════════════════════════════════════════════

/**
 * FAST mission ArenaAllocator.
 * O(1) bump-pointer allocation. Supports child arenas for nested structs.
 * When parent arena resets, all child arenas are also reclaimed.
 */
class ArenaAllocator {
  constructor(capacity = 65536) {
    this._buffer = Buffer.alloc(capacity);
    this._capacity = capacity;
    this._offset = 0;
    this._children = [];
  }

  /** Allocate `size` bytes, returns offset pointer. */
  alloc(size) {
    const aligned = ((size + 7) >>> 3) << 3; // 8-byte alignment
    if (this._offset + aligned > this._capacity) {
      throw new Error('ArenaAllocator out of memory');
    }
    const ptr = this._offset;
    this._offset += aligned;
    return ptr;
  }

  /** Write a value at a given pointer offset. */
  write(ptr, data) {
    if (typeof data === 'number') {
      this._buffer.writeDoubleLE(data, ptr);
    } else if (typeof data === 'string') {
      const buf = Buffer.from(data, 'utf8');
      buf.copy(this._buffer, ptr);
    } else if (Buffer.isBuffer(data)) {
      data.copy(this._buffer, ptr);
    }
  }

  /** Read a value at a given pointer offset. */
  read(ptr, type = 'double') {
    if (type === 'double') return this._buffer.readDoubleLE(ptr);
    if (type === 'int32') return this._buffer.readInt32LE(ptr);
    return this._buffer.toString('utf8', ptr, this._offset);
  }

  /** Reset allocator — O(1). Cascades to all child arenas. */
  reset() {
    this._offset = 0;
    for (const child of this._children) child.reset();
    this._children = [];
  }

  /** Create a child arena that inherits this arena's lifetime. */
  createChild() {
    const child = new ArenaAllocator(this._capacity);
    this._children.push(child);
    return child;
  }

  get used() { return this._offset; }
  get remaining() { return this._capacity - this._offset; }
}

/**
 * PERSISTENT mission ARCHeap.
 * Automatic Reference Counting with cascading propagation through nested references.
 */
class ARCHeap {
  constructor() {
    this._objects = new Map();
    this._refCounts = new Map();
    this._references = new Map(); // parent -> Set<child>
  }

  /**
   * Allocate an object with an initial reference count of 1.
   * @param {string} id - object identifier
   * @param {any} value - the object value
   */
  alloc(id, value) {
    if (this._objects.has(id)) throw new Error(`ARCHeap: object "${id}" already exists`);
    this._objects.set(id, value);
    this._refCounts.set(id, 1);
    this._references.set(id, new Set());
    return id;
  }

  /**
   * Retain (increment refcount). Cascades to nested references.
   * @param {string} id - object identifier
   */
  retain(id) {
    const count = this._refCounts.get(id);
    if (count === undefined) throw new Error(`ARCHeap: unknown object "${id}"`);
    this._refCounts.set(id, count + 1);
    // Cascade to children
    const children = this._references.get(id);
    if (children) {
      for (const childId of children) this.retain(childId);
    }
  }

  /**
   * Release (decrement refcount). Cascades to nested references.
   * When refcount reaches 0, the object and all its transitive children are freed.
   * @param {string} id - object identifier
   */
  release(id) {
    const count = this._refCounts.get(id);
    if (count === undefined) throw new Error(`ARCHeap: unknown object "${id}"`);
    const newCount = count - 1;
    if (newCount <= 0) {
      // Free this object and cascade
      this._free(id);
    } else {
      this._refCounts.set(id, newCount);
    }
  }

  /** Get an object's value. */
  get(id) {
    return this._objects.get(id);
  }

  /** Set a new value (preserves refcount). */
  set(id, value) {
    if (!this._objects.has(id)) throw new Error(`ARCHeap: unknown object "${id}"`);
    this._objects.set(id, value);
  }

  /**
   * Register a nested reference (parent -> child).
   * This ensures cascading retain/release.
   */
  addReference(parentId, childId) {
    if (!this._references.has(parentId)) this._references.set(parentId, new Set());
    this._references.get(parentId).add(childId);
  }

  /** Internal: free an object and cascade to children. */
  _free(id) {
    const children = this._references.get(id) || new Set();
    this._objects.delete(id);
    this._refCounts.delete(id);
    this._references.delete(id);
    // Cascade to children
    for (const childId of children) this._free(childId);
  }

  /** Check if an object exists. */
  has(id) { return this._objects.has(id); }

  /** Current number of live objects. */
  get liveCount() { return this._objects.size; }
}

module.exports = { ArenaAllocator, ARCHeap };

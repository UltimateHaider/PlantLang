const CAPABILITIES = {
  FILE_READ: 'FILE_READ',
  FILE_WRITE: 'FILE_WRITE',
  NET_CONNECT: 'NET_CONNECT',
  NET_LISTEN: 'NET_LISTEN',
  PROCESS_SPAWN: 'PROCESS_SPAWN',
  PROCESS_KILL: 'PROCESS_KILL',
  SYS_EXECVE: 'SYS_EXECVE',
  SYS_PTRACE: 'SYS_PTRACE',
  ENV_ACCESS: 'ENV_ACCESS',
  RAW_SOCKET: 'RAW_SOCKET'
};

const DEFAULT_DENIED_SYSCALLS = ['execve', 'ptrace', 'fork', 'clone', 'kill'];

class CapabilityViolationError extends Error {
  constructor(message, code, detail) {
    super(message);
    this.name = 'CapabilityViolationError';
    this.code = code;
    this.detail = detail;
  }
}

class CapabilityGuard {
  constructor() {
    this._capabilities = new Map();
    this._auditHooks = [];
    this._initializeDefaults();
  }

  _initializeDefaults() {
    this._capabilities.set('SAFE', new Set());
    this._capabilities.set('BALANCED', new Set([
      CAPABILITIES.FILE_READ,
      CAPABILITIES.NET_CONNECT
    ]));
    this._capabilities.set('FAST', new Set([
      CAPABILITIES.FILE_READ,
      CAPABILITIES.FILE_WRITE,
      CAPABILITIES.NET_CONNECT
    ]));
    this._capabilities.set('SMART', new Set([
      CAPABILITIES.FILE_READ,
      CAPABILITIES.FILE_WRITE,
      CAPABILITIES.NET_CONNECT
    ]));
    this._capabilities.set('PERSISTENT', new Set([
      CAPABILITIES.FILE_READ,
      CAPABILITIES.FILE_WRITE,
      CAPABILITIES.NET_CONNECT,
      CAPABILITIES.NET_LISTEN
    ]));
  }

  onViolation(callback) {
    this._auditHooks.push(callback);
  }

  _emitViolation(severity, message, detail) {
    for (const hook of this._auditHooks) {
      try { hook(severity, message, detail); } catch (e) { }
    }
  }

  grantPermission(mode, capability) {
    if (!this._capabilities.has(mode)) {
      this._capabilities.set(mode, new Set());
    }
    this._capabilities.get(mode).add(capability);
  }

  revokePermission(mode, capability) {
    if (this._capabilities.has(mode)) {
      this._capabilities.get(mode).delete(capability);
    }
  }

  hasPermission(mode, capability) {
    const perms = this._capabilities.get(mode);
    if (!perms) return false;
    return perms.has(capability);
  }

  checkPermission(mode, capability, resource) {
    const perms = this._capabilities.get(mode);
    if (!perms || !perms.has(capability)) {
      const msg = 'CRITICAL: Seccomp policy violation! Process killed.';
      this._emitViolation('CRITICAL', msg, { mode, capability, resource });
      throw new CapabilityViolationError(msg, 'ACCESS_DENIED', { mode, capability, resource });
    }
    return true;
  }

  enforceSandbox(mode, action, resource) {
    if (mode === 'SAFE' && DEFAULT_DENIED_SYSCALLS.includes(action)) {
      const msg = 'CRITICAL: Seccomp policy violation! Process killed.';
      this._emitViolation('CRITICAL', msg, { mode, action, resource });
      throw new CapabilityViolationError(msg, 'SIGSYS', { mode, action, resource });
    }
    return this.checkPermission(mode, action, resource);
  }

  getPermissions(mode) {
    const perms = this._capabilities.get(mode);
    return perms ? Array.from(perms) : [];
  }

  resetToDefaults() {
    this._capabilities.clear();
    this._initializeDefaults();
  }
}

module.exports = { CapabilityGuard, CapabilityViolationError, CAPABILITIES };

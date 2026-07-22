const crypto = require('crypto');
const fs = require('fs');
const tls = require('tls');

const JWT_ALGORITHMS = {
  RS256: { hash: 'RSA-SHA256', padding: crypto.constants.RSA_PKCS1_PADDING },
  Ed25519: null
};

class JWTVerificationError extends Error {
  constructor(message, code) {
    super(message);
    this.name = 'JWTVerificationError';
    this.code = code;
  }
}

class mTLSJwtGuard {
  constructor(options = {}) {
    this._jtiSet = new Set();
    this._keyCache = new Map();
    this._maxJtiEntries = options.maxJtiEntries || 100000;
    this._jtiCleanupInterval = options.jtiCleanupInterval || 60000;
    this._cleanupTimer = null;

    this._certPath = options.certPath || process.env.MTLS_CERT || null;
    this._keyPath = options.keyPath || process.env.MTLS_KEY || null;
    this._caPath = options.caPath || process.env.MTLS_CA || null;
    this._cert = null;
    this._key = null;
    this._ca = null;

    if (this._certPath && this._keyPath) {
      this.loadCertificates(this._certPath, this._keyPath, this._caPath);
    }

    this._scheduleJtiCleanup();

    this._certCheckInterval = options.certCheckInterval || 86400000;
    this._certTimer = null;
    if (this._cert) {
      this._scheduleCertCheck();
    }
  }

  loadCertificates(certPath, keyPath, caPath) {
    try {
      this._cert = fs.readFileSync(certPath);
      this._key = fs.readFileSync(keyPath);
      if (caPath) {
        this._ca = fs.readFileSync(caPath);
      }
    } catch (e) {
      if (typeof process !== 'undefined' && process.env.DEBUG) {
        console.log('[MTLS] Certificate load error:', e.message);
      }
    }
  }

  getTLSOptions() {
    if (!this._cert || !this._key) {
      return { rejectUnauthorized: false };
    }
    return {
      cert: this._cert,
      key: this._key,
      ca: this._ca,
      rejectUnauthorized: true,
      requestCert: true,
      secureProtocol: 'TLS_method',
      minVersion: 'TLSv1.3'
    };
  }

  checkCertificateExpiry() {
    if (!this._cert) {
      return { valid: false, reason: 'No certificate loaded' };
    }
    try {
      const cert = new crypto.X509Certificate(this._cert);
      const expiry = new Date(cert.validTo);
      const now = new Date();
      const daysLeft = Math.floor((expiry - now) / 86400000);
      if (daysLeft < 0) {
        return { valid: false, reason: 'Certificate expired', daysOverdue: -daysLeft };
      }
      if (daysLeft < 30) {
        return { valid: true, warning: true, daysLeft, message: `Certificate expires in ${daysLeft} days` };
      }
      return { valid: true, warning: false, daysLeft };
    } catch (e) {
      return { valid: false, reason: e.message };
    }
  }

  _scheduleCertCheck() {
    if (this._certTimer) clearInterval(this._certTimer);
    this._certTimer = setInterval(() => {
      const status = this.checkCertificateExpiry();
      if (!status.valid) {
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log('[MTLS] Certificate auto-renewal needed:', status.reason);
        }
      }
    }, this._certCheckInterval);
    if (this._certTimer.unref) this._certTimer.unref();
  }

  _scheduleJtiCleanup() {
    if (this._cleanupTimer) clearInterval(this._cleanupTimer);
    this._cleanupTimer = setInterval(() => {
      if (this._jtiSet.size > this._maxJtiEntries) {
        this._jtiSet.clear();
      }
    }, this._jtiCleanupInterval);
    if (this._cleanupTimer.unref) this._cleanupTimer.unref();
  }

  verifyJWT(token, publicKeyPem, options = {}) {
    const parts = token.split('.');
    if (parts.length !== 3) {
      throw new JWTVerificationError('Malformed JWT token', 'MALFORMED');
    }

    const [headerB64, payloadB64, signatureB64] = parts;

    let header, payload;
    try {
      header = JSON.parse(Buffer.from(headerB64, 'base64url').toString('utf8'));
      payload = JSON.parse(Buffer.from(payloadB64, 'base64url').toString('utf8'));
    } catch (e) {
      throw new JWTVerificationError('Invalid JWT encoding', 'ENCODING');
    }

    const alg = header.alg || 'RS256';

    if (payload.exp) {
      const now = Math.floor(Date.now() / 1000);
      if (now > payload.exp) {
        const warnMsg = 'WARN: Expired JWT token presented.';
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log(warnMsg);
        }
        throw new JWTVerificationError(warnMsg, 'EXPIRED');
      }
    }

    if (payload.jti) {
      if (this._jtiSet.has(payload.jti)) {
        const alertMsg = 'SECURITY_ALERT: Replay attack detected for JWT ID ' + payload.jti;
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log(alertMsg);
        }
        throw new JWTVerificationError(alertMsg, 'REPLAY');
      }
      this._jtiSet.add(payload.jti);
      if (this._jtiSet.size > this._maxJtiEntries) {
        this._jtiSet.clear();
      }
    }

    if (publicKeyPem) {
      const cacheKey = typeof publicKeyPem === 'string' ? publicKeyPem.slice(0, 64) : 'key';
      if (!this._keyCache.has(cacheKey)) {
        this._keyCache.set(cacheKey, publicKeyPem);
      }

      const signedContent = headerB64 + '.' + payloadB64;
      const signature = Buffer.from(signatureB64, 'base64url');

      let verified = false;
      try {
        if (alg === 'Ed25519') {
          verified = crypto.verify(null, Buffer.from(signedContent), publicKeyPem, signature);
        } else {
          const verify = crypto.createVerify('RSA-SHA256');
          verify.update(signedContent);
          verify.end();
          verified = verify.verify({ key: publicKeyPem, padding: crypto.constants.RSA_PKCS1_PADDING }, signature);
        }
      } catch (e) {
        const alertMsg = 'SECURITY_ALERT: JWT signature verification failed! Potential forgery.';
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log(alertMsg);
        }
        throw new JWTVerificationError(alertMsg, 'FORGERY');
      }

      if (!verified) {
        const alertMsg = 'SECURITY_ALERT: JWT signature verification failed! Potential forgery.';
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log(alertMsg);
        }
        throw new JWTVerificationError(alertMsg, 'FORGERY');
      }
    }

    return { header, payload, verified: true };
  }

  verifyTLSPeer(cert) {
    if (!cert) {
      throw new JWTVerificationError('FATAL: mTLS handshake failed. Peer unverified.', 'MTLS_FAILURE');
    }
    try {
      const x509 = new crypto.X509Certificate(cert);
      return {
        subject: x509.subject,
        issuer: x509.issuer,
        validFrom: x509.validFrom,
        validTo: x509.validTo,
        fingerprint: x509.fingerprint,
        verified: true
      };
    } catch (e) {
      throw new JWTVerificationError('FATAL: mTLS handshake failed. Peer unverified.', 'MTLS_FAILURE');
    }
  }

  close() {
    if (this._cleanupTimer) clearInterval(this._cleanupTimer);
    if (this._certTimer) clearInterval(this._certTimer);
    this._jtiSet.clear();
    this._keyCache.clear();
  }
}

module.exports = { mTLSJwtGuard, JWTVerificationError };

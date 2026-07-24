const EventEmitter = require('events');
const zlib = require('zlib');

const COMPACTOR_VERSION = 1;
const MAGIC_BYTES = Buffer.from([0x50, 0x4C, 0x52, 0x53]);
const MAX_CHUNK_SIZE = 64 * 1024;

class StreamCompactor extends EventEmitter {
  constructor(options = {}) {
    super();
    this._compressionLevel = options.compressionLevel || parseInt(process.env.STREAM_COMPRESSION, 10) || 6;
    this._chunkSize = options.chunkSize || parseInt(process.env.STREAM_CHUNK_SIZE, 10) || MAX_CHUNK_SIZE;
    this._totalBytesIn = 0;
    this._totalBytesOut = 0;
  }

  configure(key, value) {
    const v = parseInt(value, 10);
    if (key === 'STREAM_COMPRESSION' && v >= 1 && v <= 9) this._compressionLevel = v;
    if (key === 'STREAM_CHUNK_SIZE' && v >= 1024 && v <= 262144) this._chunkSize = v;
  }

  _encodeHeaders(headers) {
    const encoded = {};
    for (const [k, v] of Object.entries(headers)) {
      if (v === undefined || v === null) continue;
      const t = typeof v;
      if (t === 'string') encoded[k] = { t: 's', v };
      else if (t === 'number') {
        if (Number.isInteger(v)) encoded[k] = { t: 'i', v };
        else encoded[k] = { t: 'f', v };
      } else if (t === 'boolean') encoded[k] = { t: 'b', v };
      else if (Array.isArray(v)) encoded[k] = { t: 'a', v: v.map(String) };
    }
    return encoded;
  }

  _decodeHeaders(encoded) {
    const headers = {};
    for (const [k, field] of Object.entries(encoded)) {
      switch (field.t) {
        case 's': headers[k] = field.v; break;
        case 'i': headers[k] = field.v; break;
        case 'f': headers[k] = field.v; break;
        case 'b': headers[k] = field.v; break;
        case 'a': headers[k] = field.v; break;
      }
    }
    return headers;
  }

  _calculateReduction(originalSize, compressedSize) {
    if (originalSize === 0) return 0;
    return Math.round((1 - compressedSize / originalSize) * 100);
  }

  compressReapStream(headers, payload) {
    const originalPayload = typeof payload === 'string' ? Buffer.from(payload, 'utf8') : payload;
    const originalSize = originalPayload.length;

    const encodedHeaders = this._encodeHeaders(headers);
    const headerJson = JSON.stringify(encodedHeaders);
    const headerBuf = Buffer.from(headerJson, 'utf8');

    const ts = Date.now();
    const tsBuf = Buffer.alloc(6);
    tsBuf.writeUIntBE(Math.floor(ts / 1000), 0, 6);

    const compressed = zlib.deflateRawSync(originalPayload, { level: this._compressionLevel });
    const compressedSize = compressed.length;

    const headerLen = headerBuf.length;
    const payloadLen = compressed.length;

    const totalSize = 4 + 1 + 6 + 4 + 4 + compressedSize + headerLen;
    const buf = Buffer.alloc(totalSize);

    let offset = 0;
    MAGIC_BYTES.copy(buf, offset); offset += 4;
    buf.writeUInt8(COMPACTOR_VERSION, offset); offset += 1;
    tsBuf.copy(buf, offset); offset += 6;
    buf.writeUInt32BE(originalSize, offset); offset += 4;
    buf.writeUInt32BE(headerLen, offset); offset += 4;
    compressed.copy(buf, offset); offset += compressedSize;
    headerBuf.copy(buf, offset);

    this._totalBytesIn += originalSize;
    this._totalBytesOut += totalSize;

    const reduction = this._calculateReduction(originalSize, totalSize);
    this.emit('stream:compressed', { originalSize, compressedSize: totalSize, reduction });

    return buf;
  }

  decompressReapStream(buffer) {
    if (!Buffer.isBuffer(buffer)) {
      throw new Error('Expected Buffer input');
    }
    if (buffer.length < 15) {
      throw new Error('Buffer too short for valid REAP stream');
    }

    const magic = buffer.slice(0, 4);
    if (!magic.equals(MAGIC_BYTES)) {
      throw new Error('Invalid REAP stream magic bytes');
    }

    const version = buffer.readUInt8(4);
    if (version !== COMPACTOR_VERSION) {
      throw new Error('Unsupported REAP stream version: ' + version);
    }

    const tsSec = buffer.readUIntBE(5, 6);
    const timestamp = tsSec * 1000;
    const originalSize = buffer.readUInt32BE(11);
    const headerLen = buffer.readUInt32BE(15);

    let compressed, headerBuf;
    let offset = 19;
    if (buffer.length < offset + headerLen) {
      throw new Error('Buffer truncated: missing header section');
    }
    const compressedLen = buffer.length - 4 - 1 - 6 - 4 - 4 - headerLen;
    if (compressedLen <= 0) {
      throw new Error('Buffer truncated: missing payload section');
    }
    compressed = buffer.slice(offset, offset + compressedLen);
    offset += compressedLen;
    headerBuf = buffer.slice(offset, offset + headerLen);

    let payload;
    try {
      payload = zlib.inflateRawSync(compressed);
    } catch (e) {
      throw new Error('Payload decompression failed: ' + e.message);
    }

    if (payload.length !== originalSize) {
      throw new Error('Decompressed size mismatch: expected ' + originalSize + ' got ' + payload.length);
    }

    let encodedHeaders;
    try {
      encodedHeaders = JSON.parse(headerBuf.toString('utf8'));
    } catch (e) {
      throw new Error('Header parse failed: ' + e.message);
    }

    const headers = this._decodeHeaders(encodedHeaders);

    this._totalBytesIn += buffer.length;
    this._totalBytesOut += payload.length;

    this.emit('stream:decompressed', { originalSize: payload.length, compressedSize: buffer.length });

    return { headers, payload: payload.toString('utf8'), timestamp };
  }

  getStats() {
    return {
      compressionLevel: this._compressionLevel,
      chunkSize: this._chunkSize,
      totalBytesIn: this._totalBytesIn,
      totalBytesOut: this._totalBytesOut,
    };
  }
}

module.exports = { StreamCompactor };

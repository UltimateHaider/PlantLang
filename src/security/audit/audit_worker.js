const { parentPort } = require('worker_threads');

const BATCH_SIZE = 50;
let batch = [];

parentPort.on('message', (msg) => {
  if (msg.type === 'flush') {
    batch.push(msg.entry);
    if (batch.length >= BATCH_SIZE) {
      for (const e of batch) {
        if (typeof process !== 'undefined' && process.env.DEBUG) {
          console.log('[AUDIO] Flush: ' + JSON.stringify(e));
        }
      }
      batch = [];
    }
    parentPort.postMessage({ type: 'flushed', count: batch.length });
  }
  if (msg.type === 'shutdown') {
    for (const e of batch) {
      if (typeof process !== 'undefined' && process.env.DEBUG) {
        console.log('[AUDIO] Final flush: ' + JSON.stringify(e));
      }
    }
    batch = [];
    parentPort.postMessage({ type: 'shutdown_ack' });
  }
});

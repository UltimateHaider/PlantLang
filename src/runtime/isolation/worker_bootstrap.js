'use strict';

/**
 * Worker bootstrap — minimal child process entry point for WarmProcessPool.
 *
 * Listens for IPC messages from the parent:
 *   - { type: 'ping' }  → responds with { type: 'pong' }
 *   - { type: 'task', data } → executes task, responds with result/error
 */
process.on('message', (msg) => {
  if (!msg || !msg.type) return;

  switch (msg.type) {
    case 'ping':
      process.send({ type: 'pong' });
      break;

    case 'task':
      try {
        // Execute the task function (deserialized as data)
        const result = typeof msg.data === 'function'
          ? msg.data()
          : msg.data;
        process.send({ type: 'result', data: result });
      } catch (err) {
        process.send({ type: 'error', message: err.message });
      }
      break;

    default:
      // Unknown message type — ignore
      break;
  }
});

// Signal readiness
process.send({ type: 'pong' });

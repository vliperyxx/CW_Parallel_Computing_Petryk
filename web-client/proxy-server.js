const express = require('express');
const cors = require('cors');
const net = require('net');

const CPP_HOST = '127.0.0.1';
const CPP_PORT = 8080;
const HTTP_PORT = 3000;

const app = express();
app.use(cors());
app.use(express.json());

const cppSocket = new net.Socket();
let cppConnected = false;
let cppBuffer = '';
let expectWelcome = true;

let isQueued = false;

const requestQueue = [];
let currentRequest = null;
cppSocket.connect(CPP_PORT, CPP_HOST, () => {
    cppConnected = true;
    console.log(`Server: Connected to ${CPP_HOST}:${CPP_PORT}`);
    processQueue();
});

cppSocket.on('data', (data) => {
    cppBuffer += data.toString('utf8');

    let idx;
    while ((idx = cppBuffer.indexOf('\n')) !== -1) {
        let line = cppBuffer.slice(0, idx);
        cppBuffer = cppBuffer.slice(idx + 1);
        line = line.replace('\r', '');

        if (expectWelcome) {
            if (line === "SERVER_BUSY") {
                console.log('Server: No free slots, added to queue.');
                isQueued = true;
            }
            else if (line.startsWith("Welcome")) {
                console.log('Server: ' + line);
                expectWelcome = false;
                isQueued = false;
            }
            continue;
        }

        if (!currentRequest) {
            continue;
        }

        const request = currentRequest;

        if (request.stage === 'header') {
            if (line === 'SERVER_BUSY') {
                console.log('Server: Busy, request queued.');
                request.resolve({status: 'busy'});
                finishRequest();
            }
            else if (line === 'NOT_FOUND') {
                console.log('Server: No results found.');
                request.resolve({results: []});
                finishRequest();
            }
            else if (line.startsWith('ERROR')) {
                console.log('Server: ' + line);
                request.reject(new Error('Server error: ' + line));
                finishRequest();
            }
            else if (line.startsWith('OK:')) {
                const parts = line.split(':');
                const count = parseInt(parts[1], 10) || 0;

                console.log(`Server: Found ${count} results.`);

                request.expectedLines = count;
                request.mode = 'search_results';

                if (count === 0) {
                    request.resolve({results: []});
                    finishRequest();
                } else {
                    request.stage = 'body';
                }
            }
            else if (line.startsWith('SNIPPETS_FOUND:')) {
                const parts = line.split(':');
                const count = parseInt(parts[1], 10) || 0;

                console.log(`Server: Found ${count} snippets.`);

                request.mode = 'snippets';
                request.stage = 'body';
            }
            else {
                request.reject(new Error('Unknown header: ' + line));
                finishRequest();
            }
        }

        else if (request.stage === 'body') {
            if (request.mode === 'search_results') {
                request.lines.push(line);

                if (request.lines.length >= request.expectedLines) {
                    const parsedResults = request.lines.map(raw => {
                        const match = raw.match(/^\[(\d+)\] (.*?) \| matches=(\d+)/);
                        if (match) {
                            return {index: parseInt(match[1]), path: match[2], matches: parseInt(match[3])};
                        }
                        return {raw};
                    });

                    request.resolve({results: parsedResults});
                    finishRequest();
                }
            }
            else if (request.mode === 'snippets') {
                const snippets = line.split(';').filter(s => s.length > 0);
                request.resolve({snippets});
                finishRequest();
            }
        }
    }
});

cppSocket.on('error', (error) => {
    console.error('Server: Connection error:', error);
    if (currentRequest) {
        currentRequest.reject(error);
        currentRequest = null;
    }
});

cppSocket.on('close', () => {
    console.error('Server: Connection closed.');
    cppConnected = false;
    if (currentRequest) {
        currentRequest.reject(new Error('Server connection closed'));
        currentRequest = null;
    }
});

function finishRequest() {
    currentRequest = null;
    processQueue();
}

function processQueue() {
    if (!cppConnected) {
        return;
    }
    if (currentRequest || requestQueue.length === 0) {
        return;
    }

    if (isQueued) {
        const request = requestQueue.shift();
        request.resolve({status: 'busy'});
        setTimeout(processQueue, 0);
        return;
    }

    currentRequest = requestQueue.shift();
    currentRequest.stage = 'header';
    currentRequest.lines = [];
    currentRequest.mode = null;

    cppSocket.write(currentRequest.command + '\n');
}

function sendCommand(command) {
    return new Promise((resolve, reject) => {
        if (isQueued) {
            return resolve({status: 'busy'});
        }

        const request = {command, resolve, reject, stage: 'pending', expectedLines: 0, lines: [], mode: null};
        requestQueue.push(request);
        processQueue();
    });
}

app.post('/search', async (request, result) => {
    const {query} = request.body;
    if (!query || !query.trim()) {
        return result.status(400).json({error: 'Query is required'});
    }

    try {
        const response = await sendCommand(`search ${query.trim()}`);

        if (response.status === 'busy') {
            return result.json({status: 'busy'});
        }

        result.json(response);

    } catch (error) {
        console.error(error);
        result.status(500).json({error: 'Search failed: ' + error.message});
    }
});

app.post('/snippet', async (request, result) => {
    const {index} = request.body;

    if (index === undefined || index === null) {
        return result.status(400).json({error: 'Index is required'});
    }

    try {
        const response = await sendCommand(`getsnippet ${index}`);

        result.json(response);

    } catch (error) {
        console.error(error);
        result.status(500).json({error: 'Failed to get snippets: ' + error.message});
    }
});

app.listen(HTTP_PORT, () => {
    console.log(`Proxy listening on http://localhost:${HTTP_PORT}`);
});
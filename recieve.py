#!/usr/bin/env python3
from flask import Flask, request, Response

app = Flask(__name__)

@app.route('/mic', methods=['POST'])
def receive_audio():
    # Open the output file in binary-write mode
    with open('received.wav', 'wb') as f:
        # stream incoming data in chunks to avoid loading everything into memory
        chunk_size = 4096
        data_stream = request.environ.get('wsgi.input')
        while True:
            chunk = data_stream.read(chunk_size)
            if not chunk:
                break
            f.write(chunk)
    return Response("Received", status=200, mimetype='text/plain')

if __name__ == '__main__':
    # By default Flask binds to 127.0.0.1:5000; you can change host/port as needed
    app.run(host='127.0.0.1', port=8888, threaded=True)

#!/usr/bin/env python3
import sys
import requests

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} file.wav", file=sys.stderr)
        sys.exit(1)

    filename = sys.argv[1]
    url = "http://127.0.0.1:8887/upload"

    try:
        with open(filename, 'rb') as f:
            files = {'file': (filename, f, 'audio/wav')}
            response = requests.post(url, files=files)
            response.raise_for_status()
            print(response.text)
    except FileNotFoundError:
        print(f"Error: file '{filename}' not found", file=sys.stderr)
        sys.exit(1)
    except requests.RequestException as e:
        print(f"Upload failed: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()

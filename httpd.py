#!/usr/bin/python3
from flask import Flask
import json

app = Flask(__name__)

rlist = {"names": [
    "Defualt",
    "Resurected",
    "Listed",
    "Hollowed",
    "Pers",
]}

@app.route("/list", methods=["GET"])
def index():
    return json.dumps(rlist), 200, {'Content-Type': 'application/json','Access-Control-Allow-Origin': 'https://localhost:3000'}

if __name__ == "__main__":
    app.run(host="0.0.0.0",port=8000,ssl_context=('cert.pem', 'key.pem'))

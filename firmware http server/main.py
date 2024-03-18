#!/usr/bin/env python3

import os
from pathlib import Path
from flask import Flask, flash, request, redirect, send_from_directory, url_for
from werkzeug.utils import secure_filename

UPLOAD_FOLDER = '/firmware'

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = "/firmware"

@app.route('/<type>/<second>/<third>', methods=['GET'])
@app.route('/<type>/<second>', methods=['GET', 'POST'])
@app.route('/<type>', methods=['POST'])
def upload_or_download_file(type, second=None, third=None):
    if request.method == 'POST':
        # Handling file upload
        if 'file' not in request.files:
            flash('No file part')
            return redirect(request.url)
        file = request.files['file']
        if file.filename == '':
            flash('No selected file')
            return redirect(request.url)
        filename = secure_filename(file.filename)
        directory = os.path.join(app.config['UPLOAD_FOLDER'], type)
        if second:
            directory = os.path.join(directory, second)
        Path(directory).mkdir(parents=True, exist_ok=True)
        file.save(os.path.join(directory, filename))
        return "OK"
    elif request.method == 'GET':
        # Handling file download
        if third:
            return send_from_directory(os.path.join(app.config["UPLOAD_FOLDER"], type, second), third)
        else:
            return send_from_directory(os.path.join(app.config["UPLOAD_FOLDER"], type), second)
    else:
        return "Bad Request", 500

@app.route('/', methods=['GET'])
def index():
    return "Nothing here, sorry."

if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=8080)

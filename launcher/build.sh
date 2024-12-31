source ./venv/bin/activate

python -m nuitka --standalone --onefile --enable-plugin=no-qt --enable-plugin=tk-inter --linux-onefile-icon=launcher.png --include-data-dir=assets=. --output-filename=launcher-lin launcher.py
mv launcher-lin ..

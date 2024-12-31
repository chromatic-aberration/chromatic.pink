source .\linvenv\Scripts\activate

python -m nuitka --standalone --onefile --enable-plugin=no-qt --enable-plugin=tk-inter --linux-onefile-icon=launcher.png --include-data-dir=assets=. launcher.py
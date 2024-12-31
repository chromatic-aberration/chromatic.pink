.\venv\Scripts\activate

python -m nuitka --standalone --onefile --enable-plugin=no-qt --enable-plugin=tk-inter --windows-icon-from-ico=launcher.ico --include-data-dir=assets=. launcher.py
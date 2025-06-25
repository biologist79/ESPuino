import os
import subprocess

def install_dependencies():
    try:
        # Überprüfen, ob das Modul bereits installiert ist
        import rich
        print("Module 'rich' is already installed.")
    except ImportError:
        print("Module 'rich' is not installed. Let's try to install it...")
        # Installation des Moduls mit pip
        subprocess.check_call([os.sys.executable, '-m', 'pip', 'install', 'rich'])

if __name__ == "__main__":
    install_dependencies()

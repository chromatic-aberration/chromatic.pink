import os
import sys
import argparse
import json
import threading
import requests
import toml
import tkinter as tk
import customtkinter as ctk
import webbrowser
import subprocess
from PIL import Image, ImageTk
import logging

from mcstatus import JavaServer

# Enhanced logging configuration
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

# File handler
file_handler = logging.FileHandler('launcher.log')
file_handler.setLevel(logging.DEBUG)
file_formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(message)s')
file_handler.setFormatter(file_formatter)

# Console handler
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setLevel(logging.DEBUG)
console_formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(message)s')
console_handler.setFormatter(console_formatter)

# Add handlers to the logger
logger.addHandler(file_handler)
logger.addHandler(console_handler)

# Constants
CONFIG_DIR = os.path.join(os.path.dirname(sys.argv[0]), 'config')
CONFIG_FILE = os.path.join(CONFIG_DIR, 'tempad-client.jsonc')
INIT_FILE = os.path.join(os.path.dirname(sys.argv[0]), '.tempad_initialized')
LOGO_PATH = os.path.join(CONFIG_DIR, 'simplemenu', 'logo', 'edition.png')
PACK_TOML_URL = "http://chromatic.pink/pack.toml"
MINECRAFT_SERVER_IP = "188.165.47.57"
MINECRAFT_SERVER_PORT = 26955
JAVA_DEFAULT_ARGS = ["-jar", "packwiz-installer-bootstrap.jar", "http://chromatic.pink/pack.toml"]
CHANGELOG_URL = "https://discord.com/channels/1315863508320804905/1318760261229744190"
MODLIST_URL = "https://github.com/chromatic-aberration/chromatic.pink/blob/main/README.md"
LAUNCHER_VER = "0.1"

# Preset colors (hexadecimal representation)
PRESET_COLORS = [
    0xFF16FB,  # Default pink (16722355)
    0x0631FF,  # Blue (25343)
    0xFF0000,  # Red (16711680)
    0x00FF00,  # Green (65280)
    0xFFFF00,  # Yellow (16776320)
    0xFF00FF,  # Magenta (16711935)
    0x00FFFF,  # Cyan (65535)
    0x808080   # Gray (8421504)
]

# Initialize configuration
def initialize_config():
    logging.info("Initializing configuration.")
    if not os.path.exists(INIT_FILE):
        os.makedirs(os.path.dirname(CONFIG_FILE), exist_ok=True)
        default_config = {
            "color": 16722355,
            "renderBlur": True
        }
        with open(CONFIG_FILE, 'w') as f:
            json.dump(default_config, f, indent=4)
        with open(INIT_FILE, 'w') as f:
            f.write('initialized')
        logging.info("Initialized configuration.")

def read_config():
    logging.info("Reading configuration.")
    try:
        with open(CONFIG_FILE, 'r') as f:
            # Simple JSONC: remove lines starting with //
            lines = f.readlines()
            json_str = ''.join([line for line in lines if not line.strip().startswith('//')])
            config = json.loads(json_str)
            logging.info("Configuration loaded successfully.")
            return config
    except Exception as e:
        logging.error(f"Error reading config: {e}")
        # Return default config in case of error
        return {
            "color": 16722355,
            "renderBlur": True
        }

def write_config(config):
    logging.info("Writing configuration.")
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=4)
        logging.info("Configuration updated successfully.")
    except Exception as e:
        logging.error(f"Error writing config: {e}")

# Fetch modpack version from pack.toml
def fetch_modpack_version():
    logging.info("Fetching modpack version.")
    try:
        response = requests.get(PACK_TOML_URL)
        response.raise_for_status()
        pack_data = toml.loads(response.text)
        version = pack_data.get('version', 'Unknown')
        logging.info(f"Fetched modpack version: {version}")
        return version
    except Exception as e:
        logging.error(f"Error fetching modpack version: {e}")
        return "Unknown"

# Check Minecraft server status
def check_server_status(label_status, label_players):
    logging.info("Checking Minecraft server status.")
    try:
        server = JavaServer.lookup(f"{MINECRAFT_SERVER_IP}:{MINECRAFT_SERVER_PORT}")
        status = server.status()
        # Update UI with server status
        label_status.configure(text="Server Online", text_color="#8aff9d")
        if status.players.sample:
            players = ', '.join([player.name for player in status.players.sample])
        else:
            players = "Pusto :("
        label_players.configure(text=f"Lista graczy: {players}")
        logging.info("Server is online.")
    except Exception as e:
        logging.error(f"Server offline or error: {e}")
        label_status.configure(text="Server Offline", text_color="#f06e43")
        label_players.configure(text="")

# Start Minecraft with the provided Java path
def start_minecraft(java_path):
    logging.info("Starting Minecraft.")
    try:
        args = [java_path] + JAVA_DEFAULT_ARGS
        subprocess.Popen(args, cwd=os.path.dirname(sys.argv[0]))
        logging.info("Minecraft started successfully.")
        os._exit(0)  # Exit with code 0
    except Exception as e:
        logging.error(f"Error starting Minecraft: {e}")
        os._exit(1)  # Exit with error code

# Open URL in default browser
def open_url(url):
    logging.info(f"Opening URL: {url}")
    try:
        webbrowser.open(url)
        logging.info(f"Opened URL: {url}")
    except Exception as e:
        logging.error(f"Error opening URL {url}: {e}")

    # Custom Color Picker Class
class ColorPicker(ctk.CTkToplevel):
    def __init__(self, master, current_color, preset_colors, callback):
        super().__init__(master)
        self.title("Wybierz kolor tempada:")
        
        # Hide the window top bar for a sleek look
        
        self.center_window(master, 480, 480)
        # self.update()
        # self.withdraw()
        # self.overrideredirect(True) 
        self.resizable(False, False)
        
        # self.deiconify()
        
        self.callback = callback
        self.preset_colors = preset_colors
        self.current_color = current_color  # Store the initial color
        self.selected_color = current_color  # Initialize selected_color with current_color
        self.configure(fg_color="#2B2B2B")  # Dark background

        # Make the window modal
        self.transient(master)
        self.grab_set()

        # # Instruction Label
        # instruction = ctk.CTkLabel(self, text="Select a color:", font=("Helvetica", 14), text_color="white")
        # instruction.pack(pady=10)

        # Frame for preset color buttons and color picker grid side by side
        selection_frame = ctk.CTkFrame(self, fg_color="#2B2B2B")
        selection_frame.pack(pady=0, padx=0, fill="both", expand=True)

        # Preset color buttons
        preset_frame = ctk.CTkFrame(selection_frame, fg_color="#2B2B2B")
        preset_frame.pack(side="left", padx=8, pady=8, fill="both", expand=True)

        cols = 2  # Number of columns
        for index, color in enumerate(self.preset_colors):
            hex_color = f"#{color:06x}"
            btn = ctk.CTkButton(
                preset_frame,
                fg_color=hex_color,
                width=56,
                height=56,
                command=lambda c=color: self.select_color(c),
                text="",  # Remove text from buttons
                hover_color=self.darken_color(hex_color, 0.8)  # Optional: Darken on hover
            )
            btn.grid(row=index//cols, column=index%cols, padx=8, pady=8)

        # Color picker grid
        color_picker_frame = ctk.CTkFrame(selection_frame, fg_color="#2B2B2B")
        color_picker_frame.pack(side="left", padx=10, pady=10, fill="both", expand=True)

        # Canvas for hue-saturation selection
        self.canvas_size = 300
        self.color_canvas = ctk.CTkCanvas(color_picker_frame, width=self.canvas_size, height=self.canvas_size, highlightthickness=0, bd=0)
        self.color_canvas.pack()

        # Generate and display the hue-saturation gradient
        self.generate_gradient()

        # Bind mouse click event to the canvas
        self.color_canvas.bind("<Button-1>", self.get_color_from_canvas)

        # Currently selected color label
        self.selected_color_label = ctk.CTkLabel(self, text="Wybrany kolor:", font=("Verdana", 16), text_color="white")
        self.selected_color_label.pack(pady=(0, 0))

        self.selected_color_preview = ctk.CTkFrame(self, width=400, height=20, corner_radius=10, fg_color=self.hex_color(self.selected_color))
        self.selected_color_preview.pack(pady=(0, 10), padx=20)

        # Frame for Confirm and Close buttons
        button_frame = ctk.CTkFrame(self, fg_color="#2B2B2B")
        button_frame.pack(pady=10, padx=20, fill="x", expand=True)

        # Confirm Button
        confirm_btn = ctk.CTkButton(button_frame, text="Zatwierdź", command=self.confirm_selection)
        confirm_btn.pack(side="left", fill="x", expand=True, padx=(0, 10))

        # Close Button
        close_btn = ctk.CTkButton(button_frame, text="Anuluj", command=self.close_without_saving)
        close_btn.pack(side="right", fill="x", expand=True, padx=(10, 0))

    def hex_color(self, decimal_color):
        hex_color = f"#{decimal_color:06x}"
        return hex_color

    def center_window(self, master, width, height):
        """
        Centers the window over the master window.
        """
        master.update_idletasks()  # Ensure geometry information is updated
        master_x = master.winfo_rootx()
        master_y = master.winfo_rooty()
        master_width = master.winfo_width()
        master_height = master.winfo_height()

        x = master_x + (master_width // 2) - (width // 2)
        y = master_y + (master_height // 2) - (height // 2)
        self.geometry(f"{width}x{height}+{x}+{y}")


    def darken_color(self, hex_color, factor=0.8):
        """
        Darkens the given hex color by the specified factor.
        """
        rgb = self.hex_to_rgb(hex_color)
        darkened = tuple(max(int(c * factor), 0) for c in rgb)
        return self.rgb_to_hex(darkened)

    def hex_to_rgb(self, hex_color):
        """
        Converts hex color to RGB tuple.
        """
        hex_color = hex_color.lstrip('#')
        return tuple(int(hex_color[i:i+2], 16) for i in (0, 2 ,4))

    def rgb_to_hex(self, rgb):
        """
        Converts RGB tuple to hex color.
        """
        return '#%02x%02x%02x' % rgb

    def select_color(self, color):
        """
        Updates the selected color without closing the window.
        """
        self.selected_color = color
        self.selected_color_preview.configure(fg_color=self.hex_color(color))
        logging.info(f"Color selected: {hex(color)}")

    def generate_gradient(self):
        """
        Generates a hue-saturation gradient image and displays it on the canvas.
        """
        try:
            gradient_image = Image.new("RGB", (self.canvas_size, self.canvas_size))
            for i in range(self.canvas_size):
                for j in range(self.canvas_size):
                    hue = i / self.canvas_size  # 0 to 1
                    saturation = 1 - (j / self.canvas_size)  # 1 to 0
                    r, g, b = self.hsv_to_rgb(hue, saturation, 1)
                    gradient_image.putpixel((i, j), (int(r * 255), int(g * 255), int(b * 255)))
            self.gradient_photo = ImageTk.PhotoImage(gradient_image)
            self.color_canvas.create_image(0, 0, anchor="nw", image=self.gradient_photo)
            logging.info("Hue-Saturation gradient generated and displayed.")
        except Exception as e:
            logging.error(f"Error generating gradient: {e}")

    def hsv_to_rgb(self, h, s, v):
        """
        Converts HSV to RGB.
        """
        import colorsys
        return colorsys.hsv_to_rgb(h, s, v)

    def get_color_from_canvas(self, event):
        """
        Gets the color from the canvas based on mouse click.
        """
        try:
            x, y = event.x, event.y
            if 0 <= x < self.canvas_size and 0 <= y < self.canvas_size:
                # Calculate hue and saturation based on position
                hue = x / self.canvas_size
                saturation = 1 - (y / self.canvas_size)

                # Convert HSV to RGB
                r, g, b = self.hsv_to_rgb(hue, saturation, 1)

                # Convert to decimal color
                decimal_color = (int(r * 255) << 16) + (int(g * 255) << 8) + int(b * 255)

                # Update the selected color
                self.select_color(decimal_color)
        except Exception as e:
            logging.error(f"Error selecting color from canvas: {e}")

    def confirm_selection(self):
        """
        Confirms the selected color and closes the window.
        """
        self.callback(self.selected_color)
        logging.info(f"Color confirmed: {hex(self.selected_color)}")
        self.destroy()

    def close_without_saving(self):
        """
        Closes the window without saving the selected color.
        """
        logging.info("Color selection canceled.")
        self.destroy()

# Main Launcher Class
class Launcher(ctk.CTk):
    def __init__(self, java_path, config):
        super().__init__()
        logging.info("Initializing Launcher.")
        # title_bar = ctk.CTkFrame(self, fg_color="#1F1F1F", height=30)
        # title_bar.pack(fill="x")
        # title_label = ctk.CTkLabel(title_bar, text="chromatic.pink", anchor="w", text_color="white", font=("Helvetica", 12))
        # title_label.pack(side="left", padx=10, pady=5)
        # close_button = ctk.CTkButton(title_bar, text="✕", width=20, height=20, fg_color="#E81123", hover_color="#F1707A", command=self.on_exit)
        # close_button.pack(side="right", padx=10, pady=5)
        # title_bar.bind("<Button-1>", self.click_window)
        # title_bar.bind("<B1-Motion>", self.drag_window)
        # title_label.bind("<Button-1>", self.click_window)
        # title_label.bind("<B1-Motion>", self.drag_window)
        # close_button = ctk.CTkButton(title_bar, text="✕", width=20, height=20, fg_color="#E81123", hover_color="#F1707A", command=self.on_exit)
        # close_button.pack(side="right", padx=10, pady=5)
        
        self.java_path = java_path
        self.config = config
        
        try:
            self.title("chromatic.pink")
            # self.update_idletasks()
            self.center_window(440, 460)
            # self.update()
            
            # self.withdraw()
            # self.overrideredirect(True) 
            logging.info("Applying overrideredirect(True)")
            
            self.resizable(False, False)
            ctk.set_appearance_mode("dark")
            ctk.set_default_color_theme("pink.json")

            # Implement Custom Title Bar
            logging.info("Creating custom title bar")
            # self.create_title_bar()
            
            # self.deiconify()

            # Main frame
            logging.info("Creating main frame with CTk")
            # Create a CTk Frame inside the Tkinter window
            self.main_frame = ctk.CTkFrame(self)
            self.main_frame.pack(padx=8, pady=0, fill="both", expand=True)

            # Logo
            if os.path.exists(LOGO_PATH):
                try:
                    logo_image = Image.open(LOGO_PATH)
                    self.logo = ctk.CTkImage(light_image=logo_image, dark_image=logo_image, size=(411, 47))# logo_image.size)
                    self.logo_label = ctk.CTkLabel(self.main_frame, image=self.logo, text="")  # Ensure no text
                    self.logo_label.pack(pady=(16, 16))
                    logging.info("Logo loaded successfully.")
                except Exception as e:
                    logging.error(f"Error loading logo: {e}")
                    self.logo = None
            else:
                logging.error(f"Logo not found at path: {LOGO_PATH}")
                self.logo = None

            # Server Status
            self.server_status_label = ctk.CTkLabel(self.main_frame, text="Status servera: Sprawdzanie...", font=("Verdana", 22))
            self.server_status_label.pack(pady=(0,8))

            self.players_label = ctk.CTkLabel(self.main_frame, text="", font=("Verdana", 12))
            self.players_label.pack(pady=(0, 8))

            # Buttons
            self.start_button = ctk.CTkButton(self.main_frame, text="Start MC", command=self.on_start, fg_color="#5a8750", font=("Verdana", 18), height=40)
            self.start_button.pack(padx=48, pady=8, fill="x")

            self.color_frame = ctk.CTkFrame(self.main_frame, fg_color="#2B2B2B")
            self.color_frame.pack(pady=8, fill="x")

            self.color_button = ctk.CTkButton(self.color_frame, text="Zmiana koloru tempada", command=self.on_change_color, font=("Verdana", 18), height=40)
            self.color_button.pack(side="left", padx=(48, 8), fill="x", expand=True)

            self.color_preview = ctk.CTkFrame(
                self.color_frame, 
                width=32, 
                height=32, 
                corner_radius=8, 
                fg_color=self.hex_color(self.config['color'])
            )
            self.color_preview.pack(side="left", padx=(8, 56))

            self.urls_frame = ctk.CTkFrame(self.main_frame, fg_color="#2B2B2B")
            self.urls_frame.pack(pady=8, fill="x")

            self.changelog_button = ctk.CTkButton(
                self.urls_frame, 
                text="Changelog 🔗", 
                command=lambda: open_url(CHANGELOG_URL),
                font=("Verdana", 18),
                height=40
            )
            self.changelog_button.pack(padx=(48, 8), side="left", pady=0, fill="x", expand=True)
            
            self.modlist_button = ctk.CTkButton(
                self.urls_frame, 
                text="Lista modów 🔗", 
                command=lambda: open_url(MODLIST_URL),
                font=("Verdana", 18),
                height=40
            )
            self.modlist_button.pack(padx=(0, 48), side="right", pady=0, fill="x", expand=True)


            self.exit_button = ctk.CTkButton(self.main_frame, text="Wyjście", command=self.on_exit, font=("Verdana", 18), height=40)
            self.exit_button.pack(pady=8, padx=48, fill="x")
            
            # Modpack Version
            self.version_label = ctk.CTkLabel(self.main_frame, text="Wersja modpacka = Sprawdzanie...", font=("Verdana", 14), text_color="#ffc0cb")
            self.version_label.pack(pady=(14,2))
            
            self.author_label = ctk.CTkLabel(self.main_frame, text=f"wersja launchera: {LAUNCHER_VER} by chromaetheral", font=("Verdana", 12), text_color="#5E5657")
            self.author_label.pack(pady=(0,8))
            

            # Fetch modpack version
            threading.Thread(target=self.update_version, daemon=True).start()

            # Check server status asynchronously
            threading.Thread(
                target=check_server_status, 
                args=(self.server_status_label, self.players_label), 
                daemon=True
            ).start()
            
        # Remove default window decorations
            
            
            logging.info("Launcher window displayed successfully.")
            self.update()
            
        except Exception as e:
            logging.error(f"Error during Launcher initialization: {e}")
        

    def create_title_bar(self):
        """
        Creates a custom title bar with a title label and a close button.
        """
        try:
            title_bar = ctk.CTkFrame(self, fg_color="#1F1F1F", height=30)
            title_bar.pack(fill="x")

            title_label = ctk.CTkLabel(title_bar, text="chromatic.pink", anchor="w", text_color="white", font=("Verdana", 16))
            title_label.pack(side="left", padx=10, pady=5)

            close_button = ctk.CTkButton(
                title_bar, 
                text="✕", 
                width=20, 
                height=20, 
                fg_color="#87506b", 
                hover_color="#F35C7D", 
                command=self.on_exit,
                text_color="white"
            )
            close_button.pack(side="right", padx=10, pady=5)

            # Bind window dragging to the title bar and title label
            title_bar.bind("<Button-1>", self.click_window)
            title_bar.bind("<B1-Motion>", self.drag_window)
            title_label.bind("<Button-1>", self.click_window)
            title_label.bind("<B1-Motion>", self.drag_window)

            logging.info("Custom title bar created successfully.")
        except Exception as e:
            logging.error(f"Error creating custom title bar: {e}")
    
    def click_window(self, event):
        """
        Records the position where the user clicks the window to initiate dragging.
        """
        self.offset_x = event.x
        self.offset_y = event.y

    def drag_window(self, event):
        """
        Moves the window based on the mouse movement while dragging.
        """
        x = event.x_root - self.offset_x
        y = event.y_root - self.offset_y
        self.geometry(f"+{x}+{y}")

    def center_window(self, width, height):
        """
        Centers the window on the screen based on the given width and height.
        """
        self.update_idletasks()  # Update "requested size" from geometry manager

        # Get screen width and height
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()

        # Calculate position x and y coordinates
        x = (screen_width // 2) - (width // 2)
        y = (screen_height // 2) - (height // 2)

        # Set the geometry of the window
        self.geometry(f"{width}x{height}+{x}+{y}")
        self.lift()  # Bring window to the front
        self.focus_force()  # Force focus on the window
        logging.info("Launcher window displayed and focused successfully.")


    def hex_color(self, decimal_color):
        hex_color = f"#{decimal_color:06x}"
        return hex_color

    def update_version(self):
        version = fetch_modpack_version()
        self.version_label.configure(text=f"Wersja modpacka = {version}")
        logging.info(f"Updated modpack version to {version}")

    def on_start(self):
        start_minecraft(self.java_path)

    def on_change_color(self):
        # Open custom color picker
        color_picker = ColorPicker(
            master=self, 
            current_color=self.config['color'], 
            preset_colors=PRESET_COLORS, 
            callback=self.update_color
        )
        logging.info("Opened color picker window.")

    def update_color(self, new_color):
        self.config['color'] = new_color
        write_config(self.config)
        self.color_preview.configure(fg_color=self.hex_color(new_color))
        logging.info(f"Updated Tempad color to {self.hex_color(new_color)}")

    def on_exit(self):
        self.destroy()
        logging.info("Launcher exited.")
        os._exit(1)  # Exit with error code

# Main function
def main():
    logging.info("Starting launcher.")
    parser = argparse.ArgumentParser(description="Tempad Launcher")
    parser.add_argument('java_path', type=str, help='Path to Java executable')
    args = parser.parse_args()

    # Initialize config
    initialize_config()
    config = read_config()

    # Initialize and run GUI
    app = Launcher(args.java_path, config)
    app.mainloop()

if __name__ == "__main__":
    main()

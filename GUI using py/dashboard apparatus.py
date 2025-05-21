import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
from datetime import datetime
import threading
import queue
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import re
from collections import deque
from matplotlib.dates import DateFormatter, date2num, num2date
import matplotlib.dates as mdates

class IndustrialMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("Industrial Process Monitor")
        self.root.attributes('-fullscreen', True)
        self.root.configure(bg='#1e1e1e')
        
        # Configure dark theme
        self.style = ttk.Style()
        self.style.theme_use('alt')
        self.style.configure('.', background='#1e1e1e', foreground='white')
        self.style.configure('TFrame', background='#1e1e1e')
        self.style.configure('TLabel', background='#1e1e1e', foreground='white')
        self.style.configure('TButton', background='#333', foreground='white')
        self.style.configure('TCombobox', fieldbackground='#333', foreground='white')
        self.style.map('TButton', background=[('active', '#444')])
        
        # Serial connection
        self.serial_connection = None
        self.serial_thread = None
        self.serial_queue = queue.Queue()
        self.running = False
        
        # Data storage (100 points for autoscroll)
        self.max_data_points = 100
        self.time_data = deque(maxlen=self.max_data_points)
        self.data = {
            'flow': deque(maxlen=self.max_data_points),
            'pressure': deque(maxlen=self.max_data_points),
            'temperature_1': deque(maxlen=self.max_data_points),
            'temperature_2': deque(maxlen=self.max_data_points),
            'temperature_3': deque(maxlen=self.max_data_points),
            'temperature_4': deque(maxlen=self.max_data_points),
            'temperature_5': deque(maxlen=self.max_data_points)
        }
        
        # Current values
        self.current_values = {
            'flow': tk.StringVar(value="--"),
            'pressure': tk.StringVar(value="--"),
            'temperature_1': tk.StringVar(value="--"),
            'temperature_2': tk.StringVar(value="--"),
            'temperature_3': tk.StringVar(value="--"),
            'temperature_4': tk.StringVar(value="--"),
            'temperature_5': tk.StringVar(value="--")
        }
        
        # Graph colors
        self.graph_colors = {
            'flow': '#7fbfff',    # Blue
            'pressure': '#7fff7f', # Green
            'temperature_1': '#ff7f7f', # Red
            'temperature_2': '#ffbf7f', # Orange
            'temperature_3': '#ffdf7f', # Yellow
            'temperature_4': '#bf7fff', # Purple
            'temperature_5': '#7fffff'  # Cyan
        }
        
        # Create GUI
        self.create_widgets()
        
        # Configure matplotlib
        plt.style.use('dark_background')
        
        # Start handlers
        self.root.after(100, self.process_serial_queue)
        self.root.after(100, self.update_graphs)
        self.root.bind('<Escape>', self.toggle_fullscreen)
    
    def toggle_fullscreen(self, event=None):
        self.root.attributes('-fullscreen', not self.root.attributes('-fullscreen'))
    
    def create_widgets(self):
        # Main container with improved padding
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Top control panel
        control_frame = ttk.Frame(main_frame)
        control_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Control widgets
        ttk.Label(control_frame, text="COM Port:").pack(side=tk.LEFT, padx=5)
        self.com_port_combo = ttk.Combobox(control_frame, state="readonly", width=20)
        self.com_port_combo.pack(side=tk.LEFT, padx=5)
        self.refresh_ports()
        
        self.connect_button = ttk.Button(control_frame, text="Connect", command=self.toggle_connection)
        self.connect_button.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(control_frame, text="Refresh Ports", command=self.refresh_ports).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Clear Data", command=self.clear_data).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Exit Fullscreen (ESC)", command=self.toggle_fullscreen).pack(side=tk.RIGHT, padx=5)
        
        # Current values panel
        values_frame = ttk.LabelFrame(main_frame, text="Current Values", padding=10)
        values_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Create value displays in a grid
        for i, param in enumerate(['flow', 'pressure', 'temperature_1', 'temperature_2', 
                                 'temperature_3', 'temperature_4', 'temperature_5']):
            frame = ttk.Frame(values_frame)
            frame.grid(row=0, column=i, padx=5, pady=2, sticky='w')
            
            name = 'Flow' if param == 'flow' else 'Pressure' if param == 'pressure' else f'Temp {param.split("_")[1]}'
            unit = 'L/min' if param == 'flow' else 'bar' if param == 'pressure' else '°C'
            
            ttk.Label(frame, text=f"{name}:").pack(side=tk.LEFT)
            ttk.Label(frame, textvariable=self.current_values[param], 
                    font=('Arial', 10, 'bold'), width=6, 
                    foreground=self.graph_colors[param]).pack(side=tk.LEFT)
            ttk.Label(frame, text=unit, font=('Arial', 9)).pack(side=tk.LEFT)
        
        # Raw data display (smaller)
        data_frame = ttk.LabelFrame(main_frame, text="Raw Data", padding=10)
        data_frame.pack(fill=tk.X, pady=(0, 5))
        
        self.data_display = tk.Text(
            data_frame, 
            wrap=tk.WORD, 
            height=3,
            font=('Consolas', 9),
            bg='#252526',
            fg='#d4d4d4',
            insertbackground='white'
        )
        self.data_display.pack(fill=tk.BOTH, expand=True)
        
        # Graph area - 7 individual graphs in 3 rows with improved spacing
        graph_frame = ttk.Frame(main_frame)
        graph_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 15))  # Added bottom padding
        
        # Create 7 graphs in 3 rows (3-2-2 layout)
        self.figures = []
        self.axes = []
        self.canvases = []
        
        # Row 1 - 3 graphs (Flow, Pressure, Temp1)
        row1_frame = ttk.Frame(graph_frame)
        row1_frame.pack(fill=tk.BOTH, expand=True)
        for param in ['flow', 'pressure', 'temperature_1']:
            frame = ttk.Frame(row1_frame)
            frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2, pady=2)
            
            fig = Figure(figsize=(6, 2.5), dpi=80, facecolor='#1e1e1e')
            ax = fig.add_subplot(111)
            self.configure_axes(ax, param)
            
            canvas = FigureCanvasTkAgg(fig, master=frame)
            canvas.draw()
            widget = canvas.get_tk_widget()
            widget.pack(fill=tk.BOTH, expand=True)
            
            self.figures.append(fig)
            self.axes.append(ax)
            self.canvases.append(canvas)
        
        # Row 2 - 2 graphs (Temp2, Temp3)
        row2_frame = ttk.Frame(graph_frame)
        row2_frame.pack(fill=tk.BOTH, expand=True)
        for param in ['temperature_2', 'temperature_3']:
            frame = ttk.Frame(row2_frame)
            frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2, pady=2)
            
            fig = Figure(figsize=(6, 2.5), dpi=80, facecolor='#1e1e1e')
            ax = fig.add_subplot(111)
            self.configure_axes(ax, param)
            
            canvas = FigureCanvasTkAgg(fig, master=frame)
            canvas.draw()
            widget = canvas.get_tk_widget()
            widget.pack(fill=tk.BOTH, expand=True)
            
            self.figures.append(fig)
            self.axes.append(ax)
            self.canvases.append(canvas)
        
        # Row 3 - 2 graphs (Temp4, Temp5)
        row3_frame = ttk.Frame(graph_frame)
        row3_frame.pack(fill=tk.BOTH, expand=True)
        for param in ['temperature_4', 'temperature_5']:
            frame = ttk.Frame(row3_frame)
            frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2, pady=2)
            
            fig = Figure(figsize=(6, 2.5), dpi=80, facecolor='#1e1e1e')
            ax = fig.add_subplot(111)
            self.configure_axes(ax, param)
            
            canvas = FigureCanvasTkAgg(fig, master=frame)
            canvas.draw()
            widget = canvas.get_tk_widget()
            widget.pack(fill=tk.BOTH, expand=True)
            
            self.figures.append(fig)
            self.axes.append(ax)
            self.canvases.append(canvas)
    
    def configure_axes(self, ax, param):
        """Configure axes for a specific parameter"""
        ax.set_facecolor('#1e1e1e')
        ax.grid(True, color='#444', linestyle='--', alpha=0.7)
        ax.tick_params(colors='white', labelsize=8)
        ax.xaxis.label.set_color('white')
        ax.yaxis.label.set_color('white')
        ax.title.set_color('white')
        
        name = 'Flow' if param == 'flow' else 'Pressure' if param == 'pressure' else f'Temperature {param.split("_")[1]}'
        unit = 'L/min' if param == 'flow' else 'bar' if param == 'pressure' else '°C'
        
        ax.set_title(f'{name} ({unit})', pad=5, fontsize=10)
        ax.set_xlabel('Time (HH:MM:SS)', fontsize=8)
        
        # Configure date formatting
        time_format = DateFormatter('%H:%M:%S')
        ax.xaxis.set_major_formatter(time_format)
        
        # Adjust layout to prevent label cutoff
        fig = ax.get_figure()
        fig.tight_layout()
        fig.subplots_adjust(bottom=0.25)  # Increased bottom margin
        
        ax.spines['bottom'].set_color('#666')
        ax.spines['top'].set_color('#666') 
        ax.spines['right'].set_color('#666')
        ax.spines['left'].set_color('#666')
    
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.com_port_combo['values'] = ports
        if ports:
            self.com_port_combo.current(0)
    
    def toggle_connection(self):
        if self.serial_connection and self.serial_connection.is_open:
            self.disconnect_serial()
            self.connect_button.config(text="Connect")
        else:
            if self.connect_serial():
                self.connect_button.config(text="Disconnect")
    
    def connect_serial(self):
        port = self.com_port_combo.get()
        if not port:
            self.display_message("No COM port selected!")
            return False
        
        try:
            self.serial_connection = serial.Serial(
                port=port,
                baudrate=9600,
                timeout=1
            )
            self.running = True
            self.serial_thread = threading.Thread(target=self.read_serial_data, daemon=True)
            self.serial_thread.start()
            self.display_message(f"Connected to {port} at 9600 baud")
            return True
        except Exception as e:
            self.display_message(f"Error connecting to {port}: {str(e)}")
            return False
    
    def disconnect_serial(self):
        self.running = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
            self.display_message("Disconnected from serial port")
    
    def read_serial_data(self):
        while self.running and self.serial_connection and self.serial_connection.is_open:
            try:
                if self.serial_connection.in_waiting:
                    line = self.serial_connection.readline().decode('utf-8').strip()
                    if line:
                        timestamp = datetime.now()
                        self.serial_queue.put((timestamp, line))
            except Exception as e:
                self.serial_queue.put(("ERROR", f"Serial read error: {str(e)}"))
                self.running = False
    
    def process_serial_queue(self):
        while not self.serial_queue.empty():
            timestamp, message = self.serial_queue.get()
            if timestamp == "ERROR":
                self.display_message(message)
            else:
                self.display_message(f"[{timestamp.strftime('%H:%M:%S.%f')[:-3]}] {message}")
                self.parse_and_store_data(timestamp, message)
        self.root.after(100, self.process_serial_queue)
    
    def parse_and_store_data(self, timestamp, data_str):
        try:
            pattern = r'(flow|pressure|temperature_\d):([\d.]+)'
            matches = re.findall(pattern, data_str)
            
            if not matches:
                self.display_message("Warning: Data format not recognized")
                return
            
            self.time_data.append(timestamp)
            
            for param, value in matches:
                if param in self.data:
                    try:
                        float_value = float(value)
                        self.data[param].append(float_value)
                        self.current_values[param].set(f"{float_value:.1f}")
                    except ValueError:
                        self.display_message(f"Warning: Invalid value for {param}: {value}")
            
        except Exception as e:
            self.display_message(f"Error parsing data: {str(e)}")
    
    def update_graphs(self):
        if len(self.time_data) == 0:
            self.root.after(200, self.update_graphs)
            return
        
        # Convert timestamps to matplotlib date format
        time_dates = [date2num(t) for t in self.time_data]
        
        # Update each graph individually
        params = ['flow', 'pressure', 'temperature_1', 'temperature_2', 
                 'temperature_3', 'temperature_4', 'temperature_5']
        
        for i, (ax, param) in enumerate(zip(self.axes, params)):
            ax.clear()
            self.configure_axes(ax, param)
            
            if len(self.data[param]) > 0:
                # Plot with timestamps
                ax.plot_date(time_dates, self.data[param], 
                           color=self.graph_colors[param],
                           linewidth=1.5,
                           markersize=0)
                
                # Auto-scale y-axis with 10% padding
                y_min = min(self.data[param]) * 0.9 if min(self.data[param]) > 0 else min(self.data[param]) * 1.1
                y_max = max(self.data[param]) * 1.1 if max(self.data[param]) > 0 else max(self.data[param]) * 0.9
                ax.set_ylim(y_min, y_max)
                
                # Auto-scroll x-axis to show most recent 100 points
                if len(time_dates) > 1:
                    ax.set_xlim(time_dates[0], time_dates[-1])
            
            # Redraw canvas
            self.canvases[i].draw()
        
        self.root.after(200, self.update_graphs)
    
    def display_message(self, message):
        self.data_display.insert(tk.END, message + "\n")
        self.data_display.see(tk.END)
        self.data_display.update()
    
    def clear_data(self):
        for param in self.data:
            self.data[param].clear()
        self.time_data.clear()
        self.display_message("All data cleared - ready for new data stream")
        
        for param in self.current_values:
            self.current_values[param].set("--")
        
        # Clear all graphs
        for ax, param in zip(self.axes, ['flow', 'pressure', 'temperature_1', 'temperature_2', 
                                       'temperature_3', 'temperature_4', 'temperature_5']):
            ax.clear()
            self.configure_axes(ax, param)
        
        for canvas in self.canvases:
            canvas.draw()
    
    def on_closing(self):
        self.disconnect_serial()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = IndustrialMonitor(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()

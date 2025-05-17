from flask import Flask, request, jsonify, render_template
from event import EventManager

app = Flask(__name__, 
            template_folder="src",
            static_folder="src")
event_manager = EventManager()

# Create events
button_clicked = event_manager.create_event("button_clicked")
data_received = event_manager.create_event("data_received")

# Define event handlers
def on_button_click(data):
    print(f"Button clicked! Data: {data}")

def on_data_received(data):
    print(f"Data received: {data}")

# Subscribe to events
button_clicked += on_button_click
data_received += on_data_received

# Start event processing
event_manager.start_processing()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/trigger_button', methods=['POST'])
def trigger_button():
    data = request.json
    event_manager.trigger_event("button_clicked", data)
    return jsonify({"status": "success"})

if __name__ == '__main__':
    app.run(debug=True, port=4000)

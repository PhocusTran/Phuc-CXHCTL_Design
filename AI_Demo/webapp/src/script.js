function triggerEvent(msg) {
    fetch('/trigger_button', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            timestamp: new Date().toISOString(),
            message: msg,
        })
    })
    .then(response => response.json())
    .then(data => {
         
         console.log(`Event triggered successfully at ${new Date().toLocaleTimeString()}`);
    })
    .catch(error => {
        console.error('Error:', error);
    });
}


const socket = io('http://localhost:5000');

socket.on('connect', () => {
    console.log('Connected to WebSocket server');
});

// Listen for the 'image_data' event from the server
socket.on('image_data', function(data) {
    const imageElement = document.getElementById(data.id);  // Get the img element by ID
    const imageSrc = 'data:image/jpeg;base64,' + data.image;  // Convert base64 string to image source
    imageElement.src = imageSrc;  // Set the src attribute to display the image
});

// always reload
window.addEventListener('load', function() {
    if (performance.navigation.type === 2) {  // Page loaded via back/forward
      window.location.reload(true);  // true forces a complete reload
    }
});
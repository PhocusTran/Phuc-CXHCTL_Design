# Use a base image with Python pre-installed
FROM python:3.11-slim

# Set the working directory
WORKDIR /app

# Install system dependencies, including OpenGL
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libglib2.0-0 \
    curl && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy project files
COPY . /app

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Install Node.js and npm for TailwindCSS
RUN curl -fsSL https://deb.nodesource.com/setup_18.x | bash - && \
    apt-get install -y nodejs && \
    npm install && \
    npx tailwindcss build src/input.css -o statics/output.css

# Expose the Flask port (default is 5000)
EXPOSE 5000

# Start the Flask application
CMD ["python", "demo.py"]


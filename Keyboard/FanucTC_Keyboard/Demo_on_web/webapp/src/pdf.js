console.log("PDF.js is ready");
const pdfjsLib = window['pdfjs-dist/build/pdf'];
pdfjsLib.GlobalWorkerOptions.workerSrc = '/src/pdfjs/pdf.worker.min.js';

const canvas = document.getElementById('source-viewer');
const context = canvas.getContext('2d');

const fileInput = document.getElementById('file-input');

// Function to render the PDF
function renderPDF(arrayBuffer) {
    const typedArray = new Uint8Array(arrayBuffer);
    pdfjsLib.getDocument(typedArray).promise.then(pdf => {
        pdf.getPage(1).then(page => {
            const viewport = page.getViewport({ scale: 4 });
            canvas.width = viewport.width;
            canvas.height = viewport.height;


            const renderContext = {
                canvasContext: context,
                viewport: viewport
            };
            page.render(renderContext);

        });
    }).catch(error => {
        console.error("Error rendering PDF:", error);
        alert("Unable to render the selected PDF.");
    });
}


document.getElementById('load_btn').addEventListener('click', function(event) {
    event.preventDefault();
    fileInput.click();
    triggerEvent("load_pdf");
});

document.addEventListener('keydown', function(event) {
    // Check if Ctrl + O is pressed
    if (event.ctrlKey && event.key === 'o') {
        // Prevent the default action (open file dialog)
        event.preventDefault();
        fileInput.click();
    }
});

// Handle file selection
fileInput.addEventListener('change', function(event) {
    const file = event.target.files[0];

    if (!file || file.type !== 'application/pdf') {
        alert('Please select a valid PDF file.');
        return;
    }

    // Render PDF in the browser
    const fileReader = new FileReader();
    fileReader.onload = async function () {
        const typedArray = new Uint8Array(this.result);
        const pdfDoc = await pdfjsLib.getDocument(typedArray).promise;

        const canvasIds = ['source-viewer', 'highlight-viewer']; // Array of canvas IDs to render into

        for (let pageNum = 1; pageNum <= Math.min(pdfDoc.numPages, canvasIds.length); pageNum++) {
            const page = await pdfDoc.getPage(pageNum);
            const viewport = page.getViewport({ scale: 1 });

            const canvas = document.getElementById(canvasIds[pageNum - 1]);
            if (!canvas) continue;

            canvas.width = viewport.width;
            canvas.height = viewport.height;

            const context = canvas.getContext('2d');
            const renderContext = {
                canvasContext: context,
                viewport: viewport,
            };
            await page.render(renderContext).promise;
        }
    };
    fileReader.readAsArrayBuffer(file);

    // Upload the file to the server
    uploadFile(file);
});

// Upload file function
async function uploadFile(file) {
    const formData = new FormData();
    formData.append('pdf', file);

    try {
        const response = await fetch('/upload', {
            method: 'POST',
            body: formData,
        });

        if (!response.ok) {
            throw new Error('File upload failed.');
        }
    } catch (error) {
        console.error('Error uploading file:', error);
        alert('Error uploading file.');
    }
}

socket.on('processbar/enable', function(data) {
    console.log(data)
    processbar = document.getElementById('process_bar');
    processbar.style.display = "block";
});

socket.on('processbar/value', function(data) {
    console.log(data)
    processline = document.getElementById('process_line');
    processline.style.width = `${data * 10}%`
});
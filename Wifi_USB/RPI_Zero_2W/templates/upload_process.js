var files = [];
document.addEventListener('DOMContentLoaded', function() {
    console.log("Here")
        // Prevent the actual submit from happening
        const uploadButton = document.getElementById('uploadButton')
        const fileElem = document.getElementById("fileElem") //New
        const uploadStatus = document.getElementById('uploadStatus');
        const uploadList = document.getElementById('fileList')//New
        const form = document.querySelector('.upload-form');
        const progressContainer = document.getElementById('progressContainer')
        const progressBar = document.getElementById('progressBar')
        console.log(uploadList)
        form.addEventListener('submit', function(event) {
            // Stop the form from submitting since we're handling the data ourselves
            event.preventDefault();
            uploadAllFiles()
        });

        dropArea.addEventListener('click', function() {
            fileElem.click();
        });
        ;
        ['dragenter', 'dragover'].forEach(eventName => {
            dropArea.addEventListener(eventName, highlight)
        })

        ;
        ['dragleave', 'drop'].forEach(eventName => {
            dropArea.addEventListener(eventName, unhighlight)
        })

        function highlight(e) {
            dropArea.classList.add('highlight')
        }

        function unhighlight(e) {
            dropArea.classList.remove('highlight')
        }

        function handleDrop(e) {
            const dt = e.dataTransfer
            const fileList = dt.files

            handleFiles(fileList)
        }
        ;
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            dropArea.addEventListener(eventName, preventDefaults)
        })

        function preventDefaults(e) {
            e.preventDefault()
            e.stopPropagation()
        }

        function handleFiles(fileList) {
            //If there is a old file, we make the upload bar disapper
            progressContainer.style.display = 'block'; // Show the progress bar container
            for (let i = 0; i < fileList.length; i++) {
                const file = fileList[i];
                files.push(file) //Add files to be upload at once

                let list = document.createElement('li');
                uploadList.appendChild(list).textContent = file.name
            }

            console.log(uploadList.value)

            //This is so the page does not run with zero, and does not submit it
            if (uploadList.lenght >= 0) {
                uploadButton.style.display = 'block'
            }
        }

    function uploadAllFiles() { //Add it here, inside document event listener
        const uploadStatus = document.getElementById('uploadStatus');
        let filesUploaded = 0;
        const totalFiles = files.length;

        files.forEach(function(file) {
            uploadFile(file, function() {
                filesUploaded++;
                uploadStatus.textContent = `Uploaded ${filesUploaded} of ${totalFiles} files.`;
                if (filesUploaded === totalFiles) {
                    uploadStatus.textContent = `All ${totalFiles} files uploaded successfully!`;
                }
            });
        });
    }

    function uploadFile(file, callback) {
        const uploadStatus = document.getElementById('uploadStatus');

        const xhr = new XMLHttpRequest();
        xhr.open('POST', form.action, true);

        xhr.onload = function() {
            if (xhr.status === 200) {
                console.log(`File "${file.name}" uploaded successfully`);
                if (callback) callback();
            } else {
                uploadStatus.textContent = `Error uploading "${file.name}": ${xhr.statusText}`;
            }
        };

        xhr.onerror = function() {
            uploadStatus.textContent = `Error uploading "${file.name}": Network error`;
        };

        xhr.upload.onprogress = function(event) {
            if (event.lengthComputable) {
                const percentComplete = (event.loaded / event.total) * 100;
                progressBar.style.width = percentComplete + '%';
                progressBar.textContent = `${file.name}: ${percentComplete.toFixed(2)}%`;
            }
        };

        const formData = new FormData();
        formData.append('file', file, file.name);
        xhr.send(formData);
    }
});
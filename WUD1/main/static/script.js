
const contextMenu = document.getElementById("context-menu");
const deleteForm = document.getElementById("delete-form");

function showContextMenu(e, item) {
    e.preventDefault();
    const deletePath = item.dataset.path;
    deleteForm.action = deletePath;

    // Position the menu
    contextMenu.style.left = `${e.pageX}px`;
    contextMenu.style.top = `${e.pageY}px`;
    contextMenu.classList.remove("hidden");

    // Hide on outside click
    document.addEventListener("click", hideContextMenu);
}

function hideContextMenu(e) {
    if (!contextMenu.contains(e.target)) {
        contextMenu.classList.add("hidden");
        document.removeEventListener("click", hideContextMenu);
    }
}

function setpath() {
    const file = document.getElementById("newfile").files[0];
    if (file) {
        document.getElementById("filepath").value = file.name;
    }
}

function upload() {
    const fileInput = document.getElementById("newfile");
    const filePath = document.getElementById("filepath").value;
    const uploadPath = "/upload/" + filePath;
    const file = fileInput.files[0];

    if (!file) {
        alert("No file selected!");
    } else if (!filePath) {
        alert("File path on server is not set!");
    } else if (filePath.endsWith('/')) {
        alert("File name not specified after path!");
    } else {
        fileInput.disabled = true;
        document.getElementById("filepath").disabled = true;
        document.getElementById("upload_btn").innerText = "Uploading";
        document.getElementById("upload_btn").disabled = true;

        const xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (xhttp.readyState === 4) {
                if (xhttp.status === 200) {
                    document.open();
                    document.write(xhttp.responseText);
                    document.close();
                } else if (xhttp.status === 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload();
                } else {
                    alert(`${xhttp.status} Error!\n${xhttp.responseText}`);
                    location.reload();
                }
            }
        };
        xhttp.open("POST", uploadPath, true);
        xhttp.send(file);
    }
}

const contextMenu = document.getElementById("context-menu");
const deleteForm = document.getElementById("delete-form");

function showContextMenu(e, item) {
    e.preventDefault();
    const deletePath = item.dataset.path;
    deleteForm.action = deletePath;

    contextMenu.style.left = `${e.pageX}px`;
    contextMenu.style.top = `${e.pageY}px`;
    contextMenu.classList.remove("hidden");

    document.addEventListener("click", hideContextMenu);
}

function hideContextMenu(e) {
    if (!contextMenu.contains(e.target)) {
        contextMenu.classList.add("hidden");
        document.removeEventListener("click", hideContextMenu);
    }
}

function setpath() {
    const files = document.getElementById("newfile").files;
    if (files.length > 0) {
        document.getElementById("filepath").value = "/";
    }
}

function upload() {
    const fileInput = document.getElementById("newfile");
    const pathPrefix = document.getElementById("filepath").value;
    const files = fileInput.files;

    if (files.length === 0) {
        alert("No file selected!");
        return;
    }

    let prefix = pathPrefix.trim();
    if (prefix && !prefix.endsWith("/")) {
        prefix += "/";
    }

    fileInput.disabled = true;
    document.getElementById("filepath").disabled = true;
    document.getElementById("upload_btn").innerText = "Uploading...";
    document.getElementById("upload_btn").disabled = true;

    let index = 0;

    function uploadNext() {
        if (index >= files.length) {
            // ✅ Remount silently, then reload
            fetch("/remount", { method: "POST" })
                .then(() => location.reload())
                .catch(() => location.reload());
            return;
        }

        const file = files[index];
        const uploadPath = "/upload/" + prefix + file.name;

        const xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
            if (xhttp.readyState === 4) {
                if (xhttp.status === 200) {
                    index++;
                    uploadNext();
                } else {
                    alert(`${xhttp.status} Error on file ${file.name}\n${xhttp.responseText}`);
                    location.reload();
                }
            }
        };
        xhttp.open("POST", uploadPath, true);
        xhttp.send(file);
    }

    uploadNext();
}

function remount() {
    if (!confirm("Are you sure you want to remount the USB storage?")) return;

    const btn = document.getElementById("remount_btn");
    btn.disabled = true;
    btn.innerText = "Remounting...";

    fetch("/remount", { method: "POST" })
        .then(response => response.text())
        .then(text => {
            alert(text);
            btn.disabled = false;
            btn.innerText = "Remount USB";
        })
        .catch(err => {
            alert("Failed to remount.");
            btn.disabled = false;
            btn.innerText = "Remount USB";
        });
}

function deleteSelected() {
    const checkboxes = document.querySelectorAll(".file-checkbox:checked");
    if (checkboxes.length === 0) {
        alert("No files selected.");
        return;
    }

    if (!confirm("Are you sure you want to delete selected files?")) return;

    let deletedCount = 0;

    checkboxes.forEach((cb) => {
        const path = cb.dataset.path;
        const xhttp = new XMLHttpRequest();
        xhttp.open("POST", path, true);
        xhttp.onreadystatechange = function () {
            if (xhttp.readyState === 4) {
                deletedCount++;
                if (deletedCount === checkboxes.length) {
                    location.reload();
                }
            }
        };
        xhttp.send();
    });
}
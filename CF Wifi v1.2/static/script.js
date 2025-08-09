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

// // Rename functionality
// function renameFile() {
//     if (!currentItem) return;

//     const link = currentItem.querySelector("a");
//     const fullPath = decodeURIComponent(link.getAttribute("href"));  // quan trọng nếu tên file có dấu tiếng Việt!
//     const parts = fullPath.split('/');
//     const oldName = parts.pop(); // "1.pdf"
//     const folder = parts.join('/'); // "" nếu là root

//     const input = document.createElement("input");
//     input.value = oldName;
//     input.classList.add("border", "px-2", "py-1", "text-sm");

//     link.replaceWith(input);
//     input.focus();
//     input.setSelectionRange(0, input.value.length);

//     input.addEventListener("blur", () => {
//         input.replaceWith(link);
//     });

//     input.addEventListener("keydown", (e) => {
//         if (e.key === "Enter") {
//             const newName = input.value.trim();
//             if (newName && newName !== oldName) {
//                 const folderPath = fullPath.substring(0, fullPath.lastIndexOf('/') + 1); // lấy thư mục chứa file

//                 const oldPath = fullPath;  // vì href là đường dẫn đầy đủ rồi
//                 const newPath = folderPath + newName;

//                 const url = `/rename?old=${encodeURIComponent(oldPath)}&new=${encodeURIComponent(newPath)}`;
//                 console.log("Rename URL:", url);

//                 fetch(url)
//                     .then(resp => {
//                         if (resp.ok) location.reload();
//                         else resp.text().then(t => alert("Rename failed:\n" + t));
//                     })
//                     .catch(err => alert("Rename failed:\n" + err));
//             } else {
//                 input.replaceWith(link);
//             }
//         }
//     });
// }

// // Add Rename to context menu
// contextMenu.innerHTML += `<button type="button" onclick="renameFile()" class="w-full text-left px-4 py-2 hover:bg-sky-100">Rename</button>`;
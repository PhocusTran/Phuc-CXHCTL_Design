const contextMenu = document.getElementById("context-menu");
let currentItem = null; // Biến toàn cục để lưu trữ phần tử file-item hiện tại

// function showContextMenu(e, item) {
//     e.preventDefault(); // Ngăn context menu mặc định của trình duyệt
//     currentItem = item; // Lưu phần tử đang được chuột phải

//     // Xóa tất cả các mục menu cũ
//     contextMenu.innerHTML = ''; 

//     // Thêm nút Rename
//     const renameButton = document.createElement('button');
//     renameButton.type = 'button';
//     // Đảm bảo các class này khớp với Tailwind CSS của bạn
//     renameButton.classList.add('w-full', 'text-left', 'px-4', 'py-2', 'hover:bg-sky-100', 'text-gray-800'); // Thêm màu chữ mặc định
//     renameButton.textContent = 'Rename';
//     renameButton.onclick = renameFile; // Gán hàm xử lý rename
//     contextMenu.appendChild(renameButton);

//     // Vị trí menu
//     contextMenu.style.left = `${e.pageX}px`;
//     contextMenu.style.top = `${e.pageY}px`;
//     contextMenu.classList.remove("hidden"); // Hiển thị menu

//     // Đảm bảo sự kiện ẩn menu được gán đúng cách
//     document.addEventListener("click", hideContextMenu);
// }

// // *** ĐÂY LÀ HÀM hideContextMenu ĐÃ ĐƯỢC SỬA LỖI ĐỂ XỬ LÝ 'undefined' EVENT OBJECT ***
// function hideContextMenu(e) {
//     // Nếu có sự kiện e (tức là click từ người dùng)
//     if (e) {
//         // Nếu contextMenu vẫn đang hiển thị VÀ click không nằm trong contextMenu
//         if (!contextMenu.classList.contains("hidden") && !contextMenu.contains(e.target)) {
//             contextMenu.classList.add("hidden");
//             document.removeEventListener("click", hideContextMenu);
//         }
//     } else {
//         // Nếu không có sự kiện e (tức là được gọi nội bộ từ JS, ví dụ từ renameFile)
//         // Thì chỉ cần ẩn contextMenu
//         contextMenu.classList.add("hidden");
//         document.removeEventListener("click", hideContextMenu); // Vẫn cần xóa listener nếu đã thêm
//     }
// }

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
    const selectedCheckboxes = document.querySelectorAll('.file-checkbox:checked');
    const filesToDelete = Array.from(selectedCheckboxes).map(cb => cb.value);

    if (filesToDelete.length === 0) {
        alert("Please select at least one file to delete.");
        return;
    }

    if (!confirm(`Are you sure you want to delete ${filesToDelete.length} selected file(s)/folder(s)?`)) {
        return;
    }

    const form = document.createElement('form');
    form.method = 'POST';
    form.action = '/delete'; 

    const hiddenInput = document.createElement('input');
    hiddenInput.type = 'hidden';
    hiddenInput.name = 'files'; 
    hiddenInput.value = JSON.stringify(filesToDelete); 
    form.appendChild(hiddenInput);

    document.body.appendChild(form); 
    form.submit(); 
}

// Hàm này không còn dùng trực tiếp trong deleteSelected, nhưng giữ lại nếu có chỗ khác gọi
function prepareDeleteForm(form) {
    var checkboxes = document.querySelectorAll('input[type="checkbox"]:checked');
    var files = [];
    for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].value) {
            files.push(checkboxes[i].value);
        }
    }

    if (files.length === 0) {
        alert("Please select at least one file to delete.");
        return false;
    }

    if (!confirm("Are you sure you want to delete selected files?")) {
        return false;
    }
    return true;
}

// Rename functionality
// function renameFile() {
//     if (!currentItem) return; // Đảm bảo có phần tử được chọn

//     hideContextMenu(); // Ẩn context menu

//     const linkElement = currentItem.querySelector("a");
//     if (!linkElement) {
//         console.error("Link element not found within currentItem.");
//         return;
//     }

//     // Lấy đường dẫn tương đối (ví dụ: "folder/file.txt" hoặc "file.txt") từ data-path hoặc data-href
//     // Chúng ta sẽ dùng data-path để đảm bảo lấy đúng tên file/folder gốc
//     let oldRelativePath = currentItem.dataset.path; // Ví dụ: "/delete/MC.nc" hoặc "/delete/folder/"
//     if (oldRelativePath.startsWith('/delete/')) {
//         oldRelativePath = oldRelativePath.substring('/delete/'.length); // Bỏ "/delete/"
//     }
//     // Loại bỏ dấu '/' ở cuối nếu là thư mục
//     if (oldRelativePath.endsWith('/')) {
//         oldRelativePath = oldRelativePath.substring(0, oldRelativePath.length - 1);
//     }

//     const oldName = oldRelativePath.split('/').pop(); // Lấy tên file/folder cuối cùng (MC.nc)

//     // Tạo input field để đổi tên
//     const inputField = document.createElement("input");
//     inputField.value = oldName;
//     inputField.classList.add("border", "px-2", "py-1", "text-sm", "rounded", "focus:ring-2", "focus:ring-sky-300", "focus:outline-none"); // Thêm style
//     inputField.style.width = `${linkElement.offsetWidth + 20}px`; // Đặt chiều rộng tương ứng với tên cũ

//     // Thay thế thẻ <a> bằng input
//     linkElement.replaceWith(inputField);

//     inputField.focus();
//     inputField.select(); // Chọn toàn bộ văn bản để dễ dàng đổi tên

//     // Sử dụng biến cờ để tránh gọi hàm 2 lần
//     let renameInProgress = false;

//     const performRename = () => {
//         if (renameInProgress) return; // Tránh gọi lần thứ hai
//         renameInProgress = true;

//         const newName = inputField.value.trim();

//         if (!newName || newName === oldName) {
//             // Nếu tên mới rỗng hoặc không thay đổi, khôi phục lại thẻ <a>
//             inputField.replaceWith(linkElement);
//             renameInProgress = false;
//             return;
//         }

//         // Xây dựng đường dẫn mới
//         let newRelativePath;
//         const lastSlashIndex = oldRelativePath.lastIndexOf('/');
//         if (lastSlashIndex !== -1) {
//             // Nếu có thư mục cha
//             const folderPath = oldRelativePath.substring(0, lastSlashIndex + 1); // "subfolder/"
//             newRelativePath = folderPath + newName;
//         } else {
//             // Nếu ở thư mục gốc
//             newRelativePath = newName;
//         }

//         // Kiểm tra log để debug
//         console.log("DEBUG: oldRelativePath (sent to server):", oldRelativePath);
//         console.log("DEBUG: newRelativePath (sent to server):", newRelativePath);
        
//         // EncodeURIComponent để xử lý khoảng trắng và ký tự đặc biệt
//         const url = `/rename?old=${encodeURIComponent(oldRelativePath)}&new=${encodeURIComponent(newRelativePath)}`;
        
//         console.log("Rename URL:", url);

//         fetch(url, { method: "GET" }) // Server handler là GET
//             .then(response => {
//                 // Kiểm tra status code trước
//                 if (response.ok) { // response.ok là true cho status 200-299
//                     return response.text().then(text => {
//                         console.log("Rename Response:", text); // Log phản hồi từ server
//                         // Kiểm tra thông báo thành công cụ thể từ server
//                         if (text.includes("Rename successful")) { // CHỈNH SỬA TẠI ĐÂY
//                             // Nếu thành công, kích hoạt remount và reload trang
//                             fetch("/remount", { method: "POST" }) // Kích hoạt remount
//                                 .then(() => location.reload()) // Sau đó reload trang
//                                 .catch((err) => {
//                                     alert("Rename successful, but failed to remount or reload:\n" + err);
//                                     location.reload(); // Vẫn cố gắng reload để cập nhật UI
//                                 });
//                         } else {
//                             // Trường hợp status ok nhưng nội dung không đúng (ít xảy ra nếu server code đúng)
//                             alert("Rename failed: Unexpected response from server.");
//                             inputField.replaceWith(linkElement); // Khôi phục nếu thất bại
//                         }
//                     });
//                 } else {
//                     // Nếu status code là lỗi (ví dụ 500), đọc text để lấy thông báo lỗi từ server
//                     return response.text().then(text => {
//                         alert("Rename failed:\n" + text); // Hiển thị lỗi từ server
//                         inputField.replaceWith(linkElement); // Khôi phục nếu thất bại
//                     });
//                 }
//             })
//             .catch(err => {
//                 alert("Rename failed: Network error or unexpected problem.\n" + err);
//                 inputField.replaceWith(linkElement); // Khôi phục nếu thất bại
//             })
//             .finally(() => {
//                 renameInProgress = false; // Luôn đặt lại cờ
//             });
//     };

//     // Remove old listeners to prevent multiple calls
//     inputField.removeEventListener("blur", performRename);
//     inputField.removeEventListener("keydown", performRename); // This might need a specific handler to remove correctly

//     // Re-add new listeners
//     inputField.addEventListener("blur", performRename); // Khi click ra ngoài
//     inputField.addEventListener("keydown", (e) => {
//         if (e.key === "Enter") {
//             e.preventDefault(); // Ngăn hành động mặc định của Enter (như submit form)
//             performRename();
//             // Không gọi inputField.blur() trực tiếp ở đây sau performRename() khi nhấn Enter.
//             // performRename() sẽ xử lý logic và reload trang.
//             // Nếu performRename() không reload, thì blur sẽ được kích hoạt tự nhiên khi người dùng click ra ngoài.
//         }
//     });
// }
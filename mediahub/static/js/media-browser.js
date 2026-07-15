// Inline media-library tree browser for the card assignment form.
// Mirrors the ESPuino web UI's own SD file explorer (lazy-loading tree,
// folders before files, click-to-select) but hand-written in vanilla JS
// to match this project's no-framework/offline-first frontend.
(function () {
	"use strict";

	function initMediaBrowser(root) {
		var listUrl = root.dataset.listUrl;
		var labels = JSON.parse(document.getElementById("media-tree-labels").textContent);
		// Read from the JSON <script> block, not an HTML attribute: a raw
		// tojson'd path containing a `"` would otherwise terminate the
		// value="..." attribute early and truncate/corrupt it.
		var selection = (labels.initialSelection || []).slice();
		var singleFileModes = (labels.singleFileModes || []).map(String);
		var playModeSelect = document.getElementById("play_mode");

		var selectedListEl = document.getElementById("selected-files");
		var selectedEmptyEl = document.getElementById("selected-files-empty");
		var hiddenInput = document.getElementById("selected-paths-json");

		function isSingleFileMode() {
			return !!playModeSelect && singleFileModes.indexOf(playModeSelect.value) !== -1;
		}

		function updateUseFolderButtons() {
			var disabled = isSingleFileMode();
			root.querySelectorAll(".mt-use-btn").forEach(function (btn) {
				btn.disabled = disabled;
				btn.title = disabled ? labels.singleFileOnly : "";
			});
		}

		function saveSelection() {
			hiddenInput.value = JSON.stringify(selection);
			renderSelectedList();
			syncCheckboxes();
		}

		function renderSelectedList() {
			selectedListEl.innerHTML = "";
			selectedEmptyEl.hidden = selection.length > 0;
			selection.forEach(function (path) {
				var li = document.createElement("li");
				var nameSpan = document.createElement("span");
				nameSpan.textContent = path;
				var removeBtn = document.createElement("button");
				removeBtn.type = "button";
				removeBtn.className = "mt-remove";
				removeBtn.setAttribute("aria-label", labels.remove);
				removeBtn.textContent = "×";
				removeBtn.addEventListener("click", function () {
					var idx = selection.indexOf(path);
					if (idx !== -1) {
						selection.splice(idx, 1);
						saveSelection();
					}
				});
				li.appendChild(nameSpan);
				li.appendChild(removeBtn);
				selectedListEl.appendChild(li);
			});
		}

		function syncCheckboxes() {
			root.querySelectorAll(".mt-file").forEach(function (li) {
				var checkbox = li.querySelector(".mt-checkbox");
				checkbox.checked = selection.indexOf(li.dataset.path) !== -1;
			});
		}

		function addPaths(paths) {
			if (isSingleFileMode()) {
				// Acts like a radio group in this mode: picking a new file replaces
				// whatever was selected before instead of adding to it.
				selection = paths.slice(-1);
				saveSelection();
				return;
			}
			paths.forEach(function (path) {
				if (selection.indexOf(path) === -1) {
					selection.push(path);
				}
			});
			saveSelection();
		}

		function removePath(path) {
			var idx = selection.indexOf(path);
			if (idx !== -1) {
				selection.splice(idx, 1);
				saveSelection();
			}
		}

		function joinPath(base, name) {
			return base ? base + "/" + name : name;
		}

		function fetchListing(path) {
			return fetch(listUrl + "?path=" + encodeURIComponent(path)).then(function (resp) {
				if (!resp.ok) {
					throw new Error("listing failed");
				}
				return resp.json();
			});
		}

		function buildDirNode(path, name) {
			var li = document.createElement("li");
			li.className = "mt-dir";
			li.dataset.path = path;
			li.dataset.loaded = "false";

			var row = document.createElement("div");
			row.className = "mt-row";

			var toggle = document.createElement("span");
			toggle.className = "mt-toggle";
			toggle.textContent = "▸";

			var icon = document.createElement("span");
			icon.className = "mt-icon";
			icon.textContent = "📁";

			var nameSpan = document.createElement("span");
			nameSpan.className = "mt-name";
			nameSpan.textContent = name;

			var useBtn = document.createElement("button");
			useBtn.type = "button";
			useBtn.className = "mt-use-btn";
			useBtn.textContent = labels.useFolder;
			useBtn.disabled = isSingleFileMode();
			useBtn.title = useBtn.disabled ? labels.singleFileOnly : "";

			var children = document.createElement("ul");
			children.className = "mt-children";
			children.hidden = true;

			row.appendChild(toggle);
			row.appendChild(icon);
			row.appendChild(nameSpan);
			row.appendChild(useBtn);
			li.appendChild(row);
			li.appendChild(children);

			function expand() {
				if (li.dataset.loaded === "true") {
					children.hidden = !children.hidden;
					toggle.textContent = children.hidden ? "▸" : "▾";
					return;
				}
				toggle.textContent = "…";
				fetchListing(path).then(function (data) {
					renderChildren(children, data);
					li.dataset.loaded = "true";
					children.hidden = false;
					toggle.textContent = "▾";
				}).catch(function () {
					toggle.textContent = "▸";
					children.hidden = false;
					children.innerHTML = "";
					var error = document.createElement("li");
					error.className = "mt-empty mt-error";
					error.textContent = labels.listError;
					children.appendChild(error);
				});
			}

			toggle.addEventListener("click", expand);
			nameSpan.addEventListener("click", expand);

			useBtn.addEventListener("click", function () {
				var originalLabel = useBtn.textContent;
				useBtn.disabled = true;
				fetchListing(path).then(function (data) {
					addPaths(data.files.map(function (name) {
						return joinPath(path, name);
					}));
				}).catch(function () {
					window.alert(labels.listError);
				}).finally(function () {
					useBtn.disabled = false;
					useBtn.textContent = originalLabel;
				});
			});

			return li;
		}

		function buildFileNode(path, name) {
			var li = document.createElement("li");
			li.className = "mt-file";
			li.dataset.path = path;

			var checkbox = document.createElement("input");
			checkbox.type = "checkbox";
			checkbox.className = "mt-checkbox";
			checkbox.checked = selection.indexOf(path) !== -1;
			checkbox.addEventListener("change", function () {
				if (checkbox.checked) {
					addPaths([path]);
				} else {
					removePath(path);
				}
			});

			var icon = document.createElement("span");
			icon.className = "mt-icon";
			icon.textContent = "🎵";

			var nameSpan = document.createElement("span");
			nameSpan.className = "mt-name";
			nameSpan.textContent = name;

			li.appendChild(checkbox);
			li.appendChild(icon);
			li.appendChild(nameSpan);
			return li;
		}

		function renderChildren(container, data) {
			container.innerHTML = "";
			data.dirs.forEach(function (name) {
				container.appendChild(buildDirNode(joinPath(data.path, name), name));
			});
			data.files.forEach(function (name) {
				container.appendChild(buildFileNode(joinPath(data.path, name), name));
			});
			if (!data.dirs.length && !data.files.length) {
				var empty = document.createElement("li");
				empty.className = "mt-empty muted";
				empty.textContent = labels.emptyFolder;
				container.appendChild(empty);
			}
		}

		var tree = document.createElement("ul");
		tree.className = "mt-root";
		root.appendChild(tree);

		fetchListing("").then(function (data) {
			renderChildren(tree, data);
			updateUseFolderButtons();
		}).catch(function () {
			var error = document.createElement("li");
			error.className = "mt-empty mt-error";
			error.textContent = labels.listError;
			tree.appendChild(error);
		});

		if (playModeSelect) {
			playModeSelect.addEventListener("change", function () {
				if (isSingleFileMode() && selection.length > 1) {
					selection = selection.slice(0, 1);
					saveSelection();
				}
				updateUseFolderButtons();
			});
		}

		if (isSingleFileMode() && selection.length > 1) {
			selection = selection.slice(0, 1);
		}

		// Sync the hidden field with the pre-filled selection right away —
		// otherwise clicking Save without touching the tree would submit the
		// "[]" placeholder and wipe an existing card's files.
		saveSelection();
	}

	document.addEventListener("DOMContentLoaded", function () {
		var root = document.getElementById("media-tree");
		if (root) {
			initMediaBrowser(root);
		}
	});
})();

// Opens a native <dialog> to pick a different, already-known ESPuino and
// copy the current card's configuration (name/kind/play mode/files or
// stream) over as an independent assignment for that device.
(function () {
	"use strict";

	document.addEventListener("DOMContentLoaded", function () {
		var dataEl = document.getElementById("duplicate-devices-data");
		var dialog = document.getElementById("duplicate-dialog");
		if (!dataEl || !dialog) {
			return;
		}

		var devices = JSON.parse(dataEl.textContent);
		var form = document.getElementById("duplicate-form");
		var select = document.getElementById("duplicate-target");
		var desc = document.getElementById("duplicate-dialog-desc");

		document.querySelectorAll(".mh-duplicate-btn").forEach(function (btn) {
			btn.addEventListener("click", function () {
				var sourceEspId = btn.dataset.espId;

				select.innerHTML = "";
				Object.keys(devices).forEach(function (espId) {
					if (espId === sourceEspId) {
						return;
					}
					var option = document.createElement("option");
					option.value = espId;
					option.textContent = devices[espId].alias || espId;
					select.appendChild(option);
				});

				if (!select.options.length) {
					window.alert(btn.dataset.noOtherDevice);
					return;
				}

				form.action = btn.dataset.url;
				desc.textContent = btn.dataset.desc;
				dialog.showModal();
			});
		});
	});
})();
